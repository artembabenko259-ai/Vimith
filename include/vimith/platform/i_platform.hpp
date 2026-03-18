#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace vimith::platform {

// ---------------------------------------------------------------------------
// IPlatform – abstract interface over OS-specific functionality
//
// Three concern groups:
//   1. Console raw mode (for direct key input without echo/buffering)
//   2. Clipboard (system clipboard integration)
//   3. Process / shell utilities
// ---------------------------------------------------------------------------
class IPlatform {
public:
    virtual ~IPlatform() = default;

    // ── Console ────────────────────────────────────────────────────────────

    // Enable raw (unbuffered, no-echo) console mode.
    // Must be called before entering the event loop.
    // Returns true on success.
    virtual bool enableRawMode()  = 0;

    // Restore the console to its original state.
    // Should be called on clean exit (or via RAII).
    virtual void disableRawMode() = 0;

    // ── Clipboard ─────────────────────────────────────────────────────────

    // Set the system clipboard to `text`.
    virtual bool setClipboard(std::string_view text)  = 0;

    // Get the current system clipboard contents.
    virtual std::optional<std::string> getClipboard() = 0;

    // ── Environment ───────────────────────────────────────────────────────

    // Return the user's home directory (e.g. C:\Users\name or /home/name)
    virtual std::string homeDirectory() const = 0;

    // Return the name of the OS (e.g. "win32", "linux", "macos")
    virtual std::string_view osName() const noexcept = 0;
};

} // namespace vimith::platform
