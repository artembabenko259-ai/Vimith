#include "vimith/command/command_dispatcher.hpp"
#include "vimith/input/mode_manager.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace vimith::command {

using namespace vimith::core;

CommandDispatcher::CommandDispatcher(EditorState&         state,
                                     BufferManager&       buffer,
                                     input::ModeManager&  modes)
    : m_state(state), m_buf(buffer), m_modes(modes)
{}

// ── Main dispatch ────────────────────────────────────────────────────────────

bool CommandDispatcher::dispatch(const Command& cmd) {
    std::visit([this](const auto& c) { on(c); }, cmd);
    scrollToCursor();
    return m_state.running;
}

// ── Cursor helpers ────────────────────────────────────────────────────────────

std::size_t CommandDispatcher::lineLen(std::size_t line) const {
    return m_buf.getLine(line).size();
}

std::size_t CommandDispatcher::currentLineLen() const {
    return lineLen(m_state.cursor.line);
}

std::size_t CommandDispatcher::cursorOffset() const {
    return m_buf.cursorToOffset(m_state.cursor.line, m_state.cursor.col);
}

void CommandDispatcher::clampCursor() {
    const std::size_t lineCount = m_buf.lineCount();
    if (lineCount == 0) {
        m_state.cursor = {0, 0};
        return;
    }
    m_state.cursor.line = std::min(m_state.cursor.line, lineCount - 1);

    const std::size_t llen = currentLineLen();
    const bool inInsert = (m_modes.getMode() == input::Mode::Insert);
    const std::size_t maxCol = inInsert
        ? llen
        : (llen > 0 ? llen - 1 : 0);
    m_state.cursor.col = std::min(m_state.cursor.col, maxCol);
}

void CommandDispatcher::scrollToCursor() {
    // Vertical
    const int visLines = m_state.termHeight - 2; // status bar occupies 1 line
    if (m_state.cursor.line < m_state.topLine) {
        m_state.topLine = m_state.cursor.line;
    } else if (m_state.cursor.line >= m_state.topLine + static_cast<std::size_t>(visLines)) {
        m_state.topLine = m_state.cursor.line - static_cast<std::size_t>(visLines) + 1;
    }

    // Horizontal
    const int visCols = m_state.termWidth - (m_state.options.showLineNumbers ? 6 : 0);
    if (m_state.cursor.col < m_state.leftCol) {
        m_state.leftCol = m_state.cursor.col;
    } else if (m_state.cursor.col >= m_state.leftCol + static_cast<std::size_t>(visCols)) {
        m_state.leftCol = m_state.cursor.col - static_cast<std::size_t>(visCols) + 1;
    }
}

void CommandDispatcher::moveCursorTo(std::size_t line, std::size_t col) {
    m_state.cursor.line = line;
    m_state.cursor.col  = col;
    clampCursor();
}

void CommandDispatcher::moveCursorBy(int dLine, int dCol) {
    const std::size_t lineCount = m_buf.lineCount();
    if (lineCount == 0) return;

    int newLine = static_cast<int>(m_state.cursor.line) + dLine;
    newLine = std::clamp(newLine, 0, static_cast<int>(lineCount) - 1);

    int newCol = static_cast<int>(m_state.cursor.col) + dCol;
    newCol = std::max(0, newCol);

    m_state.cursor.line = static_cast<std::size_t>(newLine);
    m_state.cursor.col  = static_cast<std::size_t>(newCol);
    clampCursor();
}

// ── Word navigation ───────────────────────────────────────────────────────────

static bool isWordChar(char c, bool bigWord) noexcept {
    if (bigWord) return !std::isspace(static_cast<unsigned char>(c));
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::size_t CommandDispatcher::nextWordStart(std::size_t offset,
                                              bool bigWord) const {
    const std::string text = m_buf.getLine(0); // TODO: multi-line walk
    // Simplified: operate on full text
    const std::string full = [&] {
        std::string s;
        for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
            if (i > 0) s += '\n';
            s += m_buf.getLine(i);
        }
        return s;
    }();

    std::size_t i = offset;
    const std::size_t n = full.size();
    if (i >= n) return n;

    // Skip current word
    while (i < n && isWordChar(full[i], bigWord)) ++i;
    // Skip whitespace
    while (i < n && std::isspace(static_cast<unsigned char>(full[i]))) ++i;
    return i;
}

