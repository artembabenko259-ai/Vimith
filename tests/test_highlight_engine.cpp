#include <catch2/catch_test_macros.hpp>
#include "vimith/syntax/highlight_engine.hpp"
#include "vimith/syntax/cpp_highlighter.hpp"
#include "vimith/syntax/rust_highlighter.hpp"
#include "vimith/syntax/python_highlighter.hpp"

using namespace vimith::syntax;

// ── Language detection ────────────────────────────────────────────────────────

TEST_CASE("HighlightEngine – detectLanguage from extension", "[highlight]") {
    CHECK(HighlightEngine::detectLanguage("main.cpp")    == "cpp");
    CHECK(HighlightEngine::detectLanguage("foo.cxx")     == "cpp");
    CHECK(HighlightEngine::detectLanguage("bar.h")       == "cpp");
    CHECK(HighlightEngine::detectLanguage("bar.hpp")     == "cpp");
    CHECK(HighlightEngine::detectLanguage("lib.rs")      == "rust");
    CHECK(HighlightEngine::detectLanguage("script.py")   == "python");
    CHECK(HighlightEngine::detectLanguage("config.toml") == "");
    CHECK(HighlightEngine::detectLanguage("noext")       == "");
}

TEST_CASE("HighlightEngine – case-insensitive extension", "[highlight]") {
    CHECK(HighlightEngine::detectLanguage("FILE.CPP") == "cpp");
    CHECK(HighlightEngine::detectLanguage("main.RS")  == "rust");
    CHECK(HighlightEngine::detectLanguage("app.PY")   == "python");
}

// ── Registry ─────────────────────────────────────────────────────────────────

TEST_CASE("HighlightEngine – auto-registers built-in highlighters", "[highlight]") {
    HighlightEngine engine;
    CHECK(engine.getHighlighter("cpp")    != nullptr);
    CHECK(engine.getHighlighter("rust")   != nullptr);
    CHECK(engine.getHighlighter("python") != nullptr);
    CHECK(engine.getHighlighter("java")   == nullptr);
}

TEST_CASE("HighlightEngine – custom highlighter registration", "[highlight]") {
    class DummyHL : public IHighlighter {
    public:
        std::string_view language() const noexcept override { return "dummy"; }
        void tokenize(std::string_view t, HighlightedLine& out) const override {
            out.text = std::string{t};
        }
    };

    HighlightEngine engine;
    engine.registerHighlighter(std::make_unique<DummyHL>());
    CHECK(engine.getHighlighter("dummy") != nullptr);
}

// ── C++ tokenizer ─────────────────────────────────────────────────────────────

TEST_CASE("CppHighlighter – keyword highlighting", "[highlight][cpp]") {
    CppHighlighter hl;

    auto result = hl.tokenizeLine("int main() {");
    REQUIRE_FALSE(result.tokens.empty());

    // 'int' should be a keyword
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) {
            return result.text.substr(t.start, t.length) == "int";
        });
    REQUIRE(it != result.tokens.end());
    CHECK(it->kind == TokenKind::Keyword);
}

TEST_CASE("CppHighlighter – line comment", "[highlight][cpp]") {
    CppHighlighter hl;
    auto result = hl.tokenizeLine("// this is a comment");
    REQUIRE_FALSE(result.tokens.empty());
    CHECK(result.tokens[0].kind == TokenKind::Comment);
    CHECK(result.tokens[0].start  == 0);
    CHECK(result.tokens[0].length == result.text.size());
}

TEST_CASE("CppHighlighter – string literal", "[highlight][cpp]") {
    CppHighlighter hl;
    auto result = hl.tokenizeLine("const char* s = \"hello world\";");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) { return t.kind == TokenKind::String; });
    REQUIRE(it != result.tokens.end());
    CHECK(result.text.substr(it->start, it->length) == "\"hello world\"");
}

