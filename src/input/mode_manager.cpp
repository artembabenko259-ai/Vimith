#include "vimith/input/mode_manager.hpp"

#include <cctype>

namespace vimith::input {

using namespace vimith::command;

// ── Helpers ──────────────────────────────────────────────────────────────────

static bool isOperatorKey(char c) noexcept {
    return c == 'd' || c == 'c' || c == 'y';
}

static bool isMotionKey(char c) noexcept {
    return c == 'h' || c == 'j' || c == 'k' || c == 'l' ||
           c == 'w' || c == 'W' || c == 'b' || c == 'B' ||
           c == 'e' || c == 'E' || c == '0' || c == '$' ||
           c == '^' || c == 'G' || c == 'g' || c == '%' ||
           c == 'f' || c == 'F' || c == 't' || c == 'T';
}

// ── Public interface ─────────────────────────────────────────────────────────

void ModeManager::setMode(Mode m) {
    m_mode = m;
    resetAccumulator();
}

void ModeManager::enterNormal() { setMode(Mode::Normal); }
void ModeManager::enterInsert() { setMode(Mode::Insert); }

void ModeManager::resetAccumulator() {
    m_count   = 0;
    m_pending.clear();
}

Command ModeManager::processKey(const KeyEvent& ev) {
    switch (m_mode) {
        case Mode::Normal:      return processNormal(ev);
        case Mode::Insert:      return processInsert(ev);
        case Mode::Visual:
        case Mode::VisualLine:
        case Mode::VisualBlock: return processVisual(ev);
        case Mode::Command:     return processCommand(ev);
        case Mode::Search:      return processSearch(ev);
    }
    return NoOp{};
}

// ── Normal mode ──────────────────────────────────────────────────────────────

Command ModeManager::processNormal(const KeyEvent& ev) {
    const char c = isPrintable(ev.key) ? keyToChar(ev.key) : '\0';

    // ── Ctrl combos ──────────────────────────────────────────────────────
    if (ev.ctrl) {
        switch (c) {
            case 'r': resetAccumulator(); return Redo{};
            case 'u': resetAccumulator(); return ScrollHalfPageUp{};
            case 'd': resetAccumulator(); return ScrollHalfPageDown{};
            case 'b': resetAccumulator(); return ScrollPageUp{};
            case 'f': resetAccumulator(); return ScrollPageDown{};
            default: return NoOp{};
        }
    }

    // ── Numeric prefix accumulation ──────────────────────────────────────
    if (std::isdigit(static_cast<unsigned char>(c))) {
        const int digit = c - '0';
        if (digit != 0 || m_count > 0) {        // '0' alone is a motion key
            m_count = m_count * 10 + digit;
            return NoOp{};
        }
    }

    const int count = m_count > 0 ? m_count : 1;

    // ── Pending operator + motion ─────────────────────────────────────────
    if (!m_pending.empty()) {
        const char op = m_pending[0];

        if (m_pending.size() == 1) {
            // Waiting for motion (or second key of "gg", "dd", etc.)

            // Double-key shorthands
            if (c == op) {
                resetAccumulator();
                if (op == 'd') return DeleteLine{count};
                if (op == 'c') return ChangeLine{count};
                if (op == 'y') return YankLine{count};
                if (op == 'g') return MoveFileStart{};
                if (op == '>') return IndentRight{count};
                if (op == '<') return IndentLeft{count};
            }

            // f/F/t/T – need one more char
            if (op == 'f' || op == 'F' || op == 't' || op == 'T') {
                if (c != '\0') {
                    const bool fwd    = (op == 'f' || op == 't');
                    const bool before = (op == 't' || op == 'T');
                    resetAccumulator();
                    return MoveFindChar{c, fwd, before, count};
                }
                return NoOp{};
            }

            // Operator + motion
            if (isMotionKey(c) && (op == 'd' || op == 'c' || op == 'y')) {
                // f/F/t/T as second key of operator need a 3rd char – handle below
                if (c == 'f' || c == 'F' || c == 't' || c == 'T') {
                    m_pending += c; // need one more char
                    return NoOp{};
                }
                resetAccumulator();
                if (op == 'd') return DeleteMotion{c, count};
                if (op == 'c') return ChangeMotion{c, count};
                if (op == 'y') return YankMotion {c, count};
            }

            // ESC cancels pending
            if (ev.key == Key::Escape) { resetAccumulator(); return NoOp{}; }
        }

        if (m_pending.size() == 2) {
            // op + 'f'/'t' + findChar
            const char motionKey = m_pending[1];
            if (c != '\0') {
                const char op2    = m_pending[0];
                const bool fwd    = (motionKey == 'f' || motionKey == 't');
                const bool before = (motionKey == 't' || motionKey == 'T');
                resetAccumulator();
                if (op2 == 'd') return DeleteMotion{motionKey, count, false, c};
                if (op2 == 'c') return ChangeMotion{motionKey, count, false, c};
                if (op2 == 'y') return YankMotion  {motionKey, count, false, c};
                // Fallback: just do the find motion
                return MoveFindChar{c, fwd, before, count};
            }
            return NoOp{};
        }

        resetAccumulator();
    }

    // ── Arrow keys ────────────────────────────────────────────────────────
    switch (ev.key) {
        case Key::ArrowLeft:  return MoveLeft {count};
        case Key::ArrowRight: return MoveRight{count};
        case Key::ArrowUp:    return MoveUp   {count};
        case Key::ArrowDown:  return MoveDown {count};
        case Key::Home:       return MoveLineStart{};
        case Key::End:        return MoveLineEnd{};
        case Key::PageUp:     return ScrollPageUp{};
        case Key::PageDown:   return ScrollPageDown{};
        default: break;
    }

    if (!isPrintable(ev.key)) return NoOp{};

    // ── Single-key Normal commands ────────────────────────────────────────
    switch (c) {
        // Movement
        case 'h': return MoveLeft {count};
        case 'j': return MoveDown {count};
        case 'k': return MoveUp   {count};
        case 'l': return MoveRight{count};
        case 'w': return MoveWordForward {count, false};
        case 'W': return MoveWordForward {count, true };
        case 'b': return MoveWordBackward{count, false};
        case 'B': return MoveWordBackward{count, true };
        case 'e': return MoveWordEnd     {count, false};
        case 'E': return MoveWordEnd     {count, true };
        case '0': { resetAccumulator(); return MoveLineStart{}; }
        case '$': { resetAccumulator(); return MoveLineEnd{};   }
        case '^': { resetAccumulator(); return MoveFirstNonBlank{}; }
        case 'G': { resetAccumulator();
                    return MoveFileEnd{m_count > 0
                        ? std::optional<int>{m_count}
                        : std::nullopt}; }
        case '%': return MoveMatchingBrace{};

        // f/F/t/T – need one more char
        case 'f': case 'F': case 't': case 'T':
            m_pending = std::string(1, c);
            return NoOp{};

        // g prefix
        case 'g':
            m_pending = "g";
            return NoOp{};

        // ── Mode transitions ──────────────────────────────────────────────
        case 'i': { resetAccumulator(); m_mode = Mode::Insert;
                    return EnterInsert{InsertWhere::BeforeCursor}; }
        case 'a': { resetAccumulator(); m_mode = Mode::Insert;
                    return EnterInsert{InsertWhere::AfterCursor}; }
        case 'I': { resetAccumulator(); m_mode = Mode::Insert;
                    return EnterInsert{InsertWhere::LineStart}; }
        case 'A': { resetAccumulator(); m_mode = Mode::Insert;
                    return EnterInsert{InsertWhere::LineEnd}; }
        case 'o': { resetAccumulator(); m_mode = Mode::Insert;
                    return EnterInsert{InsertWhere::NewLineBelow}; }
        case 'O': { resetAccumulator(); m_mode = Mode::Insert;
                    return EnterInsert{InsertWhere::NewLineAbove}; }
        case 's': { resetAccumulator(); m_mode = Mode::Insert;
                    return DeleteCharForward{count}; } // handled as compound in dispatcher
        case 'v': { resetAccumulator(); m_mode = Mode::Visual;      return EnterVisual{}; }
        case 'V': { resetAccumulator(); m_mode = Mode::VisualLine;  return EnterVLine{}; }
        case ':': { resetAccumulator(); m_mode = Mode::Command;
                    m_cmdLine.clear();                return EnterCommand{}; }
        case '/': { resetAccumulator(); m_mode = Mode::Search;
                    m_cmdLine.clear(); m_searchForward = true;  return EnterSearch{true};  }
        case '?': { resetAccumulator(); m_mode = Mode::Search;
                    m_cmdLine.clear(); m_searchForward = false; return EnterSearch{false}; }

        // ── Editing ───────────────────────────────────────────────────────
        case 'x': { resetAccumulator(); return DeleteCharForward{count};  }
        case 'X': { resetAccumulator(); return DeleteCharBackward{count}; }
        case 'D': { resetAccumulator(); return DeleteToLineEnd{};         }
        case 'C': { resetAccumulator(); m_mode = Mode::Insert;
                    return ChangeToLineEnd{};                              }
        case 'J': { resetAccumulator(); return JoinLines{count};          }

        // Operators (wait for motion)
        case 'd': case 'c': case 'y':
            m_pending = std::string(1, c);
            return NoOp{};

        // Indent
        case '>': m_pending = ">"; return NoOp{};
        case '<': m_pending = "<"; return NoOp{};

        // Clipboard
        case 'p': { resetAccumulator(); return Paste{false};  }
        case 'P': { resetAccumulator(); return Paste{true};   }

        // History
        case 'u': { resetAccumulator(); return Undo{}; }

        // Repeat
        case '.': { resetAccumulator(); return RepeatLast{}; }

        // Search repeat
        case 'n': { resetAccumulator(); return SearchNext{true};  }
        case 'N': { resetAccumulator(); return SearchNext{false}; }

        default: return NoOp{};
    }
}

// ── Insert mode ──────────────────────────────────────────────────────────────

Command ModeManager::processInsert(const KeyEvent& ev) {
    if (ev.ctrl) {
        if (keyToChar(ev.key) == 'c' || ev.key == Key::Escape) {
            m_mode = Mode::Normal;
            return LeaveInsert{};
        }
        return NoOp{};
    }

    switch (ev.key) {
        case Key::Escape:
            m_mode = Mode::Normal;
            return LeaveInsert{};

        case Key::Backspace:
            return DeleteCharBackward{1};

        case Key::Delete:
            return DeleteCharForward{1};

        case Key::Enter:
            return InsertChar{'\n'};

        case Key::Tab:
            return InsertChar{'\t'};

        case Key::ArrowLeft:  return MoveLeft{1};
        case Key::ArrowRight: return MoveRight{1};
        case Key::ArrowUp:    return MoveUp{1};
        case Key::ArrowDown:  return MoveDown{1};
        case Key::Home:       return MoveLineStart{};
        case Key::End:        return MoveLineEnd{};

        default:
            if (ev.isPlainChar()) {
                return InsertChar{ev.asChar()};
            }
            return NoOp{};
    }
}

// ── Visual mode ──────────────────────────────────────────────────────────────

Command ModeManager::processVisual(const KeyEvent& ev) {
    if (ev.ctrl) return NoOp{};

    switch (ev.key) {
        case Key::Escape:
            m_mode = Mode::Normal;
            return LeaveVisual{};
        default: break;
    }

    if (!isPrintable(ev.key)) return NoOp{};
    const char c = keyToChar(ev.key);
    const int count = m_count > 0 ? m_count : 1;

    switch (c) {
        // Mode exits
        case 'v': m_mode = Mode::Normal;      return LeaveVisual{};
        case 'V': m_mode = Mode::Normal;      return LeaveVisual{};

        // Operators on selection
        case 'd': { m_mode = Mode::Normal; return VisualDelete{}; }
        case 'c': { m_mode = Mode::Insert; return VisualChange{}; }
        case 'y': { m_mode = Mode::Normal; return VisualYank{};   }
        case '>': return VisualIndentRight{};
        case '<': return VisualIndentLeft{};

        // All motion keys extend the selection
        case 'h': return MoveLeft{count};
        case 'j': return MoveDown{count};
        case 'k': return MoveUp{count};
        case 'l': return MoveRight{count};
        case 'w': return MoveWordForward{count, false};
        case 'W': return MoveWordForward{count, true};
        case 'b': return MoveWordBackward{count, false};
        case 'B': return MoveWordBackward{count, true};
        case 'e': return MoveWordEnd{count, false};
        case 'E': return MoveWordEnd{count, true};
        case '0': return MoveLineStart{};
        case '$': return MoveLineEnd{};
        case 'G': return MoveFileEnd{std::nullopt};

        default: return NoOp{};
    }
}

// ── Command mode (:) ─────────────────────────────────────────────────────────

Command ModeManager::processCommand(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Escape:
            m_mode = Mode::Normal;
            m_cmdLine.clear();
            return NoOp{};

        case Key::Enter: {
            std::string line = m_cmdLine;
            m_cmdLine.clear();
            m_mode = Mode::Normal;
            return ExecuteCommand{std::move(line)};
        }

        case Key::Backspace:
            if (!m_cmdLine.empty()) {
                m_cmdLine.pop_back();
            } else {
                m_mode = Mode::Normal;
            }
            return NoOp{};

        default:
            if (ev.isPlainChar()) {
                m_cmdLine += ev.asChar();
            }
            return NoOp{};
    }
}