std::size_t CommandDispatcher::prevWordStart(std::size_t offset,
                                              bool bigWord) const {
    const std::string full = [&] {
        std::string s;
        for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
            if (i > 0) s += '\n';
            s += m_buf.getLine(i);
        }
        return s;
    }();

    if (offset == 0) return 0;
    std::size_t i = offset - 1;

    // Skip whitespace backwards
    while (i > 0 && std::isspace(static_cast<unsigned char>(full[i]))) --i;
    // Skip word backwards
    while (i > 0 && isWordChar(full[i], bigWord)) --i;
    if (!isWordChar(full[i], bigWord) && i < offset) ++i;
    return i;
}

std::size_t CommandDispatcher::nextWordEnd(std::size_t offset,
                                            bool bigWord) const {
    const std::string full = [&] {
        std::string s;
        for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
            if (i > 0) s += '\n';
            s += m_buf.getLine(i);
        }
        return s;
    }();

    std::size_t i = offset;
    const std::size_t n = full.size();
    if (i + 1 >= n) return n - 1;
    ++i;
    // Skip whitespace
    while (i < n && std::isspace(static_cast<unsigned char>(full[i]))) ++i;
    // Skip to end of word
    while (i + 1 < n && isWordChar(full[i + 1], bigWord)) ++i;
    return i;
}

// ── Offset ↔ cursor conversion helpers ───────────────────────────────────────

static std::pair<std::size_t, std::size_t>
offsetToCursor(const std::string& fullText, std::size_t offset) {
    std::size_t line = 0, col = 0, pos = 0;
    for (char c : fullText) {
        if (pos == offset) break;
        if (c == '\n') { ++line; col = 0; }
        else           { ++col; }
        ++pos;
    }
    return {line, col};
}

// ── Movement handlers ─────────────────────────────────────────────────────────

void CommandDispatcher::on(const MoveLeft& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        if (m_state.cursor.col > 0) {
            --m_state.cursor.col;
        }
    }
}

void CommandDispatcher::on(const MoveRight& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        moveCursorBy(0, 1);
    }
}

void CommandDispatcher::on(const MoveUp& cmd) {
    moveCursorBy(-cmd.count, 0);
}

void CommandDispatcher::on(const MoveDown& cmd) {
    moveCursorBy(cmd.count, 0);
}

void CommandDispatcher::on(const MoveWordForward& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t off  = cursorOffset();
        const std::size_t next = nextWordStart(off, cmd.bigWord);
        // Build full text once to map back
        std::string full;
        full.reserve(m_buf.textLength() + m_buf.lineCount());
        for (std::size_t ln = 0; ln < m_buf.lineCount(); ++ln) {
            if (ln > 0) full += '\n';
            full += m_buf.getLine(ln);
        }
        auto [line, col] = offsetToCursor(full, next);
        moveCursorTo(line, col);
    }
}

void CommandDispatcher::on(const MoveWordBackward& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t off  = cursorOffset();
        const std::size_t prev = prevWordStart(off, cmd.bigWord);
        std::string full;
        for (std::size_t ln = 0; ln < m_buf.lineCount(); ++ln) {
            if (ln > 0) full += '\n';
            full += m_buf.getLine(ln);
        }
        auto [line, col] = offsetToCursor(full, prev);
        moveCursorTo(line, col);
    }
}

void CommandDispatcher::on(const MoveWordEnd& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t off  = cursorOffset();
        const std::size_t end  = nextWordEnd(off, cmd.bigWord);
        std::string full;
        for (std::size_t ln = 0; ln < m_buf.lineCount(); ++ln) {
            if (ln > 0) full += '\n';
            full += m_buf.getLine(ln);
        }
        auto [line, col] = offsetToCursor(full, end);
        moveCursorTo(line, col);
    }
}

void CommandDispatcher::on(const MoveLineStart&) {
    m_state.cursor.col = 0;
}

void CommandDispatcher::on(const MoveLineEnd&) {
    const std::size_t llen = currentLineLen();
    m_state.cursor.col = llen > 0 ? llen - 1 : 0;
}

void CommandDispatcher::on(const MoveFirstNonBlank&) {
    const std::string line = m_buf.getLine(m_state.cursor.line);
    std::size_t col = 0;
    while (col < line.size() && std::isspace(static_cast<unsigned char>(line[col])))
        ++col;
    m_state.cursor.col = col;
}

void CommandDispatcher::on(const MoveFileStart&) {
    moveCursorTo(0, 0);
}

void CommandDispatcher::on(const MoveFileEnd& cmd) {
    const std::size_t lineCount = m_buf.lineCount();
    if (lineCount == 0) return;

    if (cmd.line.has_value()) {
        const std::size_t target =
            static_cast<std::size_t>(std::max(0, *cmd.line - 1));
        moveCursorTo(std::min(target, lineCount - 1), 0);
    } else {
        moveCursorTo(lineCount - 1, 0);
    }
}

