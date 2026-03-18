#pragma once

#include "vimith/command/i_command.hpp"
#include "vimith/core/buffer_manager.hpp"
#include "vimith/editor_state.hpp"

#include <memory>
#include <string>

namespace vimith::input { class ModeManager; }

namespace vimith::command {

// ---------------------------------------------------------------------------
// CommandDispatcher
//
// Executes a Command (returned by ModeManager::processKey) against the
// EditorState and BufferManager.
//
// Each handler is a private method.  std::visit dispatches to the correct
// overload at runtime with zero virtual-call overhead.
// ---------------------------------------------------------------------------
class CommandDispatcher {
public:
    CommandDispatcher(EditorState&             state,
                      core::BufferManager&     buffer,
                      input::ModeManager&      modes);

    // Execute a command. Returns false if the editor should quit.
    bool dispatch(const Command& cmd);

private:
    EditorState&         m_state;
    core::BufferManager& m_buf;
    input::ModeManager&  m_modes;

    // Last change for '.' repeat
    Command m_lastChange{NoOp{}};

    // ── Cursor helpers ─────────────────────────────────────────────────────
    void clampCursor();
    void scrollToCursor();

    // Move cursor by (dLine, dCol); clamped to buffer bounds
    void moveCursorBy(int dLine, int dCol);
    void moveCursorTo(std::size_t line, std::size_t col);

    std::size_t currentLineLen() const;
    std::size_t lineLen(std::size_t line) const;

    // Word navigation helpers
    std::size_t nextWordStart(std::size_t offset, bool bigWord) const;
    std::size_t prevWordStart(std::size_t offset, bool bigWord) const;
    std::size_t nextWordEnd  (std::size_t offset, bool bigWord) const;

    // Convert cursor to flat char offset
    std::size_t cursorOffset() const;

    // ── Yank / delete helpers ─────────────────────────────────────────────
    void yankLines(std::size_t startLine, std::size_t count);
    void deleteLines(std::size_t startLine, std::size_t count);

    // ── Ex-command parser ─────────────────────────────────────────────────
    void executeExCommand(const std::string& line);

    // ── Option parser ─────────────────────────────────────────────────────
    void applySetOption(const std::string& name, const std::string& value);

    // ── Search ────────────────────────────────────────────────────────────
    bool findPattern(const std::string& pattern, bool forward,
                     std::size_t& outLine, std::size_t& outCol) const;

    // ── Handlers (one per Command alternative) ────────────────────────────
    void on(const NoOp&)               {}

    void on(const MoveLeft&            cmd);
    void on(const MoveRight&           cmd);
    void on(const MoveUp&              cmd);
    void on(const MoveDown&            cmd);
    void on(const MoveWordForward&     cmd);
    void on(const MoveWordBackward&    cmd);
    void on(const MoveWordEnd&         cmd);
    void on(const MoveLineStart&       cmd);
    void on(const MoveLineEnd&         cmd);
    void on(const MoveFirstNonBlank&   cmd);
    void on(const MoveFileStart&       cmd);
    void on(const MoveFileEnd&         cmd);
    void on(const MoveFindChar&        cmd);
    void on(const MoveMatchingBrace&   cmd);
    void on(const ScrollHalfPageUp&    cmd);
    void on(const ScrollHalfPageDown&  cmd);
    void on(const ScrollPageUp&        cmd);
    void on(const ScrollPageDown&      cmd);

    void on(const EnterInsert&         cmd);
    void on(const LeaveInsert&         cmd);
    void on(const EnterVisual&         cmd);
    void on(const EnterVLine&          cmd);
    void on(const EnterVBlock&         cmd);
    void on(const LeaveVisual&         cmd);
    void on(const EnterCommand&        cmd);
    void on(const EnterSearch&         cmd);

    void on(const InsertChar&          cmd);
    void on(const InsertText&          cmd);
    void on(const DeleteCharForward&   cmd);
    void on(const DeleteCharBackward&  cmd);
    void on(const DeleteLine&          cmd);
    void on(const DeleteToLineEnd&     cmd);
    void on(const ChangeToLineEnd&     cmd);
    void on(const ChangeLine&          cmd);
    void on(const YankLine&            cmd);
    void on(const YankToLineEnd&       cmd);
    void on(const DeleteMotion&        cmd);
    void on(const ChangeMotion&        cmd);
    void on(const YankMotion&          cmd);
    void on(const OpenLineBelow&       cmd);
    void on(const OpenLineAbove&       cmd);
    void on(const JoinLines&           cmd);
    void on(const IndentRight&         cmd);
    void on(const IndentLeft&          cmd);

    void on(const Paste&               cmd);
    void on(const PasteLine&           cmd);

    void on(const Undo&                cmd);
    void on(const Redo&                cmd);
    void on(const RepeatLast&          cmd);

    void on(const WriteFile&           cmd);
    void on(const QuitEditor&          cmd);
    void on(const WriteQuit&           cmd);
    void on(const OpenFile&            cmd);

    void on(const SearchPattern&       cmd);
    void on(const SearchNext&          cmd);

    void on(const SetOption&           cmd);
    void on(const SwitchToHexMode&     cmd);
    void on(const SwitchToTextMode&    cmd);
    void on(const ExecuteCommand&      cmd);

    void on(const VisualDelete&        cmd);
    void on(const VisualChange&        cmd);
    void on(const VisualYank&          cmd);
    void on(const VisualIndentRight&   cmd);
    void on(const VisualIndentLeft&    cmd);
};

} // namespace vimith::command
