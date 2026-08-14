cmake_minimum_required(VERSION 3.25)
project(Vimith VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd / IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerOptions.cmake)
include(FetchContent)

# ── ftxui ────────────────────────────────────────────────────────────────────
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# ── Catch2 ───────────────────────────────────────────────────────────────────
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── Zydis (x86-64 disassembler) ───────────────────────────────────────────────
# Not shallow-cloned: Zydis pulls in Zycore as a git submodule, and shallow
# submodule fetches are unreliable across git versions.
FetchContent_Declare(zydis
    GIT_REPOSITORY https://github.com/zyantific/zydis.git
    GIT_TAG        v4.1.0
)
set(ZYDIS_BUILD_TOOLS      OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zydis)

# ── nlohmann/json (MCP server's JSON-RPC encoding) ────────────────────────────
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# ── vimith_core  (PieceTable, BufferManager, MmapBuffer stub) ────────────────
add_library(vimith_core STATIC
    src/core/piece_table.cpp
    src/core/mmap_buffer.cpp
    src/core/buffer_manager.cpp
)
target_include_directories(vimith_core PUBLIC include)
target_link_libraries(vimith_core PUBLIC vimith_options)

# ── vimith_input  (KeyEvent, InputHandler, ModeManager) ─────────────────────
add_library(vimith_input STATIC
    src/input/input_handler.cpp
    src/input/mode_manager.cpp
)
target_include_directories(vimith_input PUBLIC include)
target_link_libraries(vimith_input PUBLIC vimith_core)

# ── vimith_command  (CommandDispatcher) ──────────────────────────────────────
add_library(vimith_command STATIC
    src/command/command_dispatcher.cpp
)
target_include_directories(vimith_command PUBLIC include)
target_link_libraries(vimith_command PUBLIC vimith_input)

# ── vimith_syntax  (HighlightEngine + tokenizers) ────────────────────────────
add_library(vimith_syntax STATIC
    src/syntax/highlight_engine.cpp
)
target_include_directories(vimith_syntax PUBLIC include)
target_link_libraries(vimith_syntax PUBLIC vimith_options)

# ── vimith_disasm  (Zydis-backed x86-64 decoder) ─────────────────────────────
add_library(vimith_disasm STATIC
    src/disasm/disassembler.cpp
)
target_include_directories(vimith_disasm PUBLIC include)
target_link_libraries(vimith_disasm PUBLIC vimith_options Zydis)

# ── vimith_mcp  (MCP server: exposes the live buffer as tools over HTTP) ─────
if(WIN32)
    add_library(vimith_mcp STATIC src/mcp/mcp_server_win32.cpp)
    target_link_libraries(vimith_mcp PUBLIC ws2_32)
else()
    add_library(vimith_mcp STATIC src/mcp/mcp_server_stub.cpp)
endif()
target_include_directories(vimith_mcp PUBLIC include)
target_link_libraries(vimith_mcp PUBLIC vimith_core vimith_disasm nlohmann_json::nlohmann_json)

# ── vimith_rendering  (Renderer, TextView, HexView, DisasmView — needs ftxui) ─
add_library(vimith_rendering STATIC
    src/rendering/renderer.cpp
    src/rendering/text_view.cpp
    src/rendering/hex_view.cpp
    src/rendering/disasm_view.cpp
)
target_include_directories(vimith_rendering PUBLIC include)
target_link_libraries(vimith_rendering PUBLIC
    vimith_core
    vimith_input
    vimith_command
    vimith_syntax
    vimith_disasm
    vimith_mcp
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# ── vimith_platform  (Win32 / POSIX) ─────────────────────────────────────────
if(WIN32)
    add_library(vimith_platform STATIC src/platform/win32_platform.cpp)
else()
    add_library(vimith_platform STATIC src/platform/unix_platform.cpp)
endif()
target_include_directories(vimith_platform PUBLIC include)
target_link_libraries(vimith_platform PUBLIC vimith_options)

# ── vimith executable ─────────────────────────────────────────────────────────
add_executable(vimith src/main.cpp)
target_link_libraries(vimith PRIVATE
    vimith_rendering
    vimith_platform
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
