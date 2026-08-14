# Vimith
# Vimith — Ultimate C++20 Modal Editor
## Interface Preview
---
| Text Editing Mode (NORMAL) | Binary/Hex Mode (:hex) |
|----------------------------|-------------------------|
| ![Text Mode](Знімок%20екрана%202026-03-18%20143758.png) | ![Hex Mode](Знімок%20екрана%202026-03-18%20143716.png) |
---
Vimith is Vim but have more function
---
A modal terminal editor for text and binary/hex, built on C++20.
Inspired by Vim. Designed for performance-first use on AMD Ryzen 5 7600X.

---

## Features

- **Vim-style modal editing** — Normal / Insert / Visual / Command / Search modes
- **Piece Table** text buffer — O(log n) edits, unlimited undo/redo
- **Hex/Binary mode** — memory-mapped view via WinAPI (`CreateFileMapping`) on Windows
- **Live x86-64 disassembler** — [Zydis](https://github.com/zyantific/zydis)-backed panel next to the hex view; decodes and follows the byte under the cursor as you move or edit
- **Live binary patching** — in `:hex` mode, `i` enters byte-patch editing: type hex digits to overwrite bytes directly through the memory-mapped file, no separate save step, disassembly re-decodes on the next frame
- **MCP server** — exposes the open file as [Model Context Protocol](https://modelcontextprotocol.io) tools (`read_bytes`, `disassemble`, `write_patch`, `search_bytes`, `set_cursor`, `get_status`) over loopback HTTP, so any MCP client (Claude Desktop, Claude Code, or your own agent) can read and patch the *same live buffer* you're watching — see [MCP server](#mcp-server) below
- **Syntax highlighting** — pluggable engine with built-in C++, Rust, and Python tokenizers
- **Non-blocking I/O** — file save dispatched to background thread; render thread never blocks
- **ftxui rendering** — retained-mode component tree, handles terminal resize automatically
- **Cross-platform core** — Windows (MSVC) primary target; POSIX stub ready for Arch Linux

---

## Building

### Requirements

- CMake ≥ 3.25
- MSVC 19.3+ with `/std:c++20` (Windows) **or** Clang 16+ / GCC 12+ (Linux)
- Git (for FetchContent to clone ftxui and Catch2)
- Internet access on first build (dependencies are fetched automatically)

### Windows (MSVC / Cursor)

```powershell
# From the Vimith root directory
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or with Ninja (faster):

```powershell
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux / macOS

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run tests

```bash
cmake --build build --target vimith_tests
cd build && ctest --output-on-failure
```

---

## Usage

```
vimith [OPTIONS] [FILE]

Options:
  --hex       Open FILE in hex/binary mode
  --version   Print version and exit
  --help      Print this help message

Examples:
  vimith main.cpp          # open a source file
  vimith --hex data.bin    # open in hex editor mode
  vimith                   # start with empty buffer
```

---

## Key Bindings (Normal Mode)

| Key           | Action                          |
|---------------|---------------------------------|
| `h j k l`     | Move left / down / up / right   |
| `w b e`       | Word forward / backward / end   |
| `0 ^ $`       | Line start / first non-blank / end |
| `gg G`        | File start / end                |
| `f{c} t{c}`   | Find char forward / before      |
| `F{c} T{c}`   | Find char backward              |
| `%`           | Jump to matching bracket        |
| `Ctrl+u/d`    | Scroll half page                |
| `Ctrl+b/f`    | Scroll full page                |
| **`i a o`**   | Insert before / after / new line below |
| **`I A O`**   | Insert at line start / end / new line above |
| `x X`         | Delete char at / before cursor  |
| `dd D`        | Delete line / to end of line    |
| `cc C`        | Change line / to end of line    |
| `yy Y`        | Yank line / to end of line      |
| `d{motion}`   | Delete motion (dw, db, d$, …)   |
| `c{motion}`   | Change motion                   |
| `y{motion}`   | Yank motion                     |
| `p P`         | Paste after / before cursor     |
| `u Ctrl+r`    | Undo / Redo                     |
| `.`           | Repeat last change              |
| `v V`         | Visual char / line selection    |
| `:`           | Enter Command mode              |
| `/ ?`         | Search forward / backward       |
| `n N`         | Next / previous match           |
| `J`           | Join lines                      |
| `>> <<`       | Indent right / left             |

## Ex Commands (`:` mode)

| Command       | Action                          |
|---------------|---------------------------------|
| `:w`          | Write (save) file               |
| `:w <path>`   | Write to a different path       |
| `:q`          | Quit (fails if unsaved changes) |
| `:q!`         | Force quit                      |
| `:wq`         | Write and quit                  |
| `:e <path>`   | Open file                       |
| `:hex`        | Switch to hex/binary view       |
| `:text`       | Switch back to text view        |
| `:set number` | Show line numbers                |
| `:set nonumber` | Hide line numbers              |
| `:set rnu`    | Relative line numbers           |
| `:set ts=4`   | Set tab stop size               |
| `:set noasm`  | Hide the disassembly panel in `:hex` mode |
| `:set asm`    | Show the disassembly panel in `:hex` mode (default) |

### Hex mode

`hjkl`, arrows, `0`/`$`, `gg`/`G`, `Ctrl+u/d/b/f` move the byte cursor instead
of the text cursor. The disassembly panel decodes x86-64 instructions starting
at the top-left visible byte and highlights whichever instruction contains
the byte under the cursor, updating live as you move or scroll.

Press `i` to enter **hex-patch editing**: type `0-9`/`a-f` to overwrite the
high then low nibble of the byte under the cursor — each completed byte
auto-advances the cursor, like a classic hex editor. Writes go straight
through the memory-mapped file (`MmapBuffer::writeByte`); there is no undo
for hex edits, and `:w` flushes them to disk. `Esc` returns to navigation.

---

## MCP server

Vimith can expose the file you have open as a [Model Context Protocol](https://modelcontextprotocol.io)
server, so any MCP client — Claude Desktop, Claude Code, or a custom
agent — can read and **patch the exact live buffer you're watching** over
loopback HTTP:

```powershell
vimith --mcp firmware.bin --hex          # http://127.0.0.1:7777/mcp
vimith --mcp=8899 firmware.bin --hex     # custom port
```

Point an MCP client at `http://127.0.0.1:<port>/mcp` (Streamable HTTP
transport, JSON-RPC 2.0). Available tools:

| Tool | Effect |
|------|--------|
| `get_status` | File path, size, cursor offset, dirty flag |
| `read_bytes` | Read raw bytes (hex + ASCII) at an offset |
| `disassemble` | Decode x86-64 instructions from an offset |
| `search_bytes` | Find occurrences of a hex byte pattern |
| `set_cursor` | Move the visible hex/disasm cursor (visual feedback only) |
| `write_patch` | Overwrite bytes at an offset — **writes immediately, no confirmation, no undo** |

Design notes / limitations:

- **Full write autonomy, by design.** `write_patch` applies the moment the
  tool call is handled — there is no confirmation prompt on the Vimith side
  and no undo. If you want a review step, put it in the MCP *client* (most
  agent UIs already ask before running a write-shaped tool).
- **Loopback only.** The listener binds `127.0.0.1` exclusively and is never
  reachable from the network. It also validates the `Origin` header on any
  request that sends one, to block browser-based DNS-rebinding.
- **No auth.** Anything that can reach the port can call every tool. Don't
  leave `--mcp` running unattended on a shared machine.
- Implemented on Windows (Winsock2) only for now; `--mcp` is a no-op stub
  on the POSIX build until the Linux port (Phase 7) lands.
- Single JSON-RPC response per request — no server-initiated SSE stream —
  which is sufficient for this server's synchronous, fast tool calls.

---

## Architecture

```
Vimith/
├── include/vimith/
│   ├── core/           PieceTable · MmapBuffer · BufferManager
│   ├── input/          KeyEvent · InputHandler (lock-free ring) · ModeManager
│   ├── command/        i_command.hpp (Command variant) · CommandDispatcher
│   ├── rendering/      Renderer · TextView · HexView · DisasmView
│   ├── disasm/         Disassembler (Zydis-backed x86-64 decoder)
│   ├── mcp/            McpServer (loopback MCP/JSON-RPC tool server)
│   ├── syntax/         IHighlighter · HighlightEngine · C++/Rust/Python
│   └── platform/       IPlatform · Win32Platform · UnixPlatform
├── src/                Implementations
├── tests/              Catch2 unit tests
└── CMakeLists.txt
```

### Data flow

```
Terminal key → ftxui CatchEvent
           → Renderer::convertEvent (ftxui::Event → KeyEvent)
           → ModeManager::processKey (KeyEvent → Command)
           → CommandDispatcher::dispatch (Command → EditorState mutation)
           → Renderer::buildUI (EditorState → ftxui Element tree)
           → ftxui screen render
```

### Concurrency

- **Main thread** — event loop, rendering, state mutation
- **File IO thread** — `BufferManager::saveAsync()` dispatches writes via `std::async(std::launch::async)`; caller gets a `std::future<bool>`
- **InputHandler ring buffer** — `RingBuffer<KeyEvent, 512>` with `std::atomic` head/tail; usable from a separate producer thread without locks
- **MCP accept-loop thread** (only when `--mcp` is passed) — `McpServer` handles HTTP connections and tool calls on a background thread; it and the main thread both lock a shared `std::mutex` (owned by `main()`, passed into both `Renderer` and `McpServer`) before touching `EditorState`/`BufferManager`. After a tool call mutates state, `McpServer` posts a custom event to the render loop so the UI wakes immediately instead of waiting for the next keystroke.

---

## Dependencies

All fetched automatically by CMake `FetchContent`:

| Library | Version | Purpose |
|---------|---------|---------|
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | v5.0.0 | TUI rendering, event loop |
| [Catch2](https://github.com/catchorg/Catch2) | v3.5.2 | Unit testing |
| [Zydis](https://github.com/zyantific/zydis) | v4.1.0 | x86-64 disassembly |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | MCP server JSON-RPC encoding |

---

## Development Roadmap

| Phase | Status | Scope |
|-------|--------|-------|
| 1 | ✅ Done | CMake · PieceTable · BufferManager |
| 2 | ✅ Done | InputHandler · ModeManager · ftxui Renderer |
| 3 | ✅ Done | Full Vim motions · operators · visual mode |
| 4 | ✅ Done | MmapBuffer (WinAPI) · HexView |
| 5 | ✅ Done | HighlightEngine · C++ / Rust / Python tokenizers |
| 6 | ✅ Done | Live x86-64 disassembly panel (Zydis) · hex-mode cursor navigation |
| 7 | ✅ Done | Live hex-patch editing (mmap write-through) · MCP server (Windows) |
| 8 | TODO  | tree-sitter integration · LSP client stub |
| 9 | TODO  | Arch Linux port · termios raw mode · MCP server on POSIX |
