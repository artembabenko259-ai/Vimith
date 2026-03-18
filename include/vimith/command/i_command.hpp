#pragma once

#include <optional>
#include <string>
#include <variant>

// Forward declaration to avoid circular includes
namespace vimith::input { enum class Mode; }

namespace vimith::command {

// ---------------------------------------------------------------------------
// All possible editor commands, used as the return type of
// ModeManager::processKey() and as the dispatch token for CommandDispatcher.
//
// Design rules:
//   - Each struct is a pure value type (no vtable, no heap allocation)
//   - The variant is the single dynamic dispatch point
//   - CommandDispatcher uses std::visit to handle each alternative
// ---------------------------------------------------------------------------

// ── No-op ──────────────────────────────────────────────────────────────────
struct NoOp {};

// ── Cursor movement ────────────────────────────────────────────────────────
struct MoveLeft        { int count = 1; };
struct MoveRight       { int count = 1; };
struct MoveUp          { int count = 1; };
struct MoveDown        { int count = 1; };

struct MoveWordForward  { int count = 1; bool bigWord = false; };
struct MoveWordBackward { int count = 1; bool bigWord = false; };
struct MoveWordEnd      { int count = 1; bool bigWord = false; };

struct MoveLineStart   {};   // 0
struct MoveLineEnd     {};   // $
struct MoveFirstNonBlank {}; // ^

struct MoveFileStart   {};           // gg
struct MoveFileEnd     { std::optional<int> line; }; // G / <N>G

struct MoveFindChar {
    char c        = '\0';
    bool forward  = true;   // f = forward, F = backward
    bool before   = false;  // t / T stops one before the char
    int  count    = 1;
};

struct MoveMatchingBrace {}; // %

struct ScrollHalfPageUp   {};  // Ctrl+u
struct ScrollHalfPageDown {};  // Ctrl+d
struct ScrollPageUp       {};  // Ctrl+b
struct ScrollPageDown     {};  // Ctrl+f

// ── Mode transitions ────────────────────────────────────────────────────────
enum class InsertWhere {
    BeforeCursor,  // i
    AfterCursor,   // a
    LineStart,     // I
    LineEnd,       // A
    NewLineBelow,  // o
    NewLineAbove,  // O
};
struct EnterInsert  { InsertWhere where = InsertWhere::BeforeCursor; };
struct LeaveInsert  {};

struct EnterVisual  {};
struct EnterVLine   {};
struct EnterVBlock  {};
struct LeaveVisual  {};

struct EnterCommand {};
struct EnterSearch  { bool forward = true; };

// ── Text editing ────────────────────────────────────────────────────────────
struct InsertChar  { char c; };         // printable character in Insert mode
struct InsertText  { std::string text; };
struct DeleteCharForward  { int count = 1; }; // x
struct DeleteCharBackward { int count = 1; }; // X / Backspace

struct DeleteLine  { int count = 1; };  // dd
struct DeleteToLineEnd {};              // D

struct ChangeToLineEnd {};              // C (delete to EOL + enter Insert)
struct ChangeLine   { int count = 1; }; // cc

struct YankLine     { int count = 1; }; // yy
struct YankToLineEnd {};                // Y

// Operator + motion (d{motion}, c{motion}, y{motion})
struct DeleteMotion  { char motion; int count = 1; bool bigWord = false; char findChar = '\0'; };
struct ChangeMotion  { char motion; int count = 1; bool bigWord = false; char findChar = '\0'; };
struct YankMotion    { char motion; int count = 1; bool bigWord = false; char findChar = '\0'; };

struct OpenLineBelow {};  // o
struct OpenLineAbove {};  // O

struct JoinLines     { int count = 1; };  // J

// Indentation
struct IndentRight   { int count = 1; };  // >>
struct IndentLeft    { int count = 1; };  // <<

// ── Clipboard ───────────────────────────────────────────────────────────────
struct Paste         { bool before = false; }; // p / P
struct PasteLine     { bool before = false; };

// ── History ─────────────────────────────────────────────────────────────────
struct Undo {};
struct Redo {};

// ── Repeat ─────────────────────────────────────────────────────────────────
struct RepeatLast {}; // .

// ── File I/O & application control ─────────────────────────────────────────
struct WriteFile     { std::string path; };          // :w
struct QuitEditor    { bool force = false; };         // :q / :q!
struct WriteQuit     {};                              // :wq
struct OpenFile      { std::string path; };           // :e <path>

// ── Search ──────────────────────────────────────────────────────────────────
struct SearchPattern { std::string pattern; bool forward = true; };
struct SearchNext    { bool forward = true; };   // n / N

// ── Options / settings ──────────────────────────────────────────────────────
struct SetOption     { std::string name; std::string value; };

// ── Buffer mode switch ───────────────────────────────────────────────────────
struct SwitchToHexMode  {};
struct SwitchToTextMode {};

// ── Execute accumulated ex-command line ─────────────────────────────────────
struct ExecuteCommand { std::string line; };

// ── Visual-mode operations ─────────────────────────────────────────────────
struct VisualDelete {};
struct VisualChange {};
struct VisualYank   {};
struct VisualIndentRight {};
struct VisualIndentLeft  {};

// ---------------------------------------------------------------------------
// The Command variant – all dispatcher logic is driven by std::visit on this.
// ---------------------------------------------------------------------------
using Command = std::variant<
    NoOp,

    // Movement
    MoveLeft, MoveRight, MoveUp, MoveDown,
    MoveWordForward, MoveWordBackward, MoveWordEnd,
    MoveLineStart, MoveLineEnd, MoveFirstNonBlank,
    MoveFileStart, MoveFileEnd,
    MoveFindChar, MoveMatchingBrace,
    ScrollHalfPageUp, ScrollHalfPageDown, ScrollPageUp, ScrollPageDown,

    // Mode transitions
    EnterInsert, LeaveInsert,
    EnterVisual, EnterVLine, EnterVBlock, LeaveVisual,
    EnterCommand, EnterSearch,

    // Editing
    InsertChar, InsertText,
    DeleteCharForward, DeleteCharBackward,
    DeleteLine, DeleteToLineEnd,
    ChangeToLineEnd, ChangeLine,
    YankLine, YankToLineEnd,
    DeleteMotion, ChangeMotion, YankMotion,
    OpenLineBelow, OpenLineAbove,
    JoinLines,
    IndentRight, IndentLeft,

    // Clipboard
    Paste, PasteLine,

    // History
    Undo, Redo,

    // Repeat
    RepeatLast,

    // File / app control
    WriteFile, QuitEditor, WriteQuit, OpenFile,

    // Search
    SearchPattern, SearchNext,

    // Settings
    SetOption,

    // Buffer mode
    SwitchToHexMode, SwitchToTextMode,

    // Ex-command dispatch
    ExecuteCommand,

    // Visual operators
    VisualDelete, VisualChange, VisualYank,
    VisualIndentRight, VisualIndentLeft
>;

} // namespace vimith::command