void CommandDispatcher::on(const MoveFindChar& cmd) {
    m_state.lastFindChar    = cmd.c;
    m_state.lastFindForward = cmd.forward;
    m_state.lastFindBefore  = cmd.before;

    const std::string line = m_buf.getLine(m_state.cursor.line);
    std::size_t col = m_state.cursor.col;

    for (int i = 0; i < cmd.count; ++i) {
        if (cmd.forward) {
            std::size_t start = col + 1;
            while (start < line.size() && line[start] != cmd.c) ++start;
            if (start < line.size()) {
                col = cmd.before ? start - 1 : start;
            }
        } else {
            if (col == 0) break;
            std::size_t start = col - 1;
            while (start > 0 && line[start] != cmd.c) --start;
            if (line[start] == cmd.c) {
                col = cmd.before ? start + 1 : start;
            }
        }
    }
    m_state.cursor.col = col;
    clampCursor();
}

void CommandDispatcher::on(const MoveMatchingBrace&) {
    // Basic %: find matching bracket on the current line
    static constexpr std::pair<char,char> pairs[] = {
        {'(',')'}, {'[',']'}, {'{','}'}
    };

    const std::string line = m_buf.getLine(m_state.cursor.line);
    const std::size_t col  = m_state.cursor.col;
    if (col >= line.size()) return;

    const char ch = line[col];
    for (auto [open, close] : pairs) {
        if (ch == open) {
            int depth = 1;
            for (std::size_t i = col + 1; i < line.size(); ++i) {
                if (line[i] == open)  ++depth;
                if (line[i] == close) --depth;
                if (depth == 0) { m_state.cursor.col = i; return; }
            }
        } else if (ch == close) {
            int depth = 1;
            for (std::size_t i = col; i > 0; --i) {
                if (line[i - 1] == close) ++depth;
                if (line[i - 1] == open)  --depth;
                if (depth == 0) { m_state.cursor.col = i - 1; return; }
            }
        }
    }
}

void CommandDispatcher::on(const ScrollHalfPageUp&) {
    const int half = m_state.termHeight / 2;
    m_state.topLine = m_state.topLine > static_cast<std::size_t>(half)
        ? m_state.topLine - static_cast<std::size_t>(half) : 0;
    moveCursorBy(-half, 0);
}

void CommandDispatcher::on(const ScrollHalfPageDown&) {
    const int half   = m_state.termHeight / 2;
    const std::size_t maxTop = m_buf.lineCount() > 0 ? m_buf.lineCount() - 1 : 0;
    m_state.topLine = std::min(m_state.topLine + static_cast<std::size_t>(half), maxTop);
    moveCursorBy(half, 0);
}

void CommandDispatcher::on(const ScrollPageUp&) {
    const int page = m_state.termHeight - 2;
    m_state.topLine = m_state.topLine > static_cast<std::size_t>(page)
        ? m_state.topLine - static_cast<std::size_t>(page) : 0;
    moveCursorBy(-page, 0);
}

void CommandDispatcher::on(const ScrollPageDown&) {
    const int page = m_state.termHeight - 2;
    const std::size_t maxTop = m_buf.lineCount() > 0 ? m_buf.lineCount() - 1 : 0;
    m_state.topLine = std::min(m_state.topLine + static_cast<std::size_t>(page), maxTop);
    moveCursorBy(page, 0);
}

// ── Mode transitions ──────────────────────────────────────────────────────────

void CommandDispatcher::on(const EnterInsert& cmd) {
    switch (cmd.where) {
        case InsertWhere::AfterCursor: {
            const std::size_t llen = currentLineLen();
            if (llen > 0) ++m_state.cursor.col;
            break;
        }
        case InsertWhere::LineStart:
            m_state.cursor.col = 0;
            break;
        case InsertWhere::LineEnd:
            m_state.cursor.col = currentLineLen();
            break;
        case InsertWhere::NewLineBelow:
            on(OpenLineBelow{});
            return;
        case InsertWhere::NewLineAbove:
            on(OpenLineAbove{});
            return;
        default:
            break;
    }
    clampCursor();
}

void CommandDispatcher::on(const LeaveInsert&) {
    // Adjust cursor one left (Vim behaviour: cursor lands on last inserted char)
    if (m_state.cursor.col > 0) --m_state.cursor.col;
    clampCursor();
}

void CommandDispatcher::on(const EnterVisual&) {
    m_state.visualSel = VisualSelection{m_state.cursor, m_state.cursor};
}

void CommandDispatcher::on(const EnterVLine&) {
    m_state.visualSel = VisualSelection{m_state.cursor, m_state.cursor};
}

