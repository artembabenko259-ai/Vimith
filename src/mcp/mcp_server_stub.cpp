#include "vimith/mcp/mcp_server.hpp"

// MCP's HTTP transport is implemented for Windows (Winsock2) only so far —
// see mcp_server_win32.cpp. This stub keeps non-Windows builds linking
// (README's Phase 7: Arch Linux port) until that lands; start() simply
// reports that it couldn't bind.

namespace vimith::mcp {

McpServer::McpServer(EditorState& state, core::BufferManager& buf, std::mutex& stateMutex)
    : m_state(state), m_buf(buf), m_mutex(stateMutex)
{}

McpServer::~McpServer() = default;

void McpServer::setRedrawCallback(std::function<void()> cb) { m_redraw = std::move(cb); }

bool McpServer::start(std::uint16_t) { return false; }

void McpServer::stop() {}

} // namespace vimith::mcp
