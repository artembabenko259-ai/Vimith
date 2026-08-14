#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vimith::disasm {

// ---------------------------------------------------------------------------
// Instruction – one decoded x86-64 instruction
// ---------------------------------------------------------------------------
struct Instruction {
    std::size_t offset   = 0;     // absolute file offset of the first byte
    std::size_t length   = 0;     // encoded length in bytes
    std::string bytesHex;         // "48 89 E5"
    std::string text;             // formatted "mov rbp, rsp" (Intel syntax)
    bool        valid    = true;  // false = decode failure, text == "(bad)"
};

// ---------------------------------------------------------------------------
// Disassembler
//
// Streaming x86-64 (long mode) decoder backed by Zydis. Stateless: decode()
// builds a fresh Zydis decoder/formatter on every call, so a single instance
// is cheap to keep around and safe to call from the render loop every frame.
// ---------------------------------------------------------------------------
class Disassembler {
public:
    // Decodes sequential instructions starting at data[0] (mapped to file
    // offset baseOffset) until either maxInstructions have been decoded or
    // data is exhausted. On an undecodable byte, emits a 1-byte "(bad)"
    // placeholder and advances by one byte so the stream resynchronizes
    // instead of stalling on arbitrary/non-code binary data.
    [[nodiscard]] std::vector<Instruction> decode(std::span<const std::uint8_t> data,
                                                    std::size_t                   baseOffset,
                                                    std::size_t                   maxInstructions) const;
};

} // namespace vimith::disasm
