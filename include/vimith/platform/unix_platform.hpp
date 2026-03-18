#pragma once
#include "vimith/platform/i_platform.hpp"

namespace vimith::platform {

// ---------------------------------------------------------------------------
// UnixPlatform – POSIX / Linux / macOS implementation
// Stubbed for future Arch Linux port readiness.
// ---------------------------------------------------------------------------
class UnixPlatform final : public IPlatform {
public:
    UnixPlatform();
    ~UnixPlatform() override;

    bool enableRawMode()  override;
    void disableRawMode() override;

    bool setClipboard(std::string_view text)   override;
    std::optional<std::string> getClipboard()  override;

    std::string     homeDirectory()      const override;
    std::string_view osName()     const noexcept override {
#if defined(__APPLE__)
        return "macos";
#else
        return "linux";
#endif
    }

private:
    bool m_rawModeEnabled = false;
    // termios state stored opaquely to avoid including <termios.h> in header
    unsigned char m_termiosBuf[64]{};
};

} // namespace vimith::platform
