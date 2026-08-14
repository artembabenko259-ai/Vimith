#pragma once

#include "vimith/editor_state.hpp"
#include "vimith/core/buffer_manager.hpp"

#include <nlohmann/json_fwd.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace vimith::mcp {

// ---------------------------------------------------------------------------
// McpServer
//
// Exposes the file currently open in Vimith as a Model Context Protocol
// server over the MCP "Streamable HTTP" transport (JSON-RPC 2.0, single
// synchronous response per call — no server-initiated SSE stream), bound to
// 127.0.0.1 only. Any MCP-capable client (Claude Desktop, Claude Code, or
// any other tool that speaks MCP) can connect to
// http://127.0.0.1:<port>/mcp and drive the *same live buffer* the user has
// open: reading bytes, disassembling, searching, and patching.
//
// Patches write straight through the memory-mapped file the instant the
// tool call is handled — no confirmation step, no undo. That's a deliberate
// choice (the user asked for full autonomy); the safety trade-off is that
// this process now also owns a loopback network listener with unauthenticated
// write access to whatever file is open, so treat it like any other dev-only
// local debug port and don't run it unattended on a shared machine.
//
// Runs its accept loop on a background thread. All access to EditorState /
// BufferManager is serialized through the caller-supplied mutex, which must
// be the same mutex the render loop locks around dispatch()/buildUI().
// ---------------------------------------------------------------------------
class McpServer {
public:
    McpServer(EditorState& state, core::BufferManager& buf, std::mutex& stateMutex);
    ~McpServer();

    McpServer(const McpServer&)            = delete;
    McpServer& operator=(const McpServer&) = delete;

    // Binds 127.0.0.1:port and starts the background accept loop.
    // Returns false (server stays stopped) if the socket couldn't be
    // created/bound, or if MCP isn't implemented on this platform yet.
    bool start(std::uint16_t port);
    void stop();

    [[nodiscard]] bool running() const noexcept { return m_running.load(); }
    [[nodiscard]] std::uint16_t port() const noexcept { return m_port; }

    // Invoked from the accept-loop thread after any tool call mutates
    // state, so the UI thread wakes and redraws immediately instead of
    // waiting for the next keystroke. Wired up by Renderer once its
    // ScreenInteractive exists.
    void setRedrawCallback(std::function<void()> cb);

private:
    EditorState&          m_state;
    core::BufferManager&  m_buf;
    std::mutex&            m_mutex;
    std::function<void()>  m_redraw;

    std::atomic<bool>      m_running{false};
    std::uint16_t          m_port = 0;
    std::thread            m_thread;

    // Opaque platform socket handle (SOCKET on Windows) — keeps <winsock2.h>
    // out of this header, mirroring MmapBuffer's void*-HANDLE pattern.
    void* m_listenSocket = nullptr;

    void acceptLoop();

    // JSON-RPC 2.0 / MCP method dispatch — defined in the platform .cpp
    // alongside the HTTP transport, so this header stays free of the full
    // nlohmann/json.hpp include.
    nlohmann::json handleRpcInternal(const nlohmann::json& req);
    nlohmann::json callTool(const std::string& name, const nlohmann::json& args);
};

} // namespace vimith::mcp
