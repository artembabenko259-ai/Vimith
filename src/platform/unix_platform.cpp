#include "vimith/platform/unix_platform.hpp"

#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#  include <termios.h>
#  include <unistd.h>
#  include <cstdio>
#endif

namespace vimith::platform {

#ifndef _WIN32
static_assert(sizeof(struct termios) <= 64,
    "termios does not fit in the opaque buffer; increase m_termiosBuf size");
#endif

UnixPlatform::UnixPlatform() = default;

UnixPlatform::~UnixPlatform() {
    if (m_rawModeEnabled) disableRawMode();
}

bool UnixPlatform::enableRawMode() {
#ifdef _WIN32
    return false; // Not applicable on Windows
#else
    if (m_rawModeEnabled) return true;

    struct termios raw{};
    if (tcgetattr(STDIN_FILENO, &raw) < 0) return false;

    // Save original
    std::memcpy(m_termiosBuf, &raw, sizeof(raw));

    // Configure raw mode
    raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    raw.c_cflag |=  static_cast<tcflag_t>(CS8);
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return false;

    m_rawModeEnabled = true;
    return true;
#endif
}

void UnixPlatform::disableRawMode() {
#ifndef _WIN32
    if (!m_rawModeEnabled) return;
    struct termios orig{};
    std::memcpy(&orig, m_termiosBuf, sizeof(orig));
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    m_rawModeEnabled = false;
#endif
}

bool UnixPlatform::setClipboard(std::string_view text) {
#ifdef _WIN32
    return false;
#else
    // Try xclip first, then xsel, then wl-copy (Wayland)
    for (const char* cmd : {"xclip -selection clipboard", "xsel --clipboard --input", "wl-copy"}) {
        FILE* pipe = popen(cmd, "w");
        if (!pipe) continue;
        fwrite(text.data(), 1, text.size(), pipe);
        const int ret = pclose(pipe);
        if (ret == 0) return true;
    }
    return false;
#endif
}

std::optional<std::string> UnixPlatform::getClipboard() {
#ifdef _WIN32
    return std::nullopt;
#else
    for (const char* cmd : {"xclip -selection clipboard -o", "xsel --clipboard --output", "wl-paste"}) {
        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;
        std::string result;
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) result += buf;
        pclose(pipe);
        if (!result.empty()) return result;
    }
    return std::nullopt;
#endif
}

std::string UnixPlatform::homeDirectory() const {
    const char* home = std::getenv("HOME");
    return home ? std::string{home} : "/";
}

} // namespace vimith::platform