void CommandDispatcher::on(const EnterVBlock&) {
    m_state.visualSel = VisualSelection{m_state.cursor, m_state.cursor};
}

void CommandDispatcher::on(const LeaveVisual&) {
    m_state.visualSel.reset();
}

void CommandDispatcher::on(const EnterCommand&) {
    m_state.commandLineInput.clear();
}

void CommandDispatcher::on(const EnterSearch& cmd) {
    m_state.search.forward = cmd.forward;
    m_state.commandLineInput.clear();
}

// ── Editing handlers ──────────────────────────────────────────────────────────

void CommandDispatcher::on(const InsertChar& cmd) {
    const std::size_t off = cursorOffset();
    m_buf.insert(off, std::string_view{&cmd.c, 1});

    if (cmd.c == '\n') {
        ++m_state.cursor.line;
        m_state.cursor.col = 0;
    } else {
        ++m_state.cursor.col;
    }
}

void CommandDispatcher::on(const InsertText& cmd) {
    const std::size_t off = cursorOffset();
    m_buf.insert(off, cmd.text);
    // Move cursor to end of inserted text
    for (char c : cmd.text) {
        if (c == '\n') { ++m_state.cursor.line; m_state.cursor.col = 0; }
        else           { ++m_state.cursor.col; }
    }
}

void CommandDispatcher::on(const DeleteCharForward& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t off  = cursorOffset();
        const std::size_t llen = currentLineLen();
        if (m_state.cursor.col < llen) {
            m_buf.erase(off, 1);
        }
    }
    clampCursor();
}

void CommandDispatcher::on(const DeleteCharBackward& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t off = cursorOffset();
        if (off > 0) {
            m_buf.erase(off - 1, 1);
            if (m_state.cursor.col > 0) {
                --m_state.cursor.col;
            } else if (m_state.cursor.line > 0) {
                --m_state.cursor.line;
                m_state.cursor.col = lineLen(m_state.cursor.line);
            }
        }
    }
    clampCursor();
}

void CommandDispatcher::yankLines(std::size_t startLine, std::size_t count) {
    m_state.yankRegister.clear();
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) m_state.yankRegister += '\n';
        m_state.yankRegister += m_buf.getLine(startLine + i);
    }
    m_state.yankLineMode = true;
}

void CommandDispatcher::deleteLines(std::size_t startLine, std::size_t count) {
    const std::size_t lineCount = m_buf.lineCount();
    const std::size_t actualCount = std::min(count, lineCount - startLine);

    for (std::size_t i = 0; i < actualCount; ++i) {
        const std::size_t off  = m_buf.cursorToOffset(startLine, 0);
        const std::string line = m_buf.getLine(startLine);
        // Delete line + newline (except last line)
        if (startLine + 1 < m_buf.lineCount()) {
            m_buf.erase(off, line.size() + 1);
        } else {
            m_buf.erase(off, line.size());
        }
    }

    if (m_buf.lineCount() == 0) {
        m_state.cursor = {0, 0};
    } else {
        m_state.cursor.line = std::min(startLine, m_buf.lineCount() - 1);
        clampCursor();
    }
}

void CommandDispatcher::on(const DeleteLine& cmd) {
    yankLines(m_state.cursor.line, static_cast<std::size_t>(cmd.count));
    deleteLines(m_state.cursor.line, static_cast<std::size_t>(cmd.count));
    m_lastChange = cmd;
}

void CommandDispatcher::on(const DeleteToLineEnd&) {
    const std::string line = m_buf.getLine(m_state.cursor.line);
    if (m_state.cursor.col >= line.size()) return;

    const std::size_t off = cursorOffset();
    const std::size_t len = line.size() - m_state.cursor.col;
    m_state.yankRegister = line.substr(m_state.cursor.col);
    m_state.yankLineMode = false;
    m_buf.erase(off, len);
    clampCursor();
}

void CommandDispatcher::on(const ChangeToLineEnd&) {
    on(DeleteToLineEnd{});
    // Mode already set to Insert by ModeManager
}

void CommandDispatcher::on(const ChangeLine& cmd) {
    yankLines(m_state.cursor.line, static_cast<std::size_t>(cmd.count));
    // Replace lines with a single empty line
    deleteLines(m_state.cursor.line, static_cast<std::size_t>(cmd.count));
    const std::size_t off = m_buf.cursorToOffset(m_state.cursor.line, 0);
    m_buf.insert(off, "\n");
    m_state.cursor.col = 0;
    // Mode is already Insert
}

