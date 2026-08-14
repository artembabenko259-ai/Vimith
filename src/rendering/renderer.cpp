#include "vimith/rendering/renderer.hpp"
#include "vimith/rendering/text_view.hpp"
#include "vimith/rendering/hex_view.hpp"
#include "vimith/rendering/disasm_view.hpp"
#include "vimith/input/mode_manager.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>

using namespace ftxui;

namespace vimith::rendering {

// ── ftxui Event → KeyEvent converter ─────────────────────────────────────────

std::optional<input::KeyEvent> Renderer::convertEvent(const ftxui::Event& ev) {
    input::KeyEvent ke;

    // Plain printable character
    if (ev.is_character()) {
        const std::string& ch = ev.character();
        if (ch.size() == 1) {
            const auto c = static_cast<unsigned char>(ch[0]);
            if (c >= 32 && c <= 126) {
                ke.key = input::keyFromChar(static_cast<char>(c));
                return ke;
            }
        }
        return std::nullopt;
    }

    // Special / control keys
    if (ev == Event::Escape)    { ke.key = input::Key::Escape;     return ke; }
    if (ev == Event::Return)    { ke.key = input::Key::Enter;      return ke; }
    if (ev == Event::Backspace) { ke.key = input::Key::Backspace;  return ke; }
    if (ev == Event::Delete)    { ke.key = input::Key::Delete;     return ke; }
    if (ev == Event::Tab)       { ke.key = input::Key::Tab;        return ke; }
    if (ev == Event::ArrowUp)   { ke.key = input::Key::ArrowUp;    return ke; }
    if (ev == Event::ArrowDown) { ke.key = input::Key::ArrowDown;  return ke; }
    if (ev == Event::ArrowLeft) { ke.key = input::Key::ArrowLeft;  return ke; }
    if (ev == Event::ArrowRight){ ke.key = input::Key::ArrowRight; return ke; }
    if (ev == Event::Home)      { ke.key = input::Key::Home;       return ke; }
    if (ev == Event::End)       { ke.key = input::Key::End;        return ke; }
    if (ev == Event::PageUp)    { ke.key = input::Key::PageUp;     return ke; }
    if (ev == Event::PageDown)  { ke.key = input::Key::PageDown;   return ke; }
    if (ev == Event::F1)        { ke.key = input::Key::F1;         return ke; }
    if (ev == Event::F2)        { ke.key = input::Key::F2;         return ke; }
    if (ev == Event::F3)        { ke.key = input::Key::F3;         return ke; }
    if (ev == Event::F4)        { ke.key = input::Key::F4;         return ke; }
    if (ev == Event::F5)        { ke.key = input::Key::F5;         return ke; }
    if (ev == Event::F6)        { ke.key = input::Key::F6;         return ke; }
    if (ev == Event::F7)        { ke.key = input::Key::F7;         return ke; }
    if (ev == Event::F8)        { ke.key = input::Key::F8;         return ke; }
    if (ev == Event::F9)        { ke.key = input::Key::F9;         return ke; }
    if (ev == Event::F10)       { ke.key = input::Key::F10;        return ke; }
    if (ev == Event::F11)       { ke.key = input::Key::F11;        return ke; }
    if (ev == Event::F12)       { ke.key = input::Key::F12;        return ke; }

    // Ctrl combos – ftxui encodes Ctrl+X as a raw byte 1..26 in the input string
    const std::string& raw = ev.input();
    if (raw.size() == 1) {
        const auto b = static_cast<unsigned char>(raw[0]);
        if (b >= 1 && b <= 26) {
            ke.key  = input::keyFromChar(static_cast<char>('a' + b - 1));
            ke.ctrl = true;
            return ke;
        }
    }

    return std::nullopt;
}

// ── Constructor ───────────────────────────────────────────────────────────────

Renderer::Renderer(EditorState&                state,
                   core::BufferManager&        buf,
                   input::ModeManager&         modes,
                   command::CommandDispatcher& dispatcher,
                   syntax::HighlightEngine&    hl,
                   std::mutex&                 uiMutex)
    : m_state(state)
    , m_buf(buf)
    , m_modes(modes)
    , m_dispatcher(dispatcher)
    , m_hl(hl)
    , m_uiMutex(uiMutex)
{}

// ── Status bar ────────────────────────────────────────────────────────────────

ftxui::Element Renderer::buildStatusBar() const {
    using input::Mode;

    const Mode mode  = m_modes.getMode();
    const auto mname = std::string{input::modeName(mode)};

    Color modeBg = Color::Blue;
    switch (mode) {
        case Mode::Insert:      modeBg = Color::Green;   break;
        case Mode::Visual:
        case Mode::VisualLine:
        case Mode::VisualBlock: modeBg = Color::Magenta; break;
        case Mode::Command:
        case Mode::Search:      modeBg = Color::Yellow;  break;
        default: break;
    }

    const Element modeElem = text(" " + mname + " ")
        | bold | color(Color::Black) | bgcolor(modeBg);

    std::string fname = m_state.filename.empty() ? "[No Name]" : m_state.filename;
    if (m_buf.isDirty()) fname += " [+]";
    const Element fileElem = text(" " + fname + " ");

    std::string posStr;
    if (m_buf.getMode() == core::BufferMode::Hex) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "0x%08zX", m_state.hexOffset);
        posStr = std::string{buf} + "  " + std::to_string(m_buf.fileSize()) + "B";
    } else {
        posStr = std::to_string(m_state.cursor.line + 1) + ":"
               + std::to_string(m_state.cursor.col  + 1)
               + "  " + std::to_string(m_buf.lineCount()) + "L";
    }
    const Element posElem = text(posStr + " ") | color(Color::White);

    return hbox({ modeElem, fileElem | flex, posElem })
         | bgcolor(Color::GrayDark);
}

