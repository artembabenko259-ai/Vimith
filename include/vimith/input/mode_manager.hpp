#pragma once

#include "vimith/input/key_event.hpp"
#include "vimith/command/i_command.hpp"

#include <string>

namespace vimith::input {

// ---------------------------------------------------------------------------
// Mode – the editor's current interaction mode
// ---------------------------------------------------------------------------
enum class Mode {
    Normal,
    Insert,
    Visual,      // character-wise visual selection
    VisualLine,  // line-wise visual selection
    VisualBlock, // block (column) visual selection
    Command,     // : ex-command input
    Search,      // / or ? incremental search
};

[[nodiscard]] inline std::string_view modeName(Mode m) noexcept {
    switch (m) {
        case Mode::Normal:      return "NORMAL";
        case Mode::Insert:      return "INSERT";
        case Mode::Visual:      return "VISUAL";
        case Mode::VisualLine:  return "V-LINE";
        case Mode::VisualBlock: return "V-BLOCK";
        case Mode::Command:     return "COMMAND";
        case Mode::Search:      return "SEARCH";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// ModeManager
//
// Implements the Vim-style modal state machine.
//
// processKey() accepts a raw KeyEvent and returns a Command variant that the
// CommandDispatcher then executes against EditorState + BufferManager.
//
// Multi-key sequences (e.g. "gg", "dw", count prefixes) are accumulated
// internally.  When a sequence is complete, the corresponding Command is
// returned and the accumulator is reset.
//
// Only Normal / Insert / Visual / Command / Search modes are fully handled
// here; VisualLine and VisualBlock re-use the Visual path with a mode tag.
// ---------------------------------------------------------------------------
class ModeManager {
public:
    ModeManager() = default;

    // ── State ──────────────────────────────────────────────────────────────
    [[nodiscard]] Mode getMode()      const noexcept { return m_mode; }
    [[nodiscard]] int  getCount()     const noexcept { return m_count > 0 ? m_count : 1; }
    [[nodiscard]] bool hasPending()   const noexcept { return !m_pending.empty(); }
    [[nodiscard]] const std::string& commandLine() const noexcept { return m_cmdLine; }
    [[nodiscard]] bool searchForward() const noexcept { return m_searchForward; }

    void setMode(Mode m);

    // ── Core entry point ───────────────────────────────────────────────────
    // Process one KeyEvent and return the resulting Command.
    // Returns command::NoOp{} when the key is consumed but no action is
    // triggered yet (e.g. waiting for operator motion, building a count).
    [[nodiscard]] command::Command processKey(const KeyEvent& ev);

    // Called when the editor wants to force a mode change without a key
    // (e.g. after command execution).
    void enterNormal();
    void enterInsert();

private:
    Mode        m_mode   {Mode::Normal};
    int         m_count  {0};           // numeric prefix (0 = none)
    std::string m_pending;              // buffered keys for multi-key sequences
    std::string m_cmdLine;              // content of : or / line
    bool        m_searchForward{true};

    // Last change for '.' repeat
    command::Command m_lastChange{command::NoOp{}};
    bool             m_recordingChange{false};

    // ── Mode-specific handlers ─────────────────────────────────────────────
    [[nodiscard]] command::Command processNormal (const KeyEvent& ev);
    [[nodiscard]] command::Command processInsert (const KeyEvent& ev);
    [[nodiscard]] command::Command processVisual (const KeyEvent& ev);
    [[nodiscard]] command::Command processCommand(const KeyEvent& ev);
    [[nodiscard]] command::Command processSearch (const KeyEvent& ev);

    // Shared motion resolver – used by Normal and Visual modes
    [[nodiscard]] command::Command resolveMotion(char c, int count);

    // Shared operator+motion resolver (d, c, y + motion)
    [[nodiscard]] command::Command resolveOperatorMotion(char op,
                                                         char motion,
                                                         int  count);

    void resetAccumulator();
};

} // namespace vimith::input