void CommandDispatcher::on(const YankLine& cmd) {
    yankLines(m_state.cursor.line, static_cast<std::size_t>(cmd.count));
    m_state.statusMessage = "Yanked " + std::to_string(cmd.count) + " line(s)";
}

void CommandDispatcher::on(const YankToLineEnd&) {
    const std::string line = m_buf.getLine(m_state.cursor.line);
    m_state.yankRegister = line.substr(m_state.cursor.col);
    m_state.yankLineMode = false;
}

void CommandDispatcher::on(const DeleteMotion& cmd) {
    // Simplified: handle common single-line motions
    const std::string line = m_buf.getLine(m_state.cursor.line);
    std::size_t startCol = m_state.cursor.col;
    std::size_t endCol   = startCol;

    switch (cmd.motion) {
        case 'w': case 'W': {
            std::size_t off  = cursorOffset();
            std::size_t next = nextWordStart(off, cmd.motion == 'W');
            // Calculate col delta from offset delta
            const std::size_t lineBefore = m_state.cursor.line;
            std::string full;
            for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
                if (i > 0) full += '\n';
                full += m_buf.getLine(i);
            }
            auto [ln, col] = offsetToCursor(full, next);
            if (ln == lineBefore) {
                endCol = col;
            } else {
                // Multi-line delete: delegate to line-wise delete for now
                const std::size_t eraseLen = next - off;
                m_buf.erase(off, eraseLen);
                clampCursor();
                return;
            }
            break;
        }
        case 'e': case 'E': {
            std::size_t off = cursorOffset();
            std::size_t end = nextWordEnd(off, cmd.motion == 'E');
            std::string full;
            for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
                if (i > 0) full += '\n';
                full += m_buf.getLine(i);
            }
            auto [ln, col] = offsetToCursor(full, end);
            if (ln == m_state.cursor.line) endCol = col + 1;
            break;
        }
        case 'b': case 'B': {
            std::size_t off  = cursorOffset();
            std::size_t prev = prevWordStart(off, cmd.motion == 'B');
            std::string full;
            for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
                if (i > 0) full += '\n';
                full += m_buf.getLine(i);
            }
            auto [ln, col] = offsetToCursor(full, prev);
            if (ln == m_state.cursor.line) { startCol = col; }
            break;
        }
        case '0': startCol = 0; break;
        case '$': endCol   = line.size(); break;
        case 'h': if (startCol > 0) { startCol -= cmd.count; } break;
        case 'l': endCol += cmd.count; endCol = std::min(endCol, line.size()); break;
        case 'f': case 't': {
            for (std::size_t i = endCol + 1; i < line.size(); ++i) {
                if (line[i] == cmd.findChar) {
                    endCol = (cmd.motion == 't') ? i : i + 1;
                    break;
                }
            }
            break;
        }
        default: return;
    }

    if (endCol > startCol) {
        m_state.yankRegister = line.substr(startCol, endCol - startCol);
        m_state.yankLineMode = false;
        const std::size_t off = m_buf.cursorToOffset(m_state.cursor.line, startCol);
        m_buf.erase(off, endCol - startCol);
        m_state.cursor.col = startCol;
        clampCursor();
    }
    m_lastChange = cmd;
}

void CommandDispatcher::on(const ChangeMotion& cmd) {
    on(DeleteMotion{cmd.motion, cmd.count, cmd.bigWord, cmd.findChar});
    // Mode is already Insert (set by ModeManager)
}

void CommandDispatcher::on(const YankMotion& cmd) {
    // Similar to DeleteMotion but without erasing
    const std::string line = m_buf.getLine(m_state.cursor.line);
    std::size_t startCol = m_state.cursor.col;
    std::size_t endCol   = startCol;

    switch (cmd.motion) {
        case '$': endCol = line.size(); break;
        case '0': startCol = 0; break;
        case 'w': endCol = std::min(startCol + static_cast<std::size_t>(cmd.count), line.size()); break;
        default: endCol = std::min(startCol + 1, line.size()); break;
    }

    if (endCol > startCol) {
        m_state.yankRegister = line.substr(startCol, endCol - startCol);
        m_state.yankLineMode = false;
    }
}

void CommandDispatcher::on(const OpenLineBelow&) {
    const std::size_t line = m_state.cursor.line;
    const std::size_t off  = m_buf.cursorToOffset(line, lineLen(line));
    m_buf.insert(off, "\n");
    m_state.cursor.line = line + 1;
    m_state.cursor.col  = 0;
    // ModeManager already set mode to Insert
}

