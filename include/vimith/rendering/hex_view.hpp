#pragma once

#include "vimith/editor_state.hpp"
#include "vimith/core/buffer_manager.hpp"

#include <ftxui/dom/elements.hpp>

#include <cstddef>

namespace vimith::rendering {

// Bytes per displayed row in the hex view
inline constexpr std::size_t kHexBytesPerRow = 16;

// ---------------------------------------------------------------------------
// HexCursor – position within the hex view
// ---------------------------------------------------------------------------
struct HexCursor {
    std::size_t byteOffset = 0; // absolute file offset of the selected byte
    bool        nibbleHigh = true; // true = editing high nibble (left digit)
};

// ---------------------------------------------------------------------------
// hexTopRowOffset
//
// Computes the byte offset of the first visible row for a given cursor
// position, keeping the cursor vertically centered. Shared by HexView and
// DisasmView so their scroll windows stay in lockstep frame to frame.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::size_t hexTopRowOffset(std::size_t cursorOffset,
                                                  std::size_t visRows) {
    const std::size_t cursorRow = cursorOffset / kHexBytesPerRow;
    const std::size_t topRow    = (cursorRow >= visRows / 2) ? cursorRow - visRows / 2 : 0;
    return topRow * kHexBytesPerRow;
}

// ---------------------------------------------------------------------------
// renderHexView
//
// Builds the classic dual-pane hex editor layout:
//
//   OFFSET    HEX BYTES (16 per row)               ASCII
//   00000000  48 65 6C 6C 6F 20 57 6F  72 6C 64 0A  Hello Wor ld.
//   ...
//
// The cursor byte is highlighted in both panes.
// ---------------------------------------------------------------------------
ftxui::Element renderHexView(const EditorState&         state,
                              const core::BufferManager& buf,
                              const HexCursor&           cursor,
                              int                        width,
                              int                        height);

} // namespace vimith::rendering
