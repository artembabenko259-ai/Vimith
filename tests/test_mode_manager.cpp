#include <catch2/catch_test_macros.hpp>
#include "vimith/input/mode_manager.hpp"
#include "vimith/command/i_command.hpp"

using namespace vimith::input;
using namespace vimith::command;

// ── Helpers ──────────────────────────────────────────────────────────────────

static KeyEvent key(char c, bool ctrl = false) {
    return {keyFromChar(c), ctrl, false, false};
}

static KeyEvent special(Key k) {
    return {k, false, false, false};
}

// ── Normal mode – basic movement ─────────────────────────────────────────────

TEST_CASE("ModeManager – hjkl movement in Normal mode", "[mode_manager]") {
    ModeManager mm;
    REQUIRE(mm.getMode() == Mode::Normal);

    auto check = [&](char c, auto expected) {
        const auto cmd = mm.processKey(key(c));
        CHECK(std::holds_alternative<std::decay_t<decltype(expected)>>(cmd));
    };

    check('h', MoveLeft{});
    check('j', MoveDown{});
    check('k', MoveUp{});
    check('l', MoveRight{});
}

TEST_CASE("ModeManager – arrow keys in Normal mode", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(special(Key::ArrowLeft));
    CHECK(std::holds_alternative<MoveLeft>(cmd));
}

TEST_CASE("ModeManager – word motions", "[mode_manager]") {
    ModeManager mm;
    CHECK(std::holds_alternative<MoveWordForward> (mm.processKey(key('w'))));
    CHECK(std::holds_alternative<MoveWordBackward>(mm.processKey(key('b'))));
    CHECK(std::holds_alternative<MoveWordEnd>     (mm.processKey(key('e'))));
    CHECK(std::holds_alternative<MoveWordForward> (mm.processKey(key('W'))));
}

TEST_CASE("ModeManager – line start/end motions", "[mode_manager]") {
    ModeManager mm;
    CHECK(std::holds_alternative<MoveLineStart>(mm.processKey(key('0'))));
    CHECK(std::holds_alternative<MoveLineEnd>  (mm.processKey(key('$'))));
    CHECK(std::holds_alternative<MoveFirstNonBlank>(mm.processKey(key('^'))));
}

TEST_CASE("ModeManager – G goes to file end", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('G'));
    REQUIRE(std::holds_alternative<MoveFileEnd>(cmd));
    CHECK(!std::get<MoveFileEnd>(cmd).line.has_value());
}

// ── Count prefixes ────────────────────────────────────────────────────────────

TEST_CASE("ModeManager – count prefix applies to movement", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('3')); // count
    mm.processKey(key('j')); // motion

    // After 'j', the count should have been consumed and a MoveDown{3} returned
    // We need to re-test with the full pipeline:
    ModeManager mm2;
    mm2.processKey(key('5')); // accumulate
    auto cmd = mm2.processKey(key('h'));
    REQUIRE(std::holds_alternative<MoveLeft>(cmd));
    CHECK(std::get<MoveLeft>(cmd).count == 5);
}

TEST_CASE("ModeManager – multi-digit count", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('1'));
    mm.processKey(key('2'));
    auto cmd = mm.processKey(key('k'));
    REQUIRE(std::holds_alternative<MoveUp>(cmd));
    CHECK(std::get<MoveUp>(cmd).count == 12);
}

// ── Mode transitions ──────────────────────────────────────────────────────────

TEST_CASE("ModeManager – 'i' enters Insert mode", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('i'));
    CHECK(mm.getMode() == Mode::Insert);
    REQUIRE(std::holds_alternative<EnterInsert>(cmd));
    CHECK(std::get<EnterInsert>(cmd).where == InsertWhere::BeforeCursor);
}

TEST_CASE("ModeManager – 'a' enters Insert after cursor", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('a'));
    CHECK(mm.getMode() == Mode::Insert);
    REQUIRE(std::holds_alternative<EnterInsert>(cmd));
    CHECK(std::get<EnterInsert>(cmd).where == InsertWhere::AfterCursor);
}

TEST_CASE("ModeManager – 'I' enters Insert at line start", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('I'));
    CHECK(mm.getMode() == Mode::Insert);
    CHECK(std::get<EnterInsert>(cmd).where == InsertWhere::LineStart);
}

TEST_CASE("ModeManager – 'A' enters Insert at line end", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('A'));
    CHECK(mm.getMode() == Mode::Insert);
    CHECK(std::get<EnterInsert>(cmd).where == InsertWhere::LineEnd);
}

TEST_CASE("ModeManager – Escape in Insert returns to Normal", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('i'));
    REQUIRE(mm.getMode() == Mode::Insert);

    auto cmd = mm.processKey(special(Key::Escape));
    CHECK(mm.getMode() == Mode::Normal);
    CHECK(std::holds_alternative<LeaveInsert>(cmd));
}

TEST_CASE("ModeManager – 'v' enters Visual mode", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('v'));
    CHECK(mm.getMode() == Mode::Visual);
}

TEST_CASE("ModeManager – ':' enters Command mode", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key(':'));
    CHECK(mm.getMode() == Mode::Command);
}

TEST_CASE("ModeManager – '/' enters Search mode", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('/'));
    CHECK(mm.getMode() == Mode::Search);
    CHECK(mm.searchForward());
}

TEST_CASE("ModeManager – '?' enters backward Search mode", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('?'));
    CHECK(mm.getMode() == Mode::Search);
    CHECK_FALSE(mm.searchForward());
}

// ── Insert mode ───────────────────────────────────────────────────────────────

TEST_CASE("ModeManager – printable chars in Insert return InsertChar", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('i'));
    auto cmd = mm.processKey(key('x'));
    REQUIRE(std::holds_alternative<InsertChar>(cmd));
    CHECK(std::get<InsertChar>(cmd).c == 'x');
}