void CommandDispatcher::on(const OpenLineAbove&) {
    const std::size_t line = m_state.cursor.line;
    const std::size_t off  = m_buf.cursorToOffset(line, 0);
    m_buf.insert(off, "\n");
    m_state.cursor.col = 0;
    // cursor.line stays the same – the new empty line is above the old content
}

void CommandDispatcher::on(const JoinLines& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t line     = m_state.cursor.line;
        const std::size_t lineCount = m_buf.lineCount();
        if (line + 1 >= lineCount) break;

        const std::size_t len1 = lineLen(line);
        const std::size_t off  = m_buf.cursorToOffset(line, len1);
        // Replace '\n' with ' '
        m_buf.erase(off, 1);
        m_buf.insert(off, " ");
    }
    clampCursor();
}

void CommandDispatcher::on(const IndentRight& cmd) {
    const std::string indent(static_cast<std::size_t>(
        m_state.options.tabSize), ' ');
    for (int i = 0; i < cmd.count; ++i) {
        const std::size_t off = m_buf.cursorToOffset(m_state.cursor.line, 0);
        m_buf.insert(off, indent);
    }
}

void CommandDispatcher::on(const IndentLeft& cmd) {
    for (int i = 0; i < cmd.count; ++i) {
        const std::string line = m_buf.getLine(m_state.cursor.line);
        std::size_t spaces = 0;
        const std::size_t tabSz = static_cast<std::size_t>(m_state.options.tabSize);
        while (spaces < line.size() && spaces < tabSz
               && std::isspace(static_cast<unsigned char>(line[spaces])))
            ++spaces;
        if (spaces > 0) {
            const std::size_t off = m_buf.cursorToOffset(m_state.cursor.line, 0);
            m_buf.erase(off, spaces);
        }
    }
    clampCursor();
}

// ── Clipboard ─────────────────────────────────────────────────────────────────

void CommandDispatcher::on(const Paste& cmd) {
    if (m_state.yankRegister.empty()) return;

    if (m_state.yankLineMode) {
        on(PasteLine{cmd.before});
        return;
    }

    std::size_t off = cursorOffset();
    if (!cmd.before) {
        const std::size_t llen = currentLineLen();
        if (m_state.cursor.col < llen) ++off;
    }
    m_buf.insert(off, m_state.yankRegister);
    // Move cursor to end of pasted text
    for (char c : m_state.yankRegister) {
        if (c == '\n') { ++m_state.cursor.line; m_state.cursor.col = 0; }
        else           { ++m_state.cursor.col; }
    }
    clampCursor();
}

void CommandDispatcher::on(const PasteLine& cmd) {
    std::size_t targetLine = m_state.cursor.line;
    if (!cmd.before) ++targetLine;

    const std::string content = m_state.yankRegister + "\n";
    const std::size_t off = m_buf.cursorToOffset(
        std::min(targetLine, m_buf.lineCount()), 0);
    m_buf.insert(off, content);
    m_state.cursor.line = targetLine;
    m_state.cursor.col  = 0;
    clampCursor();
}

// ── History ───────────────────────────────────────────────────────────────────

void CommandDispatcher::on(const Undo&) {
    m_buf.undo();
    clampCursor();
}

void CommandDispatcher::on(const Redo&) {
    m_buf.redo();
    clampCursor();
}

void CommandDispatcher::on(const RepeatLast&) {
    dispatch(m_lastChange);
}

// ── File / app ────────────────────────────────────────────────────────────────

void CommandDispatcher::on(const WriteFile& cmd) {
    const std::string path = cmd.path.empty() ? m_state.filename : cmd.path;
    if (path.empty()) {
        m_state.statusMessage = "E: no file name";
        return;
    }
    const bool ok = m_buf.saveSync(path);
    m_state.statusMessage = ok
        ? ("\"" + path + "\" written")
        : ("E: cannot write \"" + path + "\"");
    if (ok && !cmd.path.empty()) m_state.filename = cmd.path;
}

void CommandDispatcher::on(const QuitEditor& cmd) {
    if (!cmd.force && m_buf.isDirty()) {
        m_state.statusMessage = "E: unsaved changes (use :q! to force)";
        return;
    }
    m_state.running = false;
}

void CommandDispatcher::on(const WriteQuit&) {
    on(WriteFile{m_state.filename});
    if (!m_buf.isDirty()) m_state.running = false;
}

void CommandDispatcher::on(const OpenFile& cmd) {
    if (cmd.path.empty()) return;
    const bool ok = m_buf.openFile(cmd.path);
    if (ok) {
        m_state.filename = cmd.path;
        m_state.cursor   = {0, 0};
        m_state.topLine  = 0;
        m_state.statusMessage = "Opened \"" + cmd.path + "\"";
    } else {
        m_state.statusMessage = "E: cannot open \"" + cmd.path + "\"";
    }
}

