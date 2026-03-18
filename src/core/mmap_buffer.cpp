#include "vimith/core/mmap_buffer.hpp"

#include <algorithm>
#include <stdexcept>
#include <cstring>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace vimith::core {

// ── Destructor / Move ────────────────────────────────────────────────────────

MmapBuffer::~MmapBuffer() {
    close();
}

MmapBuffer::MmapBuffer(MmapBuffer&& other) noexcept
    : m_view(other.m_view)
    , m_fileSize(other.m_fileSize)
    , m_writable(other.m_writable)
    , m_dirty(other.m_dirty)
#ifdef _WIN32
    , m_hFile(other.m_hFile)
    , m_hMap(other.m_hMap)
#else
    , m_fd(other.m_fd)
#endif
{
    other.resetHandles();
}

MmapBuffer& MmapBuffer::operator=(MmapBuffer&& other) noexcept {
    if (this != &other) {
        close();
        m_view     = other.m_view;
        m_fileSize = other.m_fileSize;
        m_writable = other.m_writable;
        m_dirty    = other.m_dirty;
#ifdef _WIN32
        m_hFile = other.m_hFile;
        m_hMap  = other.m_hMap;
#else
        m_fd = other.m_fd;
#endif
        other.resetHandles();
    }
    return *this;
}

void MmapBuffer::resetHandles() {
    m_view     = nullptr;
    m_fileSize = 0;
    m_writable = false;
    m_dirty    = false;
#ifdef _WIN32
    m_hFile = nullptr;
    m_hMap  = nullptr;
#else
    m_fd = -1;
#endif
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

#ifdef _WIN32

bool MmapBuffer::open(std::string_view path, bool writable) {
    close();

    const DWORD desiredAccess   = writable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
    const DWORD shareMode       = FILE_SHARE_READ;
    const DWORD creationDisp    = OPEN_EXISTING;
    const DWORD flagsAndAttribs = FILE_ATTRIBUTE_NORMAL;

    // Convert UTF-8 path to wide string for Windows API
    const int wlen = MultiByteToWideChar(CP_UTF8, 0,
        path.data(), static_cast<int>(path.size()), nullptr, 0);
    std::wstring wpath(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
        path.data(), static_cast<int>(path.size()),
        wpath.data(), wlen);

    HANDLE hFile = CreateFileW(wpath.c_str(),
        desiredAccess, shareMode, nullptr,
        creationDisp, flagsAndAttribs, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }

    // Empty file – nothing to map; treat as valid but view == nullptr
    if (fileSize.QuadPart == 0) {
        m_hFile    = hFile;
        m_hMap     = nullptr;
        m_view     = nullptr;
        m_fileSize = 0;
        m_writable = writable;
        return true;
    }

    const DWORD protect   = writable ? PAGE_READWRITE : PAGE_READONLY;
    const DWORD mapAccess = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, protect, 0, 0, nullptr);
    if (hMap == nullptr) {
        CloseHandle(hFile);
        return false;
    }

    void* view = MapViewOfFile(hMap, mapAccess, 0, 0, 0);
    if (view == nullptr) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    m_hFile    = hFile;
    m_hMap     = hMap;
    m_view     = static_cast<uint8_t*>(view);
    m_fileSize = static_cast<std::size_t>(fileSize.QuadPart);
    m_writable = writable;
    return true;
}

void MmapBuffer::flush() {
    if (m_view && m_writable) {
        FlushViewOfFile(m_view, 0);
        FlushFileBuffers(static_cast<HANDLE>(m_hFile));
        m_dirty = false;
    }
}

void MmapBuffer::close() {
    if (m_view)  { UnmapViewOfFile(m_view); }
    if (m_hMap)  { CloseHandle(static_cast<HANDLE>(m_hMap)); }
    if (m_hFile) { CloseHandle(static_cast<HANDLE>(m_hFile)); }
    resetHandles();
}

#else // POSIX

bool MmapBuffer::open(std::string_view path, bool writable) {
    close();

    const int flags = writable ? O_RDWR : O_RDONLY;
    const int fd = ::open(std::string{path}.c_str(), flags);
    if (fd < 0) return false;

    struct stat st{};
    if (fstat(fd, &st) < 0) { ::close(fd); return false; }

    const std::size_t sz = static_cast<std::size_t>(st.st_size);
    if (sz == 0) {
        m_fd       = fd;
        m_fileSize = 0;
        m_writable = writable;
        return true;
    }

    const int prot  = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
    void* view = mmap(nullptr, sz, prot, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) { ::close(fd); return false; }

    m_fd       = fd;
    m_view     = static_cast<uint8_t*>(view);
    m_fileSize = sz;
    m_writable = writable;
    return true;
}

void MmapBuffer::flush() {
    if (m_view && m_writable) {
        msync(m_view, m_fileSize, MS_SYNC);
        m_dirty = false;
    }
}

void MmapBuffer::close() {
    if (m_view)  { munmap(m_view, m_fileSize); }
    if (m_fd >= 0) { ::close(m_fd); }
    resetHandles();
}

#endif

// ── Read ─────────────────────────────────────────────────────────────────────

std::span<const uint8_t> MmapBuffer::readBytes(std::size_t offset,
                                                std::size_t length) const {
    if (!m_view || offset >= m_fileSize) return {};
    const std::size_t safeLen = std::min(length, m_fileSize - offset);
    return {m_view + offset, safeLen};
}

std::string_view MmapBuffer::asStringView() const {
    if (!m_view) return {};
    return {reinterpret_cast<const char*>(m_view), m_fileSize};
}

// ── Write ────────────────────────────────────────────────────────────────────

bool MmapBuffer::writeByte(std::size_t offset, uint8_t value) {
    if (!m_view || !m_writable || offset >= m_fileSize) return false;
    m_view[offset] = value;
    m_dirty        = true;
    return true;
}

bool MmapBuffer::writeBytes(std::size_t offset, std::span<const uint8_t> data) {
    if (!m_view || !m_writable) return false;
    if (offset + data.size() > m_fileSize) return false;
    std::memcpy(m_view + offset, data.data(), data.size());
    m_dirty = true;
    return true;
}

} // namespace vimith::core