TEST_CASE("ModeManager – Backspace in Insert returns DeleteCharBackward", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('i'));
    auto cmd = mm.processKey(special(Key::Backspace));
    CHECK(std::holds_alternative<DeleteCharBackward>(cmd));
}

TEST_CASE("ModeManager – Enter in Insert inserts newline", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('i'));
    auto cmd = mm.processKey(special(Key::Enter));
    REQUIRE(std::holds_alternative<InsertChar>(cmd));
    CHECK(std::get<InsertChar>(cmd).c == '\n');
}

// ── Operator + motion (two-key sequences) ────────────────────────────────────

TEST_CASE("ModeManager – 'dd' yields DeleteLine", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('d'));
    CHECK(mm.hasPending());
    auto cmd = mm.processKey(key('d'));
    CHECK(std::holds_alternative<DeleteLine>(cmd));
}

TEST_CASE("ModeManager – 'dw' yields DeleteMotion", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('d'));
    auto cmd = mm.processKey(key('w'));
    REQUIRE(std::holds_alternative<DeleteMotion>(cmd));
    CHECK(std::get<DeleteMotion>(cmd).motion == 'w');
}

TEST_CASE("ModeManager – 'cc' yields ChangeLine", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('c'));
    auto cmd = mm.processKey(key('c'));
    CHECK(std::holds_alternative<ChangeLine>(cmd));
}

TEST_CASE("ModeManager – 'yy' yields YankLine", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('y'));
    auto cmd = mm.processKey(key('y'));
    CHECK(std::holds_alternative<YankLine>(cmd));
}

TEST_CASE("ModeManager – 'gg' yields MoveFileStart", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('g'));
    auto cmd = mm.processKey(key('g'));
    CHECK(std::holds_alternative<MoveFileStart>(cmd));
}

TEST_CASE("ModeManager – Escape cancels pending operator", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('d'));
    REQUIRE(mm.hasPending());
    mm.processKey(special(Key::Escape));
    CHECK_FALSE(mm.hasPending());
}

// ── f/t find-char ─────────────────────────────────────────────────────────────

TEST_CASE("ModeManager – 'fx' yields MoveFindChar forward", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('f'));
    auto cmd = mm.processKey(key('x'));
    REQUIRE(std::holds_alternative<MoveFindChar>(cmd));
    auto& fc = std::get<MoveFindChar>(cmd);
    CHECK(fc.c       == 'x');
    CHECK(fc.forward == true);
    CHECK(fc.before  == false);
}

TEST_CASE("ModeManager – 'tx' yields MoveFindChar before target", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('t'));
    auto cmd = mm.processKey(key('x'));
    REQUIRE(std::holds_alternative<MoveFindChar>(cmd));
    CHECK(std::get<MoveFindChar>(cmd).before == true);
}

TEST_CASE("ModeManager – 'Fx' yields MoveFindChar backward", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key('F'));
    auto cmd = mm.processKey(key('x'));
    REQUIRE(std::holds_alternative<MoveFindChar>(cmd));
    CHECK(std::get<MoveFindChar>(cmd).forward == false);
}

// ── Command line ──────────────────────────────────────────────────────────────

TEST_CASE("ModeManager – typing in Command mode builds command line", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key(':'));
    mm.processKey(key('w'));
    mm.processKey(key('q'));
    CHECK(mm.commandLine() == "wq");
}

TEST_CASE("ModeManager – Enter in Command mode yields ExecuteCommand", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key(':'));
    mm.processKey(key('q'));
    auto cmd = mm.processKey(special(Key::Enter));
    REQUIRE(std::holds_alternative<ExecuteCommand>(cmd));
    CHECK(std::get<ExecuteCommand>(cmd).line == "q");
    CHECK(mm.getMode() == Mode::Normal);
}

TEST_CASE("ModeManager – Backspace in Command mode removes last char", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key(':'));
    mm.processKey(key('w'));
    mm.processKey(key('q'));
    mm.processKey(special(Key::Backspace));
    CHECK(mm.commandLine() == "w");
}

TEST_CASE("ModeManager – Escape cancels Command mode", "[mode_manager]") {
    ModeManager mm;
    mm.processKey(key(':'));
    mm.processKey(key('w'));
    mm.processKey(special(Key::Escape));
    CHECK(mm.getMode() == Mode::Normal);
    CHECK(mm.commandLine().empty());
}

// ── Edit commands ─────────────────────────────────────────────────────────────

TEST_CASE("ModeManager – 'x' yields DeleteCharForward", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('x'));
    CHECK(std::holds_alternative<DeleteCharForward>(cmd));
}

TEST_CASE("ModeManager – 'u' yields Undo", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('u'));
    CHECK(std::holds_alternative<Undo>(cmd));
}

TEST_CASE("ModeManager – Ctrl+r yields Redo", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey({keyFromChar('r'), true, false, false});
    CHECK(std::holds_alternative<Redo>(cmd));
}

TEST_CASE("ModeManager – 'p' yields Paste after cursor", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('p'));
    REQUIRE(std::holds_alternative<Paste>(cmd));
    CHECK_FALSE(std::get<Paste>(cmd).before);
}

TEST_CASE("ModeManager – 'P' yields Paste before cursor", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('P'));
    REQUIRE(std::holds_alternative<Paste>(cmd));
    CHECK(std::get<Paste>(cmd).before);
}

TEST_CASE("ModeManager – '.' yields RepeatLast", "[mode_manager]") {
    ModeManager mm;
    auto cmd = mm.processKey(key('.'));
    CHECK(std::holds_alternative<RepeatLast>(cmd));
}
