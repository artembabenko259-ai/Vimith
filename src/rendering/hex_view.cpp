#include "vimith/rendering/hex_view.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

using namespace ftxui;

namespace vimith::rendering {

static char hexNibble(uint8_t v) noexcept {
    return "0123456789ABCDEF"[v & 0xF];
}

static std::string byteToHex(uint8_t b) {
    char buf[3];
    buf[0] = hexNibble(b >> 4);
    buf[1] = hexNibble(b);
    buf[2] = '\0';
    return {buf};
}

static std::string offsetStr(std::size_t offset) {
    char buf[10];
    std::snprintf(buf, sizeof(buf), "%08zX", offset);
    return {buf};
}

// ── renderHexView ─────────────────────────────────────────────────────────────

ftxui::Element renderHexView(const EditorState&         state,
                              const core::BufferManager& buf,
                              const HexCursor&           cursor,
                              int                        width,
                              int                        height) {
    (void)width; // layout fills terminal width

    const std::size_t fileSize = buf.fileSize();
    if (fileSize == 0) {
        return text("(empty file)") | color(Color::GrayDark) | center;
    }

    const std::size_t bytesPerRow = kHexBytesPerRow;

    // Determine scroll offset (topRow), keeping the cursor row visible
    const std::size_t visRows   = static_cast<std::size_t>(std::max(1, height));
    const std::size_t topRow    = hexTopRowOffset(cursor.byteOffset, visRows) / bytesPerRow;
    const std::size_t totalRows = (fileSize + bytesPerRow - 1) / bytesPerRow;

    Elements rows;
    rows.reserve(visRows + 1);

    // ── Header ──────────────────────────────────────────────────────────────
    {
        std::string header = "Offset    ";
        for (std::size_t i = 0; i < bytesPerRow; ++i) {
            char buf2[4];
            std::snprintf(buf2, sizeof(buf2), "%02zX ", i);
            header += buf2;
            if (i == bytesPerRow / 2 - 1) header += ' ';
        }
        header += " ASCII";
        rows.push_back(text(header) | bold | color(Color::CyanLight));
        rows.push_back(separatorLight());
    }

    for (std::size_t row = topRow; row < std::min(topRow + visRows, totalRows); ++row) {
        const std::size_t rowOffset = row * bytesPerRow;
        const std::size_t rowEnd    = std::min(rowOffset + bytesPerRow, fileSize);
        const auto        bytes     = buf.readBytes(rowOffset, rowEnd - rowOffset);

        Elements rowElems;

        // Offset column
        rowElems.push_back(
            text(offsetStr(rowOffset) + "  ") | color(Color::GrayLight));

        // Hex bytes
        Elements hexElems;
        for (std::size_t i = 0; i < bytesPerRow; ++i) {
            if (i == bytesPerRow / 2) hexElems.push_back(text(" "));

            if (i < bytes.size()) {
                const std::size_t absOffset = rowOffset + i;
                const uint8_t     byte      = bytes[i];
                std::string       hexStr    = byteToHex(byte) + " ";

                auto elem = text(hexStr);
                if (absOffset == cursor.byteOffset) {
                    elem = elem | inverted | bold;
                } else if (byte == 0x00) {
                    elem = elem | color(Color::GrayDark);
                } else if (byte < 0x20 || byte == 0x7F) {
                    elem = elem | color(Color::Red);
                } else if (byte >= 0x80) {
                    elem = elem | color(Color::Magenta);
                } else {
                    elem = elem | color(Color::White);
                }
                hexElems.push_back(std::move(elem));
            } else {
                hexElems.push_back(text("   ")); // padding for short last row
            }
        }
        rowElems.push_back(hbox(std::move(hexElems)));
        rowElems.push_back(text("  "));

        // ASCII column
        Elements asciiElems;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const std::size_t absOffset = rowOffset + i;
            const uint8_t     byte      = bytes[i];
            const char        ch        = (byte >= 0x20 && byte < 0x7F) ? static_cast<char>(byte) : '.';
            auto              elem      = text(std::string(1, ch));

            if (absOffset == cursor.byteOffset) {
                elem = elem | inverted | bold;
            } else if (byte < 0x20 || byte == 0x7F || byte == 0x00) {
                elem = elem | color(Color::GrayDark);
            }
            asciiElems.push_back(std::move(elem));
        }
        rowElems.push_back(hbox(std::move(asciiElems)));

        rows.push_back(hbox(std::move(rowElems)));
    }

    // Scrollbar info
    rows.push_back(separatorLight());
    const double pct = fileSize > 0
        ? static_cast<double>(cursor.byteOffset) / static_cast<double>(fileSize) * 100.0
        : 0.0;
    char pctBuf[32];
    std::snprintf(pctBuf, sizeof(pctBuf), "  Offset: 0x%08zX  (%.1f%%)",
                  cursor.byteOffset, pct);
    rows.push_back(text(pctBuf) | color(Color::GrayLight));

    return vbox(std::move(rows));
}

} // namespace vimith::rendering