// ── Search mode (/ or ?) ──────────────────────────────────────────────────────

Command ModeManager::processSearch(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Escape:
            m_mode = Mode::Normal;
            m_cmdLine.clear();
            return NoOp{};

        case Key::Enter: {
            std::string pattern = m_cmdLine;
            m_cmdLine.clear();
            m_mode = Mode::Normal;
            return SearchPattern{std::move(pattern), m_searchForward};
        }

        case Key::Backspace:
            if (!m_cmdLine.empty()) {
                m_cmdLine.pop_back();
            } else {
                m_mode = Mode::Normal;
            }
            return NoOp{};

        default:
            if (ev.isPlainChar()) {
                m_cmdLine += ev.asChar();
            }
            return NoOp{};
    }
}

// ── Motion / operator-motion resolvers (shared helpers) ──────────────────────

Command ModeManager::resolveMotion(char c, int count) {
    switch (c) {
        case 'h': return MoveLeft {count};
        case 'j': return MoveDown {count};
        case 'k': return MoveUp   {count};
        case 'l': return MoveRight{count};
        case 'w': return MoveWordForward {count, false};
        case 'W': return MoveWordForward {count, true};
        case 'b': return MoveWordBackward{count, false};
        case 'B': return MoveWordBackward{count, true};
        case 'e': return MoveWordEnd     {count, false};
        case 'E': return MoveWordEnd     {count, true};
        case '0': return MoveLineStart{};
        case '$': return MoveLineEnd{};
        case '^': return MoveFirstNonBlank{};
        case 'G': return MoveFileEnd{std::nullopt};
        case '%': return MoveMatchingBrace{};
        default:  return NoOp{};
    }
}

Command ModeManager::resolveOperatorMotion(char op, char motion, int count) {
    if (op == 'd') return DeleteMotion{motion, count};
    if (op == 'c') return ChangeMotion{motion, count};
    if (op == 'y') return YankMotion  {motion, count};
    return NoOp{};
}

} // namespace vimith::input
