#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace vimith::syntax {

// ---------------------------------------------------------------------------
// TokenKind – semantic category of a syntax token
// ---------------------------------------------------------------------------
enum class TokenKind : uint8_t {
    Default,    // plain text
    Keyword,
    Type,
    Number,
    String,
    StringEscape,
    Char,
    Comment,
    Preprocessor,
    Operator,
    Punctuation,
    Identifier,
    Function,
    Variable,
    Constant,
    Attribute,  // Rust attributes, Python decorators
    Macro,
    Namespace,
};

// ---------------------------------------------------------------------------
// Token – a coloured span within a single line of text
// ---------------------------------------------------------------------------
struct Token {
    std::size_t start;   // byte offset from line start
    std::size_t length;
    TokenKind   kind;
};

// ---------------------------------------------------------------------------
// HighlightedLine – the tokenised representation of one source line
// ---------------------------------------------------------------------------
struct HighlightedLine {
    std::string          text;    // original line text (without newline)
    std::vector<Token>   tokens;  // sorted, non-overlapping
};

// ---------------------------------------------------------------------------
// IHighlighter – interface that each language plugin must implement
// ---------------------------------------------------------------------------
class IHighlighter {
public:
    virtual ~IHighlighter() = default;

    // Return the canonical language identifier, e.g. "cpp", "rust", "python"
    [[nodiscard]] virtual std::string_view language() const noexcept = 0;

    // Tokenise a single line of source text.
    // `lineText` does NOT include the trailing newline.
    // Implementations should fill `out.tokens` with sorted, non-overlapping
    // spans and copy `lineText` into `out.text`.
    virtual void tokenize(std::string_view      lineText,
                          HighlightedLine&       out) const = 0;

    // Convenience: tokenise and return by value
    [[nodiscard]] HighlightedLine tokenizeLine(std::string_view text) const {
        HighlightedLine out;
        tokenize(text, out);
        return out;
    }
};

} // namespace vimith::syntax
