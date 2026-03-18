#pragma once

#include <cstdint>

namespace vimith::input {

// ---------------------------------------------------------------------------
// Key – platform-agnostic key enumeration
//
// Printable ASCII characters are represented by their ASCII value (32-126).
// Special keys use values > 127.
// ---------------------------------------------------------------------------
enum class Key : uint32_t {
    // ── Printable range (mirrors ASCII) ────────────────────────────────────
    Space       = 32,
    // 33-126 : standard printable characters (!, ", #, ... ~)

    // ── Control characters ─────────────────────────────────────────────────
    Backspace   = 8,
    Tab         = 9,
    Enter       = 13,
    Escape      = 27,
    Delete      = 127,

    // ── Extended / special keys (> 127) ───────────────────────────────────
    ArrowUp     = 256,
    ArrowDown   = 257,
    ArrowLeft   = 258,
    ArrowRight  = 259,

    Home        = 260,
    End         = 261,
    PageUp      = 262,
    PageDown    = 263,
    Insert      = 264,

    F1  = 270,
    F2  = 271,
    F3  = 272,
    F4  = 273,
    F5  = 274,
    F6  = 275,
    F7  = 276,
    F8  = 277,
    F9  = 278,
    F10 = 279,
    F11 = 280,
    F12 = 281,

    Unknown     = 0xFFFF,
};

// Construct a Key from a printable ASCII char
inline constexpr Key keyFromChar(char c) noexcept {
    return static_cast<Key>(static_cast<uint32_t>(c));
}

// Extract the char value from a Key (only valid for printable range)
inline constexpr char keyToChar(Key k) noexcept {
    return static_cast<char>(static_cast<uint32_t>(k));
}

inline constexpr bool isPrintable(Key k) noexcept {
    const auto v = static_cast<uint32_t>(k);
    return v >= 32 && v <= 126;
}

// ---------------------------------------------------------------------------
// KeyEvent – a single keyboard event
// ---------------------------------------------------------------------------
struct KeyEvent {
    Key  key   = Key::Unknown;
    bool ctrl  = false;
    bool shift = false;
    bool alt   = false;

    // Convenience: true if this is a plain printable character with no
    // modifier other than shift (which is already baked into the key value).
    [[nodiscard]] bool isPlainChar() const noexcept {
        return isPrintable(key) && !ctrl && !alt;
    }

    [[nodiscard]] char asChar() const noexcept {
        return keyToChar(key);
    }

    bool operator==(const KeyEvent&) const = default;
};

} // namespace vimith::input