TEST_CASE("CppHighlighter – number literal", "[highlight][cpp]") {
    CppHighlighter hl;
    auto result = hl.tokenizeLine("int x = 42;");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) { return t.kind == TokenKind::Number; });
    REQUIRE(it != result.tokens.end());
    CHECK(result.text.substr(it->start, it->length) == "42");
}

TEST_CASE("CppHighlighter – preprocessor directive", "[highlight][cpp]") {
    CppHighlighter hl;
    auto result = hl.tokenizeLine("#include <vector>");
    REQUIRE_FALSE(result.tokens.empty());
    CHECK(result.tokens[0].kind == TokenKind::Preprocessor);
}

TEST_CASE("CppHighlighter – empty line", "[highlight][cpp]") {
    CppHighlighter hl;
    auto result = hl.tokenizeLine("");
    CHECK(result.text.empty());
    CHECK(result.tokens.empty());
}

// ── Rust tokenizer ────────────────────────────────────────────────────────────

TEST_CASE("RustHighlighter – keyword fn", "[highlight][rust]") {
    RustHighlighter hl;
    auto result = hl.tokenizeLine("fn main() {");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) {
            return result.text.substr(t.start, t.length) == "fn"
                && t.kind == TokenKind::Keyword;
        });
    CHECK(it != result.tokens.end());
}

TEST_CASE("RustHighlighter – macro call", "[highlight][rust]") {
    RustHighlighter hl;
    auto result = hl.tokenizeLine("    println!(\"hello\");");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) { return t.kind == TokenKind::Macro; });
    REQUIRE(it != result.tokens.end());
    CHECK(result.text.substr(it->start, it->length) == "println!");
}

TEST_CASE("RustHighlighter – attribute", "[highlight][rust]") {
    RustHighlighter hl;
    auto result = hl.tokenizeLine("#[derive(Debug)]");
    REQUIRE_FALSE(result.tokens.empty());
    CHECK(result.tokens[0].kind == TokenKind::Attribute);
}

TEST_CASE("RustHighlighter – line comment", "[highlight][rust]") {
    RustHighlighter hl;
    auto result = hl.tokenizeLine("// Rust comment");
    REQUIRE_FALSE(result.tokens.empty());
    CHECK(result.tokens[0].kind == TokenKind::Comment);
}

// ── Python tokenizer ──────────────────────────────────────────────────────────

TEST_CASE("PythonHighlighter – keyword def", "[highlight][python]") {
    PythonHighlighter hl;
    auto result = hl.tokenizeLine("def hello():");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) {
            return result.text.substr(t.start, t.length) == "def"
                && t.kind == TokenKind::Keyword;
        });
    CHECK(it != result.tokens.end());
}

TEST_CASE("PythonHighlighter – comment", "[highlight][python]") {
    PythonHighlighter hl;
    auto result = hl.tokenizeLine("# Python comment");
    REQUIRE_FALSE(result.tokens.empty());
    CHECK(result.tokens[0].kind == TokenKind::Comment);
    CHECK(result.tokens[0].start == 0);
}

TEST_CASE("PythonHighlighter – string", "[highlight][python]") {
    PythonHighlighter hl;
    auto result = hl.tokenizeLine("x = \"hello\"");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) { return t.kind == TokenKind::String; });
    REQUIRE(it != result.tokens.end());
    CHECK(result.text.substr(it->start, it->length) == "\"hello\"");
}

TEST_CASE("PythonHighlighter – decorator", "[highlight][python]") {
    PythonHighlighter hl;
    auto result = hl.tokenizeLine("@staticmethod");
    REQUIRE_FALSE(result.tokens.empty());
    CHECK(result.tokens[0].kind == TokenKind::Attribute);
}

TEST_CASE("PythonHighlighter – number", "[highlight][python]") {
    PythonHighlighter hl;
    auto result = hl.tokenizeLine("x = 3.14");
    const auto it = std::find_if(result.tokens.begin(), result.tokens.end(),
        [&](const Token& t) { return t.kind == TokenKind::Number; });
    REQUIRE(it != result.tokens.end());
}
