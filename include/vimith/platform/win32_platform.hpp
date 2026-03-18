#pragma once
#include "vimith/platform/i_platform.hpp"

namespace vimith::platform {

class Win32Platform final : public IPlatform {
public:
    Win32Platform();
    ~Win32Platform() override;

    bool enableRawMode()  override;
    void disableRawMode() override;

    bool setClipboard(std::string_view text)   override;
    std::optional<std::string> getClipboard()  override;

    std::string     homeDirectory()      const override;
    std::string_view osName()     const noexcept override { return "win32"; }

private:
    void*         m_hStdIn          = nullptr; // HANDLE
    unsigned long m_originalMode    = 0;
    bool          m_rawModeEnabled  = false;
};

} // namespace vimith::platform
