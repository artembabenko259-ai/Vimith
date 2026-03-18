#include <catch2/catch_test_macros.hpp>
#include "vimith/core/piece_table.hpp"

using namespace vimith::core;

// ── Construction ─────────────────────────────────────────────────────────────

TEST_CASE("PieceTable – empty construction", "[piece_table]") {
    PieceTable pt;
    CHECK(pt.getText() == "");
    CHECK(pt.length() == 0);
    CHECK(pt.lineCount() == 0);
    CHECK_FALSE(pt.isDirty());
}

TEST_CASE("PieceTable – construction with initial text", "[piece_table]") {
    PieceTable pt{"hello\nworld"};
    CHECK(pt.getText() == "hello\nworld");
    CHECK(pt.length() == 11);
    CHECK(pt.lineCount() == 2);
    CHECK_FALSE(pt.isDirty());
}

// ── Insert ────────────────────────────────────────────────────────────────────

TEST_CASE("PieceTable – insert into empty", "[piece_table]") {
    PieceTable pt;
    pt.insert(0, "abc");
    CHECK(pt.getText() == "abc");
    CHECK(pt.isDirty());
}

TEST_CASE("PieceTable – insert at beginning", "[piece_table]") {
    PieceTable pt{"world"};
    pt.insert(0, "hello ");
    CHECK(pt.getText() == "hello world");
}

TEST_CASE("PieceTable – insert at end", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.insert(5, " world");
    CHECK(pt.getText() == "hello world");
}

TEST_CASE("PieceTable – insert in middle", "[piece_table]") {
    PieceTable pt{"helo"};
    pt.insert(3, "l");
    CHECK(pt.getText() == "hello");
}

TEST_CASE("PieceTable – multiple inserts", "[piece_table]") {
    PieceTable pt;
    pt.insert(0, "a");
    pt.insert(1, "b");
    pt.insert(2, "c");
    CHECK(pt.getText() == "abc");
}

TEST_CASE("PieceTable – insert preserves newlines", "[piece_table]") {
    PieceTable pt{"line1\nline3"};
    pt.insert(6, "line2\n");
    CHECK(pt.getText() == "line1\nline2\nline3");
    CHECK(pt.lineCount() == 3);
}

TEST_CASE("PieceTable – insert empty string is no-op", "[piece_table]") {
    PieceTable pt{"abc"};
    pt.insert(1, "");
    CHECK(pt.getText() == "abc");
    CHECK_FALSE(pt.isDirty());
}

// ── Erase ─────────────────────────────────────────────────────────────────────

TEST_CASE("PieceTable – erase single char", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.erase(2, 1);
    CHECK(pt.getText() == "helo");
}

TEST_CASE("PieceTable – erase from beginning", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.erase(0, 2);
    CHECK(pt.getText() == "llo");
}

TEST_CASE("PieceTable – erase to end", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.erase(3, 2);
    CHECK(pt.getText() == "hel");
}

TEST_CASE("PieceTable – erase all", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.erase(0, 5);
    CHECK(pt.getText() == "");
    CHECK(pt.length() == 0);
}

TEST_CASE("PieceTable – erase spanning pieces", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.insert(5, " world");   // creates an Add piece
    pt.erase(4, 3);           // erases "o w" across the piece boundary
    CHECK(pt.getText() == "hellorld");
}

TEST_CASE("PieceTable – erase zero length is no-op", "[piece_table]") {
    PieceTable pt{"abc"};
    pt.erase(1, 0);
    CHECK(pt.getText() == "abc");
    CHECK_FALSE(pt.isDirty());
}

// ── getLine ───────────────────────────────────────────────────────────────────

TEST_CASE("PieceTable – getLine single line", "[piece_table]") {
    PieceTable pt{"hello"};
    CHECK(pt.getLine(0) == "hello");
    CHECK(pt.getLine(1) == "");
}

TEST_CASE("PieceTable – getLine multiple lines", "[piece_table]") {
    PieceTable pt{"alpha\nbeta\ngamma"};
    CHECK(pt.getLine(0) == "alpha");
    CHECK(pt.getLine(1) == "beta");
    CHECK(pt.getLine(2) == "gamma");
    CHECK(pt.getLine(3) == "");
}

TEST_CASE("PieceTable – getLine after insert creates newline", "[piece_table]") {
    PieceTable pt{"ab"};
    pt.insert(1, "\n");
    CHECK(pt.getLine(0) == "a");
    CHECK(pt.getLine(1) == "b");
    CHECK(pt.lineCount() == 2);
}

// ── Undo / Redo ───────────────────────────────────────────────────────────────

TEST_CASE("PieceTable – undo single insert", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.insert(5, " world");
    REQUIRE(pt.getText() == "hello world");

    pt.undo();
    CHECK(pt.getText() == "hello");
}

TEST_CASE("PieceTable – redo after undo", "[piece_table]") {
    PieceTable pt{"abc"};
    pt.insert(3, "d");
    pt.undo();
    REQUIRE(pt.getText() == "abc");

    pt.redo();
    CHECK(pt.getText() == "abcd");
}

TEST_CASE("PieceTable – undo erase", "[piece_table]") {
    PieceTable pt{"hello"};
    pt.erase(0, 2);
    REQUIRE(pt.getText() == "llo");

    pt.undo();
    CHECK(pt.getText() == "hello");
}

TEST_CASE("PieceTable – undo chain", "[piece_table]") {
    PieceTable pt;
    pt.insert(0, "a");
    pt.insert(1, "b");
    pt.insert(2, "c");
    REQUIRE(pt.getText() == "abc");

    pt.undo(); CHECK(pt.getText() == "ab");
    pt.undo(); CHECK(pt.getText() == "a");
    pt.undo(); CHECK(pt.getText() == "");
    pt.undo(); CHECK(pt.getText() == ""); // no-op when stack empty
}

TEST_CASE("PieceTable – redo stack cleared on new edit", "[piece_table]") {
    PieceTable pt{"abc"};
    pt.insert(3, "d");
    pt.undo();
    REQUIRE(pt.canRedo());

    pt.insert(0, "X");          // new edit clears redo stack
    CHECK_FALSE(pt.canRedo());
    CHECK(pt.getText() == "Xabc");
}

// ── markClean / isDirty ───────────────────────────────────────────────────────

TEST_CASE("PieceTable – markClean clears dirty flag", "[piece_table]") {
    PieceTable pt{"abc"};
    pt.insert(3, "d");
    REQUIRE(pt.isDirty());

    pt.markClean();
    CHECK_FALSE(pt.isDirty());
}

// ── lineCount ─────────────────────────────────────────────────────────────────

TEST_CASE("PieceTable – lineCount with trailing newline", "[piece_table]") {
    PieceTable pt{"a\nb\n"};
    CHECK(pt.lineCount() == 3); // "a", "b", ""
}

TEST_CASE("PieceTable – lineCount empty document", "[piece_table]") {
    PieceTable pt;
    CHECK(pt.lineCount() == 0);
}
