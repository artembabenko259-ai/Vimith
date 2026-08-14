#include "vimith/rendering/disasm_view.hpp"
#include "vimith/disasm/disassembler.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cstdio>

using namespace ftxui;

namespace vimith::rendering {

namespace {

std::string offsetStr(std::size_t offset) {
    char buf[10];
    std::snprintf(buf, sizeof(buf), "%08zX", offset);
    return {buf};
}

} // namespace

ftxui::Element renderDisasmView(const core::BufferManager& buf,
                                 std::size_t                topOffset,
                                 const HexCursor&           cursor,
                                 int                        height) {
    const std::size_t fileSize = buf.fileSize();
    if (fileSize == 0 || topOffset >= fileSize) {
        return text("(no code)") | color(Color::GrayDark) | center;
    }

    const std::size_t visRows = static_cast<std::size_t>(std::max(1, height));

    // The longest x86-64 instruction is 15 bytes; over-read the window so
    // the decode stream has enough trailing bytes to resolve the last
    // row's instruction instead of truncating it into a "(bad)" placeholder.
    constexpr std::size_t kMaxInsnLen = 15;
    const std::size_t     window =
        std::min(fileSize - topOffset, visRows * kHexBytesPerRow + kMaxInsnLen * 2);
    const auto bytes = buf.readBytes(topOffset, window);

    static const disasm::Disassembler decoder;
    const auto insns = decoder.decode(bytes, topOffset, visRows + 8);

    Elements rows;
    rows.reserve(visRows + 2);
    rows.push_back(text(" Disassembly (x86-64)") | bold | color(Color::CyanLight));
    rows.push_back(separatorLight());

    const std::size_t headerRows = rows.size();
    for (const auto& insn : insns) {
        if (rows.size() - headerRows >= visRows) break;

        const bool isCursor = cursor.byteOffset >= insn.offset
                            && cursor.byteOffset <  insn.offset + insn.length;

        Element line = hbox({
            text(offsetStr(insn.offset) + "  ") | color(Color::GrayLight),
            text(insn.text) | (insn.valid ? color(Color::White) : color(Color::Red)),
        });
        if (isCursor) line = line | inverted | bold;
        rows.push_back(line);
    }

    return vbox(std::move(rows));
}

} // namespace vimith::rendering
