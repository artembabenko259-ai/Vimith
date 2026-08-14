#pragma once

#include "vimith/core/buffer_manager.hpp"
#include "vimith/rendering/hex_view.hpp"

#include <ftxui/dom/elements.hpp>

#include <cstddef>

namespace vimith::rendering {

// ---------------------------------------------------------------------------
// renderDisasmView
//
// Live x86-64 disassembly panel meant to sit alongside renderHexView.
// Decodes instructions starting at `topOffset` (the same row the hex pane
// is scrolled to, via hexTopRowOffset) and highlights whichever instruction
// contains `cursor.byteOffset` — so moving the hex cursor updates the
// highlighted instruction every frame.
// ---------------------------------------------------------------------------
ftxui::Element renderDisasmView(const core::BufferManager& buf,
                                 std::size_t                topOffset,
                                 const HexCursor&           cursor,
                                 int                        height);

} // namespace vimith::rendering
