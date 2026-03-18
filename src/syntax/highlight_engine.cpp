#include "vimith/syntax/highlight_engine.hpp"
#include "vimith/syntax/cpp_highlighter.hpp"
#include "vimith/syntax/rust_highlighter.hpp"
#include "vimith/syntax/python_highlighter.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace vimith::syntax {

// ── HighlightEngine ──────────────────────────────────────────────────────────

HighlightEngine::HighlightEngine() {
    registerHighlighter(std::make_unique<CppHighlighter>());
    registerHighlighter(std::make_unique<RustHighlighter>());
    registerHighlighter(std::make_unique<PythonHighlighter>());
}

void HighlightEngine::registerHighlighter(std::unique_ptr<IHighlighter> impl) {
    if (!impl) return;
    const std::string lang{impl->language()};
    m_registry[lang] = std::move(impl);
}

const IHighlighter* HighlightEngine::getHighlighter(std::string_view lang) const noexcept {
    const auto it = m_registry.find(std::string{lang});
    return it != m_registry.end() ? it->second.get() : nullptr;
}

std::string HighlightEngine::detectLanguage(std::string_view filePath) noexcept {
    const auto dotPos = filePath.rfind('.');
    if (dotPos == std::string_view::npos) return {};

    const std::string ext{filePath.substr(dotPos + 1)};
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "cpp" || lower == "cxx" || lower == "cc"  ||
        lower == "c"   || lower == "h"   || lower == "hpp" ||
        lower == "hxx" || lower == "inl") return "cpp";

    if (lower == "rs") return "rust";

    if (lower == "py" || lower == "pyw" || lower == "pyi") return "python";

    return {};
}

HighlightedLine HighlightEngine::highlight(std::string_view lineText,
                                            std::string_view filePath) const {
    const std::string lang = detectLanguage(filePath);
    if (!lang.empty()) {
        const auto* hl = getHighlighter(lang);
        if (hl) return hl->tokenizeLine(lineText);
    }
    HighlightedLine out;
    out.text = std::string{lineText};
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Shared tokenizer utilities
// ═══════════════════════════════════════════════════════════════════════════

namespace detail {

// Append a token only if it has non-zero length
inline void addToken(std::vector<Token>& tokens,
                     std::size_t start, std::size_t end, TokenKind kind) {
    if (end > start) tokens.push_back({start, end - start, kind});
}

// Scan a C-style string literal starting at `pos` (which points to the
// opening quote character `q`). Handles \" / \' escape sequences.
// Returns the index AFTER the closing quote (or line.size() if unterminated).
inline std::size_t scanString(std::string_view line, std::size_t pos, char q) {
    ++pos; // skip opening quote
    while (pos < line.size()) {
        if (line[pos] == '\\') { pos += 2; continue; }
        if (line[pos] == q)    { return pos + 1; }
        ++pos;
    }
    return pos;
}

// Scan a decimal / hex / binary / float number.
inline std::size_t scanNumber(std::string_view line, std::size_t pos) {
    if (pos + 1 < line.size() && line[pos] == '0' &&
        (line[pos + 1] == 'x' || line[pos + 1] == 'X')) {
        pos += 2;
        while (pos < line.size() && std::isxdigit(static_cast<unsigned char>(line[pos])))
            ++pos;
        return pos;
    }
    while (pos < line.size() &&
           (std::isdigit(static_cast<unsigned char>(line[pos])) ||
            line[pos] == '.' || line[pos] == '_' || line[pos] == 'e' ||
            line[pos] == 'E' || line[pos] == 'f' || line[pos] == 'F' ||
            line[pos] == 'u' || line[pos] == 'U' || line[pos] == 'l' ||
            line[pos] == 'L'))
        ++pos;
    return pos;
}

// Scan an identifier (starts with letter or _)
inline std::size_t scanIdent(std::string_view line, std::size_t pos) {
    while (pos < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[pos])) || line[pos] == '_'))
        ++pos;
    return pos;
}

} // namespace detail

// ═══════════════════════════════════════════════════════════════════════════
// CppHighlighter
// ═══════════════════════════════════════════════════════════════════════════

