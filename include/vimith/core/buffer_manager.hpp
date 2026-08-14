#pragma once

#include "vimith/core/piece_table.hpp"
#include "vimith/core/mmap_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace vimith::core {

enum class BufferMode { Text, Hex };

// ---------------------------------------------------------------------------
// BufferManager
//
// Facade that owns either a PieceTable (text mode) or a MmapBuffer (hex mode)
// and exposes a unified API to the rest of the editor.
//
// File I/O is dispatched to a background jthread so the rendering thread is
// never blocked.  Callers receive a std::future they can poll on.
// ---------------------------------------------------------------------------
class BufferManager {
public:
    BufferManager();
    ~BufferManager() = default;

    // Non-copyable
    BufferManager(const BufferManager&)            = delete;
    BufferManager& operator=(const BufferManager&) = delete;

    // ── File operations ────────────────────────────────────────────────────

    // Synchronous open (safe to call at startup before the event loop starts)
    bool openFile(std::string_view path, BufferMode mode = BufferMode::Text);

    // Async save – returns a future that resolves to true on success
    std::future<bool> saveAsync(std::string_view path = {});

    // Synchronous save (blocks caller – only call from non-UI code)
    bool saveSync(std::string_view path = {});

    // ── Mode switching ─────────────────────────────────────────────────────
    void switchMode(BufferMode newMode);
    [[nodiscard]] BufferMode getMode() const { return m_mode; }

    // ── Text-mode API (valid when mode == Text) ────────────────────────────
    void insert(std::size_t offset, std::string_view text);
    void erase(std::size_t offset, std::size_t length);

    [[nodiscard]] std::string getLine(std::size_t lineIndex) const;
    [[nodiscard]] std::size_t lineCount()                    const;
    [[nodiscard]] std::size_t textLength()                   const;

    // Converts (line, col) to a flat character offset
    [[nodiscard]] std::size_t cursorToOffset(std::size_t line,
                                             std::size_t col)   const;

    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    // ── Hex-mode API (valid when mode == Hex) ─────────────────────────────
    [[nodiscard]] std::span<const uint8_t> readBytes(std::size_t offset,
                                                      std::size_t length) const;
    bool writeByte(std::size_t offset, uint8_t value);
    bool writeBytes(std::size_t offset, std::span<const uint8_t> data);

    // ── Universal ─────────────────────────────────────────────────────────
    [[nodiscard]] std::size_t fileSize() const;
    [[nodiscard]] bool        isDirty()  const;
    [[nodiscard]] std::string filePath() const { return m_path; }

    // Mark buffer as clean after successful save
    void markClean();

private:
    std::unique_ptr<PieceTable> m_text;
    std::unique_ptr<MmapBuffer> m_hex;
    BufferMode                  m_mode{BufferMode::Text};
    std::string                 m_path;

    bool saveTextSync(std::string_view path);
};

} // namespace vimith::core
