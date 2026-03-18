#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace vimith::core {

// ---------------------------------------------------------------------------
// MmapBuffer
//
// Memory-mapped read/write view over a binary file.
// On Windows  : CreateFileMapping / MapViewOfFile
// On POSIX    : mmap (implemented in unix_platform.cpp stub)
//
// The buffer is opened read-write when writable == true.
// Individual byte writes are written directly to the mapped view; the OS
// flushes the mapping back to disk on close() or explicit flush().
// ---------------------------------------------------------------------------
class MmapBuffer {
public:
    MmapBuffer() = default;
    ~MmapBuffer();

    // Non-copyable, movable
    MmapBuffer(const MmapBuffer&)            = delete;
    MmapBuffer& operator=(const MmapBuffer&) = delete;
    MmapBuffer(MmapBuffer&& other) noexcept;
    MmapBuffer& operator=(MmapBuffer&& other) noexcept;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    bool open(std::string_view path, bool writable = true);
    void close();
    void flush();
    [[nodiscard]] bool isOpen() const { return m_view != nullptr; }

    // ── Read ───────────────────────────────────────────────────────────────
    [[nodiscard]] std::span<const uint8_t> readBytes(std::size_t offset,
                                                     std::size_t length) const;
    [[nodiscard]] std::size_t fileSize() const { return m_fileSize; }

    // ── Write ──────────────────────────────────────────────────────────────
    // Individual byte write – for hex editing.  Returns false if out of range.
    bool writeByte(std::size_t offset, uint8_t value);

    // Write a contiguous span at offset.  Fails if span exceeds file bounds.
    bool writeBytes(std::size_t offset, std::span<const uint8_t> data);

    // ── Convenience ────────────────────────────────────────────────────────
    // Treat the mapped region as text (UTF-8) for line-based access
    [[nodiscard]] std::string_view asStringView() const;
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    void markClean()                    { m_dirty = false; }

private:
    uint8_t*    m_view     = nullptr;
    std::size_t m_fileSize = 0;
    bool        m_writable = false;
    bool        m_dirty    = false;

#ifdef _WIN32
    void* m_hFile = nullptr; // HANDLE – stored as void* to avoid WinAPI includes in header
    void* m_hMap  = nullptr;
#else
    int  m_fd     = -1;
#endif

    void resetHandles();
};

} // namespace vimith::core
