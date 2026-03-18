# Vimith
<<<<<<< HEAD
Vimith is Vim but have more function
=======

A modal terminal editor for text and binary/hex, built on C++20.
Inspired by Vim. Designed for performance-first use on AMD Ryzen 5 7600X.

---

## Features

- **Vim-style modal editing** — Normal / Insert / Visual / Command / Search modes
- **Piece Table** text buffer — O(log n) edits, unlimited undo/redo
- **Hex/Binary mode** — memory-mapped view via WinAPI (`CreateFileMapping`) on Windows
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

---

## Architecture

```
Vimith/
├── include/vimith/
│   ├── core/           PieceTable · MmapBuffer · BufferManager
│   ├── input/          KeyEvent · InputHandler (lock-free ring) · ModeManager
│   ├── command/        i_command.hpp (Command variant) · CommandDispatcher
│   ├── rendering/      Renderer · TextView · HexView
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

- **Main thread** — event loop, rendering, state mutation (zero shared mutable state across threads)
- **File IO thread** — `BufferManager::saveAsync()` dispatches writes via `std::async(std::launch::async)`; caller gets a `std::future<bool>`
- **InputHandler ring buffer** — `RingBuffer<KeyEvent, 512>` with `std::atomic` head/tail; usable from a separate producer thread without locks

---

## Dependencies

All fetched automatically by CMake `FetchContent`:

| Library | Version | Purpose |
|---------|---------|---------|
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | v5.0.0 | TUI rendering, event loop |
| [Catch2](https://github.com/catchorg/Catch2) | v3.5.2 | Unit testing |

---

## Development Roadmap

| Phase | Status | Scope |
|-------|--------|-------|
| 1 | ✅ Done | CMake · PieceTable · BufferManager |
| 2 | ✅ Done | InputHandler · ModeManager · ftxui Renderer |
| 3 | ✅ Done | Full Vim motions · operators · visual mode |
| 4 | ✅ Done | MmapBuffer (WinAPI) · HexView |
| 5 | ✅ Done | HighlightEngine · C++ / Rust / Python tokenizers |
| 6 | TODO  | tree-sitter integration · LSP client stub |
| 7 | TODO  | Arch Linux port · termios raw mode |
>>>>>>> b77f45f (Vimith Engine Release: Piece Table, WinAPI mmap, C++20 Core)
