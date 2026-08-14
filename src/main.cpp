#include "vimith/editor_state.hpp"
#include "vimith/core/buffer_manager.hpp"
#include "vimith/input/mode_manager.hpp"
#include "vimith/command/command_dispatcher.hpp"
#include "vimith/syntax/highlight_engine.hpp"
#include "vimith/rendering/renderer.hpp"
#include "vimith/mcp/mcp_server.hpp"

#ifdef _WIN32
#  include "vimith/platform/win32_platform.hpp"
using PlatformImpl = vimith::platform::Win32Platform;
#else
#  include "vimith/platform/unix_platform.hpp"
using PlatformImpl = vimith::platform::UnixPlatform;
#endif

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace {

void printUsage(const char* progName) {
    std::cerr
        << "Usage: " << progName << " [OPTIONS] [FILE]\n"
        << "\n"
        << "A modal terminal editor for text and binary/hex.\n"
        << "\n"
        << "Options:\n"
        << "  --hex       Open FILE in hex/binary mode\n"
        << "  --mcp[=PORT] Start an MCP server (default port 7777) on 127.0.0.1\n"
        << "              so any MCP client can read/patch the open file live\n"
        << "  --version   Print version and exit\n"
        << "  --help      Print this message and exit\n"
        << "\n"
        << "Key bindings (Normal mode):\n"
        << "  hjkl / arrows  Navigate\n"
        << "  i / a / o      Enter Insert mode\n"
        << "  v / V          Enter Visual / Visual-Line mode\n"
        << "  :              Enter Command mode\n"
        << "  /              Search forward\n"
        << "  :w             Write file\n"
        << "  :q             Quit\n"
        << "  :hex / :text   Switch buffer mode\n";
}

} // namespace

int main(int argc, char* argv[]) {
    // ── Argument parsing ───────────────────────────────────────────────────
    std::string    openPath;
    bool           hexMode   = false;
    bool           showHelp  = false;
    bool           mcpEnabled = false;
    std::uint16_t  mcpPort    = 7777;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};

        if (arg == "--help"    || arg == "-h") { showHelp = true; break; }
        if (arg == "--version" || arg == "-v") {
            std::cout << "Vimith 0.1.0\n"; return 0;
        }
        if (arg == "--hex") { hexMode = true; continue; }
        if (arg == "--mcp" || arg.rfind("--mcp=", 0) == 0) {
            mcpEnabled = true;
            if (const auto eq = arg.find('='); eq != std::string_view::npos) {
                try { mcpPort = static_cast<std::uint16_t>(std::stoul(std::string{arg.substr(eq + 1)})); }
                catch (...) { /* keep default port on malformed value */ }
            }
            continue;
        }

        // Treat anything not starting with '-' as a file path
        if (!arg.empty() && arg[0] != '-') {
            openPath = std::string{arg};
        }
    }

    if (showHelp) { printUsage(argv[0]); return 0; }

    // ── Subsystem construction ─────────────────────────────────────────────
    vimith::EditorState state;

    // Platform layer (console raw mode, clipboard)
    auto platform = std::make_unique<PlatformImpl>();
    // ftxui manages the raw mode itself, so we don't call enableRawMode() here.
    // The platform object is used for clipboard access by the dispatcher.

    auto bufMgr     = std::make_shared<vimith::core::BufferManager>();
    auto modeMgr    = std::make_shared<vimith::input::ModeManager>();
    auto hlEngine   = std::make_shared<vimith::syntax::HighlightEngine>();
    // HighlightEngine auto-registers C++, Rust, Python highlighters in its constructor.

    auto dispatcher = std::make_shared<vimith::command::CommandDispatcher>(
        state, *bufMgr, *modeMgr);

    // ── Open file ─────────────────────────────────────────────────────────
    if (!openPath.empty()) {
        const auto mode = hexMode
            ? vimith::core::BufferMode::Hex
            : vimith::core::BufferMode::Text;

        if (!bufMgr->openFile(openPath, mode)) {
            // File doesn't exist yet – start with an empty text buffer and
            // set the filename so :w will save to the right place.
            state.filename = openPath;
        } else {
            state.filename = openPath;
        }
    }

    // ── MCP server (optional) ────────────────────────────────────────────────
    // Shared with Renderer: both lock this before touching state/*bufMgr, since
    // MCP tool calls run on a background thread while the render loop runs on
    // the main thread.
    std::mutex uiMutex;
    vimith::mcp::McpServer mcpServer{state, *bufMgr, uiMutex};

    if (mcpEnabled) {
        if (mcpServer.start(mcpPort)) {
            state.statusMessage =
                "MCP server listening on http://127.0.0.1:" + std::to_string(mcpPort) + "/mcp";
        } else {
            std::cerr << "warning: --mcp requested but the server failed to start "
                         "(port in use, or unsupported on this platform)\n";
        }
    }

    // ── Renderer + event loop ──────────────────────────────────────────────
    vimith::rendering::Renderer renderer{
        state, *bufMgr, *modeMgr, *dispatcher, *hlEngine, uiMutex
    };
    if (mcpServer.running()) renderer.attachMcpServer(&mcpServer);

    renderer.loop();

    return 0;
}