// ── Search ────────────────────────────────────────────────────────────────────

bool CommandDispatcher::findPattern(const std::string& pattern,
                                    bool               forward,
                                    std::size_t&       outLine,
                                    std::size_t&       outCol) const {
    if (pattern.empty()) return false;
    const std::size_t lineCount = m_buf.lineCount();
    const std::size_t startLine = m_state.cursor.line;
    const std::size_t startCol  = m_state.cursor.col + (forward ? 1 : 0);

    for (std::size_t pass = 0; pass < lineCount + 1; ++pass) {
        const std::size_t ln = (startLine + (forward ? pass : lineCount - pass)) % lineCount;
        const std::string text = m_buf.getLine(ln);

        const std::size_t searchFrom =
            (pass == 0) ? (forward ? startCol : 0) : 0;
        const std::size_t searchTo   =
            (pass == 0 && !forward) ? startCol : text.size();

        const std::size_t found = forward
            ? text.find(pattern, searchFrom)
            : text.rfind(pattern, searchTo);

        if (found != std::string::npos && found <= searchTo) {
            outLine = ln;
            outCol  = found;
            return true;
        }
    }
    return false;
}

void CommandDispatcher::on(const SearchPattern& cmd) {
    m_state.search.pattern = cmd.pattern;
    m_state.search.forward = cmd.forward;
    m_state.commandLineInput.clear();

    std::size_t line = 0, col = 0;
    if (findPattern(cmd.pattern, cmd.forward, line, col)) {
        m_state.search.hasMatch  = true;
        m_state.search.matchPos  = {line, col};
        moveCursorTo(line, col);
    } else {
        m_state.search.hasMatch  = false;
        m_state.statusMessage    = "Pattern not found: " + cmd.pattern;
    }
}

void CommandDispatcher::on(const SearchNext& cmd) {
    if (m_state.search.pattern.empty()) return;
    const bool forward = cmd.forward ? m_state.search.forward
                                     : !m_state.search.forward;
    std::size_t line = 0, col = 0;
    if (findPattern(m_state.search.pattern, forward, line, col)) {
        moveCursorTo(line, col);
    } else {
        m_state.statusMessage = "Pattern not found: " + m_state.search.pattern;
    }
}

// ── Settings ──────────────────────────────────────────────────────────────────

void CommandDispatcher::on(const SetOption& cmd) {
    applySetOption(cmd.name, cmd.value);
}

void CommandDispatcher::applySetOption(const std::string& name,
                                       const std::string& value) {
    auto boolVal = [&](bool def) -> bool {
        if (value == "1" || value == "true" || value == "on")  return true;
        if (value == "0" || value == "false" || value == "off") return false;
        return def;
    };

    if (name == "number"    || name == "nu")   m_state.options.showLineNumbers = boolVal(true);
    else if (name == "nonumber" || name == "nonu") m_state.options.showLineNumbers = false;
    else if (name == "relativenumber" || name == "rnu") m_state.options.showRelativeLineNumbers = boolVal(true);
    else if (name == "tabstop" || name == "ts") {
        try { m_state.options.tabSize = std::stoi(value); } catch (...) {}
    }
    else if (name == "expandtab" || name == "et") m_state.options.expandTabs = boolVal(true);
    else if (name == "noexpandtab" || name == "noet") m_state.options.expandTabs = false;
    else if (name == "hlsearch" || name == "hls") m_state.options.hlSearch = boolVal(true);
    else if (name == "ignorecase" || name == "ic") m_state.options.ignoreCase = boolVal(true);
    else {
        m_state.statusMessage = "E: unknown option: " + name;
    }
}

// ── Buffer mode ───────────────────────────────────────────────────────────────

void CommandDispatcher::on(const SwitchToHexMode&) {
    m_buf.switchMode(BufferMode::Hex);
    m_state.statusMessage = "-- HEX MODE --";
}

void CommandDispatcher::on(const SwitchToTextMode&) {
    m_buf.switchMode(BufferMode::Text);
    m_state.statusMessage = "-- TEXT MODE --";
}

// ── Ex-command parser ─────────────────────────────────────────────────────────

void CommandDispatcher::on(const ExecuteCommand& cmd) {
    executeExCommand(cmd.line);
}