static constexpr const char* kCppKeywords[] = {
    "alignas","alignof","and","and_eq","asm","auto",
    "bitand","bitor","bool","break",
    "case","catch","char","char8_t","char16_t","char32_t","class","co_await",
    "co_return","co_yield","compl","concept","const","consteval","constexpr",
    "constinit","const_cast","continue",
    "decltype","default","delete","do","double","dynamic_cast",
    "else","enum","explicit","export","extern",
    "false","float","for","friend",
    "goto",
    "if","inline","int",
    "long",
    "mutable",
    "namespace","new","noexcept","not","not_eq","nullptr",
    "operator","or","or_eq",
    "private","protected","public",
    "register","reinterpret_cast","requires","return",
    "short","signed","sizeof","static","static_assert","static_cast","struct",
    "switch",
    "template","this","thread_local","throw","true","try","typedef","typeid",
    "typename",
    "union","unsigned","using",
    "virtual","void","volatile",
    "wchar_t","while",
    "xor","xor_eq",
    // Common stdlib types / macros highlighted as keywords
    "override","final","nullptr_t","size_t","ptrdiff_t",
    "int8_t","int16_t","int32_t","int64_t",
    "uint8_t","uint16_t","uint32_t","uint64_t",
};

static bool isCppKeyword(std::string_view word) {
    for (const char* kw : kCppKeywords) {
        if (word == kw) return true;
    }
    return false;
}

void CppHighlighter::tokenize(std::string_view lineText,
                               HighlightedLine& out) const {
    out.text   = std::string{lineText};
    out.tokens.clear();
    using detail::addToken;

    const std::size_t n = lineText.size();
    std::size_t i = 0;

    // Skip leading whitespace
    while (i < n && std::isspace(static_cast<unsigned char>(lineText[i]))) ++i;

    // Preprocessor line
    if (i < n && lineText[i] == '#') {
        addToken(out.tokens, 0, n, TokenKind::Preprocessor);
        return;
    }

    while (i < n) {
        const char c = lineText[i];

        // Line comment
        if (c == '/' && i + 1 < n && lineText[i + 1] == '/') {
            addToken(out.tokens, i, n, TokenKind::Comment);
            break;
        }

        // Block comment start (single-line portion)
        if (c == '/' && i + 1 < n && lineText[i + 1] == '*') {
            const std::size_t end = lineText.find("*/", i + 2);
            const std::size_t commentEnd = (end != std::string_view::npos) ? end + 2 : n;
            addToken(out.tokens, i, commentEnd, TokenKind::Comment);
            i = commentEnd;
            continue;
        }

        // String literal
        if (c == '"') {
            const std::size_t end = detail::scanString(lineText, i, '"');
            addToken(out.tokens, i, end, TokenKind::String);
            i = end;
            continue;
        }

        // Char literal
        if (c == '\'') {
            const std::size_t end = detail::scanString(lineText, i, '\'');
            addToken(out.tokens, i, end, TokenKind::Char);
            i = end;
            continue;
        }

        // Number
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(lineText[i + 1])))) {
            const std::size_t end = detail::scanNumber(lineText, i);
            addToken(out.tokens, i, end, TokenKind::Number);
            i = end;
            continue;
        }

        // Identifier / keyword
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const std::size_t end = detail::scanIdent(lineText, i);
            const std::string_view word = lineText.substr(i, end - i);
            const TokenKind kind = isCppKeyword(word) ? TokenKind::Keyword
                                                      : TokenKind::Identifier;
            addToken(out.tokens, i, end, kind);
            i = end;
            continue;
        }

        // Operators / punctuation
        if (std::ispunct(static_cast<unsigned char>(c))) {
            addToken(out.tokens, i, i + 1, TokenKind::Operator);
        }

        ++i;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// RustHighlighter
// ═══════════════════════════════════════════════════════════════════════════

static constexpr const char* kRustKeywords[] = {
    "as","async","await","break","const","continue","crate","dyn",
    "else","enum","extern","false","fn","for","if","impl","in",
    "let","loop","match","mod","move","mut","pub","ref","return",
    "self","Self","static","struct","super","trait","true","type",
    "unsafe","use","where","while",
    // Primitive types
    "bool","char","f32","f64",
    "i8","i16","i32","i64","i128","isize",
    "u8","u16","u32","u64","u128","usize","str",
    // Common macros
    "println","print","eprintln","eprint","panic","assert","vec","format",
    "todo","unimplemented","unreachable",
};

static bool isRustKeyword(std::string_view word) {
    for (const char* kw : kRustKeywords) {
        if (word == kw) return true;
    }
    return false;
}

