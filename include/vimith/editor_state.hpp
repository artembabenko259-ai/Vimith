#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace vimith {

// ---------------------------------------------------------------------------
// CursorPos – zero-based (line, column) pair
// ---------------------------------------------------------------------------
struct CursorPos {
    std::size_t line = 0;
    std::size_t col  = 0;

    bool operator==(const CursorPos&) const = default;

    [[nodiscard]] bool isBefore(const CursorPos& other) const noexcept {
        return line < other.line || (line == other.line && col < other.col);
    }
};

// ---------------------------------------------------------------------------
// VisualSelection – active visual-mode selection range
// ---------------------------------------------------------------------------
struct VisualSelection {
    CursorPos anchor; // position where 'v' was pressed
    CursorPos cursor; // current cursor (may be before or after anchor)
};

// ---------------------------------------------------------------------------
// SearchState – current search pattern and match tracking
// ---------------------------------------------------------------------------
struct SearchState {
    std::string pattern;
    bool        forward  = true;
    bool        hasMatch = false;
    CursorPos   matchPos;
};

// ---------------------------------------------------------------------------
// EditorOptions – runtime-configurable options (:set ...)
// ---------------------------------------------------------------------------
struct EditorOptions {
    bool showLineNumbers         = true;
    bool showRelativeLineNumbers = false;
    int  tabSize                 = 4;
    bool expandTabs              = true;
    bool wrapLines               = false;
    bool hlSearch                = true;   // highlight search matches
    bool ignoreCase              = false;
    bool showDisasm              = true;   // live disassembly panel in :hex mode
};

// ---------------------------------------------------------------------------
// EditorState – single source of truth for the entire editor session.
//
// Passed by reference/pointer to CommandDispatcher and Renderer.
// All fields are modified exclusively on the main thread.
// ---------------------------------------------------------------------------
struct EditorState {
    // ── Cursor & viewport ─────────────────────────────────────────────────
    CursorPos   cursor;
    std::size_t topLine  = 0; // index of first visible line (vertical scroll)
    std::size_t leftCol  = 0; // index of first visible column (horizontal scroll)

    // ── Hex / disassembly cursor ─────────────────────────────────────────
    std::size_t hexOffset = 0; // absolute byte offset selected in :hex mode

    // ── Terminal dimensions ────────────────────────────────────────────────
    int termWidth  = 80;
    int termHeight = 24;

    // ── File metadata ─────────────────────────────────────────────────────
    std::string filename;

    // ── UI state ──────────────────────────────────────────────────────────
    std::string statusMessage;      // transient message shown in status bar
    std::string commandLineInput;   // current content of : / / ? input line

    // ── Clipboard (unnamed register) ──────────────────────────────────────
    std::string yankRegister;
    bool        yankLineMode = false; // true when yanked with 'yy'

    // ── Visual selection ──────────────────────────────────────────────────
    std::optional<VisualSelection> visualSel;

    // ── Search ────────────────────────────────────────────────────────────
    SearchState search;

    // ── Last f/t find (for ';' and ',') ───────────────────────────────────
    char lastFindChar    = '\0';
    bool lastFindForward = true;
    bool lastFindBefore  = false;

    // ── Options ───────────────────────────────────────────────────────────
    EditorOptions options;

    // ── App-level flags ───────────────────────────────────────────────────
    bool running = true;
};

} // namespace vimith