void CommandDispatcher::executeExCommand(const std::string& line) {
    if (line.empty()) return;

    std::istringstream ss{line};
    std::string verb;
    ss >> verb;

    std::string rest;
    std::getline(ss >> std::ws, rest);

    if (verb == "q" || verb == "quit") {
        on(QuitEditor{false});
    } else if (verb == "q!" || verb == "quit!") {
        on(QuitEditor{true});
    } else if (verb == "w" || verb == "write") {
        on(WriteFile{rest});
    } else if (verb == "wq") {
        on(WriteQuit{});
    } else if (verb == "e" || verb == "edit") {
        on(OpenFile{rest});
    } else if (verb == "set") {
        // :set option / :set option=value / :set nooption
        std::string optName  = rest;
        std::string optValue = "1";
        const auto eq = rest.find('=');
        if (eq != std::string::npos) {
            optName  = rest.substr(0, eq);
            optValue = rest.substr(eq + 1);
        }
        on(SetOption{optName, optValue});
    } else if (verb == "hex") {
        on(SwitchToHexMode{});
    } else if (verb == "text") {
        on(SwitchToTextMode{});
    } else {
        m_state.statusMessage = "E: unknown command: " + verb;
    }
}

// ── Visual operators ──────────────────────────────────────────────────────────

void CommandDispatcher::on(const VisualDelete&) {
    if (!m_state.visualSel) return;
    auto [anchor, cur] = *m_state.visualSel;
    m_state.visualSel.reset();

    const CursorPos start = anchor.isBefore(cur) ? anchor : cur;
    const CursorPos end   = anchor.isBefore(cur) ? cur    : anchor;

    const std::size_t startOff = m_buf.cursorToOffset(start.line, start.col);
    const std::size_t endOff   = m_buf.cursorToOffset(end.line, end.col) + 1;
    if (endOff > startOff) {
        // Build full text to slice the selected range
        std::string full;
        full.reserve(m_buf.textLength() + m_buf.lineCount());
        for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
            if (i > 0) full += '\n';
            full += m_buf.getLine(i);
        }
        const std::size_t safeEnd = std::min(endOff, full.size());
        m_state.yankRegister = full.substr(startOff, safeEnd - startOff);
        m_state.yankLineMode = false;
        m_buf.erase(startOff, safeEnd - startOff);
        moveCursorTo(start.line, start.col);
    }
}

void CommandDispatcher::on(const VisualChange&) {
    on(VisualDelete{});
    // Mode already set to Insert by ModeManager
}

void CommandDispatcher::on(const VisualYank&) {
    if (!m_state.visualSel) return;
    auto [anchor, cur] = *m_state.visualSel;
    m_state.visualSel.reset();

    const CursorPos start = anchor.isBefore(cur) ? anchor : cur;
    const CursorPos end   = anchor.isBefore(cur) ? cur    : anchor;

    const std::size_t startOff = m_buf.cursorToOffset(start.line, start.col);
    const std::size_t endOff   = m_buf.cursorToOffset(end.line,   end.col) + 1;

    std::string full;
    for (std::size_t i = 0; i < m_buf.lineCount(); ++i) {
        if (i > 0) full += '\n';
        full += m_buf.getLine(i);
    }
    m_state.yankRegister = full.substr(startOff,
        std::min(endOff, full.size()) - startOff);
    m_state.yankLineMode = false;
    m_state.statusMessage = "Yanked selection";
}

void CommandDispatcher::on(const VisualIndentRight&) {
    if (!m_state.visualSel) return;
    auto [anchor, cur] = *m_state.visualSel;
    const std::size_t startLine = std::min(anchor.line, cur.line);
    const std::size_t endLine   = std::max(anchor.line, cur.line);
    const std::string indent(static_cast<std::size_t>(m_state.options.tabSize), ' ');
    for (std::size_t ln = startLine; ln <= endLine; ++ln) {
        const std::size_t off = m_buf.cursorToOffset(ln, 0);
        m_buf.insert(off, indent);
    }
    m_state.visualSel.reset();
}

void CommandDispatcher::on(const VisualIndentLeft&) {
    if (!m_state.visualSel) return;
    auto [anchor, cur] = *m_state.visualSel;
    const std::size_t startLine = std::min(anchor.line, cur.line);
    const std::size_t endLine   = std::max(anchor.line, cur.line);
    for (std::size_t ln = startLine; ln <= endLine; ++ln) {
        const std::string text = m_buf.getLine(ln);
        std::size_t sp = 0;
        while (sp < text.size() && sp < static_cast<std::size_t>(m_state.options.tabSize)
               && std::isspace(static_cast<unsigned char>(text[sp])))
            ++sp;
        if (sp > 0) {
            const std::size_t off = m_buf.cursorToOffset(ln, 0);
            m_buf.erase(off, sp);
        }
    }
    m_state.visualSel.reset();
}

} // namespace vimith::command
