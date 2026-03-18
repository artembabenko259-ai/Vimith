#include "vimith/rendering/text_view.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <string>

using namespace ftxui;

namespace vimith::rendering {

// ── Token → ftxui color mapping ──────────────────────────────────────────────

Color tokenColor(syntax::TokenKind kind) noexcept {
    using K = syntax::TokenKind;
    switch (kind) {
        case K::Keyword:      return Color::Blue;
        case K::Type:         return Color::Cyan;
        case K::Number:       return Color::Yellow;
        case K::String:       return Color::Green;
        case K::StringEscape: return Color::GreenLight;
        case K::Char:         return Color::GreenLight;
        case K::Comment:      return Color::GrayDark;
        case K::Preprocessor: return Color::Magenta;
        case K::Operator:     return Color::White;
        case K::Punctuation:  return Color::White;
        case K::Identifier:   return Color::Default;
        case K::Function:     return Color::Yellow;
        case K::Variable:     return Color::Default;
        case K::Constant:     return Color::Cyan;
        case K::Attribute:    return Color::Magenta;
        case K::Macro:        return Color::RedLight;
        case K::Namespace:    return Color::Cyan;
        default:              return Color::Default;
    }
}

// ── Single-line builder ───────────────────────────────────────────────────────

ftxui::Element buildTextLine(std::string_view               lineText,
                              std::size_t                    lineIndex,
                              const EditorState&             state,
                              const syntax::HighlightedLine& hl,
                              bool                           isCursorLine,
                              std::size_t                    visibleStart,
                              int                            visibleWidth) {
    // Clip the visible window (horizontal scroll)
    const std::size_t textLen     = lineText.size();
    const std::size_t clippedStart = std::min(visibleStart, textLen);
    const std::size_t clippedLen  = std::min(
        static_cast<std::size_t>(visibleWidth),
        textLen > clippedStart ? textLen - clippedStart : 0);

    const std::string_view visible = lineText.substr(clippedStart, clippedLen);

    // Cursor column in the clipped view
    const bool hasCursor = isCursorLine;
    const std::size_t cursorColGlobal = state.cursor.col;
    const std::size_t cursorColLocal  =
        (cursorColGlobal >= clippedStart)
            ? cursorColGlobal - clippedStart
            : std::string_view::npos;

    // Visual selection range on this line (clipped)
    bool hasSelection = false;
    std::size_t selStart = 0, selEnd = 0;
    if (state.visualSel.has_value()) {
        const auto& vs = *state.visualSel;
        const CursorPos lo = vs.anchor.isBefore(vs.cursor) ? vs.anchor : vs.cursor;
        const CursorPos hi = vs.anchor.isBefore(vs.cursor) ? vs.cursor : vs.anchor;
        if (lineIndex >= lo.line && lineIndex <= hi.line) {
            hasSelection = true;
            selStart = (lineIndex == lo.line) ? lo.col : 0;
            selEnd   = (lineIndex == hi.line) ? hi.col + 1 : visible.size() + clippedStart;
            selStart = (selStart > clippedStart) ? selStart - clippedStart : 0;
            selEnd   = (selEnd   > clippedStart) ? std::min(selEnd - clippedStart, visible.size()) : 0;
        }
    }

    // Build spans
    // We iterate over bytes of `visible` and apply colour from the token list.
    Elements spans;

    // Map global token list into local (visible-offset) coordinates
    // Build a colour array indexed by local byte offset
    std::vector<syntax::TokenKind> colors(visible.size(), syntax::TokenKind::Default);
    for (const auto& tok : hl.tokens) {
        const std::size_t tokStart = tok.start;
        const std::size_t tokEnd   = tok.start + tok.length;
        // Intersect with [clippedStart, clippedStart + visible.size())
        const std::size_t lo = tokStart > clippedStart ? tokStart - clippedStart : 0;
        const std::size_t hi_val = tokEnd > clippedStart
            ? std::min(tokEnd - clippedStart, visible.size())
            : 0;
        for (std::size_t j = lo; j < hi_val; ++j) {
            colors[j] = tok.kind;
        }
    }

    // Group consecutive same-kind bytes into runs, then build Elements
    std::size_t runStart = 0;
    while (runStart < visible.size()) {
        const syntax::TokenKind runKind = colors[runStart];
        std::size_t runEnd = runStart + 1;
        while (runEnd < visible.size() && colors[runEnd] == runKind) ++runEnd;

        for (std::size_t j = runStart; j < runEnd; ) {
            // Check if cursor or selection breaks the run
            std::size_t chunkEnd = runEnd;

            if (hasCursor && cursorColLocal != std::string_view::npos) {
                if (cursorColLocal >= j && cursorColLocal < runEnd) {
                    chunkEnd = cursorColLocal;
                }
            }
            if (hasSelection) {
                // Find next boundary within [j, runEnd)
                if (selStart > j && selStart < chunkEnd) chunkEnd = selStart;
                if (selEnd   > j && selEnd   < chunkEnd) chunkEnd = selEnd;
            }

            // Before cursor / selection boundary
            if (chunkEnd > j) {
                std::string chunk{visible.substr(j, chunkEnd - j)};
                auto elem = text(std::move(chunk));
                elem = elem | color(tokenColor(runKind));
                // Apply selection background
                if (hasSelection && j >= selStart && j < selEnd) {
                    elem = elem | inverted;
                }
                spans.push_back(std::move(elem));
                j = chunkEnd;
            }

            // Cursor character
            if (hasCursor && cursorColLocal != std::string_view::npos &&
                j == cursorColLocal && j < visible.size()) {
                std::string curCh{visible.substr(j, 1)};
                auto elem = text(std::move(curCh)) | inverted
                                                   | bold;
                spans.push_back(std::move(elem));
                ++j;
            } else if (hasCursor && cursorColLocal != std::string_view::npos &&
                       j == cursorColLocal && j == visible.size()) {
                // Cursor is past end of line (empty or at EOL)
                spans.push_back(text(" ") | inverted);
                break;
            } else if (j == chunkEnd && chunkEnd == runEnd) {
                break; // finished this run
            }
        }

        runStart = runEnd;
    }

    // Cursor past end-of-visible
    if (hasCursor && cursorColLocal != std::string_view::npos &&
        cursorColLocal >= visible.size() && visible.empty()) {
        spans.push_back(text(" ") | inverted);
    }

    if (spans.empty()) {
        // Empty line – still show cursor if applicable
        if (hasCursor && cursorColLocal == 0) {
            return text(" ") | inverted;
        }
        return text("");
    }

    return hbox(std::move(spans));
}

// ── Full text view ────────────────────────────────────────────────────────────

ftxui::Element renderTextView(const EditorState&          state,
                               const core::BufferManager&  buf,
                               const syntax::IHighlighter* highlighter,
                               int                         width,
                               int                         height) {
    const int lineNumWidth = state.options.showLineNumbers ? 6 : 0;
    const int textWidth    = width - lineNumWidth;

    const std::size_t lineCount   = buf.lineCount();
    const std::size_t visLines    = static_cast<std::size_t>(std::max(1, height));
    const std::size_t topLine     = state.topLine;
    const std::size_t bottomLine  = std::min(topLine + visLines, lineCount);

    Elements rows;
    rows.reserve(visLines);

    for (std::size_t ln = topLine; ln < bottomLine; ++ln) {
        const std::string lineText = buf.getLine(ln);
        const bool isCursor = (ln == state.cursor.line);

        // Tokenize
        syntax::HighlightedLine hl;
        if (highlighter) {
            highlighter->tokenize(lineText, hl);
        } else {
            hl.text = lineText;
        }

        Element lineElem = buildTextLine(
            lineText, ln, state, hl, isCursor,
            state.leftCol, textWidth);

        if (state.options.showLineNumbers) {
            // Compute displayed line number
            std::size_t displayNum = ln + 1;
            if (state.options.showRelativeLineNumbers && ln != state.cursor.line) {
                displayNum = (ln > state.cursor.line)
                    ? ln - state.cursor.line
                    : state.cursor.line - ln;
            }

            const std::string numStr = std::to_string(displayNum);
            // Right-align in 4 chars + space
            const std::string padded = std::string(
                4 > numStr.size() ? 4 - numStr.size() : 0, ' ') + numStr + " ";

            Element numElem = text(padded);
            if (ln == state.cursor.line) {
                numElem = numElem | color(Color::YellowLight);
            } else {
                numElem = numElem | color(Color::GrayDark);
            }

            rows.push_back(hbox({numElem, lineElem}));
        } else {
            rows.push_back(lineElem);
        }
    }

    // Fill remaining rows with '~' (Vim style)
    for (std::size_t i = rows.size(); i < visLines; ++i) {
        if (state.options.showLineNumbers) {
            rows.push_back(hbox({
                text("     ") | color(Color::GrayDark),
                text("~")    | color(Color::Blue)
            }));
        } else {
            rows.push_back(text("~") | color(Color::Blue));
        }
    }

    return vbox(std::move(rows)) | flex;
}

} // namespace vimith::rendering