void RustHighlighter::tokenize(std::string_view lineText,
                                HighlightedLine& out) const {
    out.text   = std::string{lineText};
    out.tokens.clear();
    using detail::addToken;

    const std::size_t n = lineText.size();
    std::size_t i = 0;

    // Skip leading whitespace to check for attribute
    std::size_t ws = 0;
    while (ws < n && std::isspace(static_cast<unsigned char>(lineText[ws]))) ++ws;
    if (ws < n && lineText[ws] == '#') {
        addToken(out.tokens, ws, n, TokenKind::Attribute);
        return;
    }

    while (i < n) {
        const char c = lineText[i];

        // Line comment  //  or  ///
        if (c == '/' && i + 1 < n && lineText[i + 1] == '/') {
            addToken(out.tokens, i, n, TokenKind::Comment);
            break;
        }

        // String literal  "..."  or  r#"..."#
        if (c == '"') {
            const std::size_t end = detail::scanString(lineText, i, '"');
            addToken(out.tokens, i, end, TokenKind::String);
            i = end;
            continue;
        }

        // Char literal  '...'
        if (c == '\'') {
            const std::size_t end = detail::scanString(lineText, i, '\'');
            addToken(out.tokens, i, end, TokenKind::Char);
            i = end;
            continue;
        }

        // Number (Rust-style with underscores, 0x, 0b, 0o prefixes)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            const std::size_t end = detail::scanNumber(lineText, i);
            addToken(out.tokens, i, end, TokenKind::Number);
            i = end;
            continue;
        }

        // Identifier / keyword / macro call (ends with !)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const std::size_t end = detail::scanIdent(lineText, i);
            const std::string_view word = lineText.substr(i, end - i);

            // Check for macro: identifier followed by '!'
            if (end < n && lineText[end] == '!') {
                addToken(out.tokens, i, end + 1, TokenKind::Macro);
                i = end + 1;
                continue;
            }

            const TokenKind kind = isRustKeyword(word) ? TokenKind::Keyword
                                                       : TokenKind::Identifier;
            addToken(out.tokens, i, end, kind);
            i = end;
            continue;
        }

        if (std::ispunct(static_cast<unsigned char>(c))) {
            addToken(out.tokens, i, i + 1, TokenKind::Operator);
        }

        ++i;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PythonHighlighter
// ═══════════════════════════════════════════════════════════════════════════

static constexpr const char* kPyKeywords[] = {
    "False","None","True",
    "and","as","assert","async","await",
    "break","class","continue","def","del","elif","else","except",
    "finally","for","from","global","if","import","in","is",
    "lambda","nonlocal","not","or","pass","raise","return",
    "try","while","with","yield",
    // Builtins often highlighted as keywords
    "print","len","range","enumerate","zip","map","filter","sorted",
    "list","dict","set","tuple","str","int","float","bool","bytes",
    "type","isinstance","issubclass","super","object","property",
    "staticmethod","classmethod","self","cls",
};

static bool isPyKeyword(std::string_view word) {
    for (const char* kw : kPyKeywords) {
        if (word == kw) return true;
    }
    return false;
}

void PythonHighlighter::tokenize(std::string_view lineText,
                                  HighlightedLine& out) const {
    out.text   = std::string{lineText};
    out.tokens.clear();
    using detail::addToken;

    const std::size_t n = lineText.size();
    std::size_t i = 0;

    // Skip whitespace to check for decorator
    std::size_t ws = 0;
    while (ws < n && std::isspace(static_cast<unsigned char>(lineText[ws]))) ++ws;
    if (ws < n && lineText[ws] == '@') {
        addToken(out.tokens, ws, n, TokenKind::Attribute);
        // Still parse the rest for any embedded tokens? For now just mark entire line.
        return;
    }

    while (i < n) {
        const char c = lineText[i];

        // Line comment
        if (c == '#') {
            addToken(out.tokens, i, n, TokenKind::Comment);
            break;
        }

        // Triple-quoted strings (single-line portion only)
        if ((c == '"' || c == '\'') && i + 2 < n &&
            lineText[i + 1] == c && lineText[i + 2] == c) {
            const std::size_t close = lineText.find(
                std::string_view{&c, 1} == "\"" ? "\"\"\"" : "'''", i + 3);
            const std::size_t end = (close != std::string_view::npos) ? close + 3 : n;
            addToken(out.tokens, i, end, TokenKind::String);
            i = end;
            continue;
        }

        // Single-quoted string
        if (c == '"' || c == '\'') {
            const std::size_t end = detail::scanString(lineText, i, c);
            addToken(out.tokens, i, end, TokenKind::String);
            i = end;
            continue;
        }

        // Number
        if (std::isdigit(static_cast<unsigned char>(c))) {
            const std::size_t end = detail::scanNumber(lineText, i);
            addToken(out.tokens, i, end, TokenKind::Number);
            i = end;
            continue;
        }

        // Identifier / keyword
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const std::size_t end = detail::scanIdent(lineText, i);
            const std::string_view word = lineText.substr(i, end - i);
            const TokenKind kind = isPyKeyword(word) ? TokenKind::Keyword
                                                     : TokenKind::Identifier;
            addToken(out.tokens, i, end, kind);
            i = end;
            continue;
        }

        if (std::ispunct(static_cast<unsigned char>(c))) {
            addToken(out.tokens, i, i + 1, TokenKind::Operator);
        }

        ++i;
    }
}

} // namespace vimith::syntax
