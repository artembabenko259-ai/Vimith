#include "vimith/platform/win32_platform.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdlib>
#include <string>

namespace vimith::platform {

Win32Platform::Win32Platform()
    : m_hStdIn(GetStdHandle(STD_INPUT_HANDLE))
{}

Win32Platform::~Win32Platform() {
    if (m_rawModeEnabled) disableRawMode();
}

bool Win32Platform::enableRawMode() {
    if (m_rawModeEnabled) return true;

    HANDLE hIn = static_cast<HANDLE>(m_hStdIn);
    DWORD  mode = 0;
    if (!GetConsoleMode(hIn, &mode)) return false;

    m_originalMode = static_cast<unsigned long>(mode);

    // Disable line input and echo; enable VT processing (Windows 10+)
    DWORD newMode = mode;
    newMode &= ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    newMode |=  static_cast<DWORD>(ENABLE_VIRTUAL_TERMINAL_INPUT);

    if (!SetConsoleMode(hIn, newMode)) return false;

    // Also enable VT output so ANSI escape sequences render correctly
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  outMode = 0;
    if (GetConsoleMode(hOut, &outMode)) {
        SetConsoleMode(hOut,
            outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                    | DISABLE_NEWLINE_AUTO_RETURN);
    }

    m_rawModeEnabled = true;
    return true;
}

void Win32Platform::disableRawMode() {
    if (!m_rawModeEnabled) return;
    SetConsoleMode(static_cast<HANDLE>(m_hStdIn),
                   static_cast<DWORD>(m_originalMode));
    m_rawModeEnabled = false;
}

bool Win32Platform::setClipboard(std::string_view text) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    // Allocate global memory for the text (CF_UNICODETEXT requires wide string)
    const int wlen = MultiByteToWideChar(CP_UTF8, 0,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    const HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE,
        (static_cast<SIZE_T>(wlen) + 1) * sizeof(wchar_t));
    if (!hMem) { CloseClipboard(); return false; }

    wchar_t* buf = static_cast<wchar_t*>(GlobalLock(hMem));
    MultiByteToWideChar(CP_UTF8, 0,
        text.data(), static_cast<int>(text.size()), buf, wlen);
    buf[wlen] = L'\0';
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}

std::optional<std::string> Win32Platform::getClipboard() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return std::nullopt;
    if (!OpenClipboard(nullptr)) return std::nullopt;

    const HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) { CloseClipboard(); return std::nullopt; }

    const wchar_t* wstr = static_cast<const wchar_t*>(GlobalLock(hData));
    if (!wstr) { CloseClipboard(); return std::nullopt; }

    const int len = WideCharToMultiByte(CP_UTF8, 0,
        wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), len, nullptr, nullptr);
    // WideCharToMultiByte includes the null terminator in the count
    if (!result.empty() && result.back() == '\0') result.pop_back();

    GlobalUnlock(hData);
    CloseClipboard();
    return result;
}

std::string Win32Platform::homeDirectory() const {
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) return {userProfile};
    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath  = std::getenv("HOMEPATH");
    if (homeDrive && homePath) return std::string{homeDrive} + std::string{homePath};
    return "C:\\";
}

} // namespace vimith::platform
