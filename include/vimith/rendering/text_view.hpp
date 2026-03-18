#pragma once

#include "vimith/editor_state.hpp"
#include "vimith/core/buffer_manager.hpp"
#include "vimith/syntax/i_highlighter.hpp"
#include "vimith/input/mode_manager.hpp"

#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

namespace vimith::rendering {

// ---------------------------------------------------------------------------
// TokenColor – maps a TokenKind to an ftxui Color
// ---------------------------------------------------------------------------
ftxui::Color tokenColor(syntax::TokenKind kind) noexcept;

// ---------------------------------------------------------------------------
// buildTextLine
//
// Renders one buffer line as an ftxui Element.
// Applies syntax highlighting (if `tokens` is non-empty) and marks the
// cursor cell when `isCursorLine` == true.
//
// `visibleStart` – first visible column (horizontal scroll offset)
// `visibleWidth` – number of columns available for text
// ---------------------------------------------------------------------------
ftxui::Element buildTextLine(std::string_view                   lineText,
                              std::size_t                        lineIndex,
                              const EditorState&                 state,
                              const syntax::HighlightedLine&     hl,
                              bool                               isCursorLine,
                              std::size_t                        visibleStart,
                              int                                visibleWidth);

// ---------------------------------------------------------------------------
// renderTextView
//
// Builds the complete text-editor area as a single ftxui Element.
// Includes optional line numbers, cursor highlight, visual selection,
// and syntax highlighting.
// ---------------------------------------------------------------------------
ftxui::Element renderTextView(const EditorState&       state,
                               const core::BufferManager& buf,
                               const syntax::IHighlighter* highlighter,
                               int                       width,
                               int                       height);

} // namespace vimith::rendering
