#include "vimith/disasm/disassembler.hpp"

#include <Zydis/Zydis.h>

namespace vimith::disasm {

namespace {

std::string bytesToHex(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) out += ' ';
        out += kDigits[bytes[i] >> 4];
        out += kDigits[bytes[i] & 0xF];
    }
    return out;
}

} // namespace

std::vector<Instruction> Disassembler::decode(std::span<const std::uint8_t> data,
                                               std::size_t                   baseOffset,
                                               std::size_t                   maxInstructions) const {
    std::vector<Instruction> result;
    if (data.empty() || maxInstructions == 0) return result;
    result.reserve(maxInstructions);

    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    std::size_t pos = 0;
    while (pos < data.size() && result.size() < maxInstructions) {
        ZydisDecodedInstruction insn;
        ZydisDecodedOperand     operands[ZYDIS_MAX_OPERAND_COUNT];

        const ZyanStatus status = ZydisDecoderDecodeFull(
            &decoder, data.data() + pos, data.size() - pos, &insn, operands);

        if (ZYAN_SUCCESS(status)) {
            char buf[160];
            ZydisFormatterFormatInstruction(
                &formatter, &insn, operands, insn.operand_count_visible,
                buf, sizeof(buf), baseOffset + pos, ZYAN_NULL);

            Instruction out;
            out.offset   = baseOffset + pos;
            out.length   = insn.length;
            out.bytesHex = bytesToHex(data.subspan(pos, insn.length));
            out.text     = buf;
            out.valid    = true;
            result.push_back(std::move(out));
            pos += insn.length;
        } else {
            Instruction out;
            out.offset   = baseOffset + pos;
            out.length   = 1;
            out.bytesHex = bytesToHex(data.subspan(pos, 1));
            out.text     = "(bad)";
            out.valid    = false;
            result.push_back(std::move(out));
            pos += 1;
        }
    }

    return result;
}

} // namespace vimith::disasm
