#pragma once

#include "vimith/editor_state.hpp"
#include "vimith/core/buffer_manager.hpp"
#include "vimith/input/input_handler.hpp"
#include "vimith/input/mode_manager.hpp"
#include "vimith/command/command_dispatcher.hpp"
#include "vimith/syntax/highlight_engine.hpp"
#include "vimith/rendering/hex_view.hpp"
#include "vimith/rendering/disasm_view.hpp"
#include "vimith/mcp/mcp_server.hpp"

#include <ftxui/component/event.hpp>

#include <memory>
#include <mutex>
#include <optional>

namespace vimith::rendering {

// ---------------------------------------------------------------------------
// Renderer
//
// Owns the ftxui ScreenInteractive and drives the main event loop.
//
// Architecture
// ─────────────
// 1. Creates an ftxui Component that renders the entire UI via CatchEvent.
// 2. CatchEvent converts ftxui::Event → vimith::input::KeyEvent → Command.
// 3. CommandDispatcher executes the Command against EditorState + BufferManager.
// 4. ftxui re-renders the component after every event.
//
// The Renderer does not own EditorState or BufferManager; it references
// objects whose lifetimes are managed by main().
// ---------------------------------------------------------------------------
class Renderer {
public:
    Renderer(EditorState&             state,
             core::BufferManager&     buf,
             input::ModeManager&      modes,
             command::CommandDispatcher& dispatcher,
             syntax::HighlightEngine& highlightEngine,
             std::mutex&              uiMutex);

    ~Renderer() = default;

    // Blocking: enters the ftxui event loop and returns when running == false.
    void loop();

    // Resize notification (called on terminal resize events by ftxui)
    void onResize(int width, int height);

    // Optional: if set, the render loop wires up mcpServer's redraw callback
    // so tool calls handled on the MCP thread wake the UI immediately
    // instead of waiting for the next keystroke. Must be called before loop().
    void attachMcpServer(mcp::McpServer* mcpServer) { m_mcpServer = mcpServer; }

private:
    EditorState&                m_state;
    core::BufferManager&        m_buf;
    input::ModeManager&         m_modes;
    command::CommandDispatcher& m_dispatcher;
    syntax::HighlightEngine&    m_hl;
    std::mutex&                 m_uiMutex;
    mcp::McpServer*             m_mcpServer = nullptr;

    // Build the complete UI tree for the current frame
    ftxui::Element buildUI();

    // Build the status bar element
    ftxui::Element buildStatusBar() const;

    // Build the command / search input line at the bottom
    ftxui::Element buildCommandLine() const;

    // Convert an ftxui::Event to a vimith::input::KeyEvent.
    // Returns nullopt for non-keyboard events (mouse, resize).
    static std::optional<input::KeyEvent> convertEvent(const ftxui::Event& ev);
};

} // namespace vimith::rendering