ftxui::Element Renderer::buildCommandLine() const {
    using input::Mode;
    const Mode mode = m_modes.getMode();

    // Pending key prefix (e.g. "d", "g") shown in command line for feedback
    const std::string pending = m_modes.hasPending()
        ? std::string{"["} + /* m_modes.pendingKeys() – not exposed */ "]"
        : "";

    if (!m_state.statusMessage.empty() &&
        mode != Mode::Command && mode != Mode::Search) {
        return text(" " + m_state.statusMessage) | color(Color::White);
    }

    if (mode == Mode::Command) {
        return text(":" + m_modes.commandLine() + "_") | color(Color::White);
    }

    if (mode == Mode::Search) {
        const char prefix = m_modes.searchForward() ? '/' : '?';
        return text(std::string(1, prefix) + m_modes.commandLine() + "_")
            | color(Color::White);
    }

    return text(pending);
}

// ── Main UI builder ───────────────────────────────────────────────────────────

ftxui::Element Renderer::buildUI() {
    const int h          = m_state.termHeight;
    const int w          = m_state.termWidth;
    const int editHeight = std::max(1, h - 2);

    Element editArea;
    if (m_buf.getMode() == core::BufferMode::Hex) {
        const HexCursor hexCursor{m_state.hexOffset, true};
        Element hexPane = renderHexView(m_state, m_buf, hexCursor, w, editHeight);

        if (m_state.options.showDisasm) {
            const std::size_t visRows   = static_cast<std::size_t>(std::max(1, editHeight));
            const std::size_t topOffset = hexTopRowOffset(hexCursor.byteOffset, visRows);
            Element disasmPane = renderDisasmView(m_buf, topOffset, hexCursor, editHeight);
            editArea = hbox({ hexPane | flex, separatorLight(), disasmPane | flex });
        } else {
            editArea = hexPane;
        }
    } else {
        const std::string lang = syntax::HighlightEngine::detectLanguage(m_state.filename);
        const auto* hl         = m_hl.getHighlighter(lang);
        editArea = renderTextView(m_state, m_buf, hl, w, editHeight);
    }

    return vbox({
        editArea | flex,
        buildStatusBar(),
        buildCommandLine()
    });
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void Renderer::loop() {
    auto screen = ScreenInteractive::Fullscreen();

    // Keep a reference to exit closure so we can call it from event handler
    std::function<void()> exitLoop = screen.ExitLoopClosure();

    // If an MCP server is attached, wire it so tool calls handled on its
    // background thread wake this loop immediately: PostEvent() is
    // thread-safe and forces a re-render even though Event::Custom isn't
    // bound to anything in convertEvent() below.
    if (m_mcpServer) {
        m_mcpServer->setRedrawCallback([&screen] { screen.PostEvent(Event::Custom); });
    }

    // ftxui::Renderer creates a component whose Render() calls our lambda.
    // ftxui::CatchEvent wraps it so we intercept all keyboard events.

    auto renderFn = ftxui::Renderer([this, &screen] {
        // Update terminal dimensions from screen
        m_state.termWidth  = screen.dimx();
        m_state.termHeight = screen.dimy();
        // Guards against the MCP server thread mutating EditorState/BufferManager
        // mid-frame (e.g. a write_patch landing while we're building the hex view).
        std::lock_guard<std::mutex> lock(m_uiMutex);
        return buildUI();
    });

    auto root = CatchEvent(renderFn, [&](Event ev) -> bool {
        // Ignore mouse and terminal-resize events (ftxui handles resize internally)
        if (ev.is_mouse()) return false;

        auto ke = convertEvent(ev);
        if (!ke) return false;

        {
            std::lock_guard<std::mutex> lock(m_uiMutex);
            const auto cmd = m_modes.processKey(*ke);
            m_dispatcher.dispatch(cmd);
        }

        // Clear status message on the next keystroke after it was set
        // (so it shows for exactly one rendering cycle after the action)
        // We leave clearing to the dispatcher via new messages.

        if (!m_state.running) {
            exitLoop();
        }

        return true; // consume the event – don't propagate to ftxui defaults
    });

    screen.Loop(root);
}

void Renderer::onResize(int width, int height) {
    m_state.termWidth  = width;
    m_state.termHeight = height;
}

} // namespace vimith::rendering
