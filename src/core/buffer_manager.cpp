#include "vimith/core/buffer_manager.hpp"

#include <fstream>
#include <future>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace vimith::core {

BufferManager::BufferManager()
    : m_text(std::make_unique<PieceTable>())
    , m_hex(std::make_unique<MmapBuffer>())
{}

// ── File operations ──────────────────────────────────────────────────────────

bool BufferManager::openFile(std::string_view path, BufferMode mode) {
    m_path = std::string{path};
    m_mode = mode;

    if (mode == BufferMode::Text) {
        std::ifstream f{m_path, std::ios::binary};
        if (!f) return false;

        std::ostringstream ss;
        ss << f.rdbuf();
        m_text = std::make_unique<PieceTable>(ss.str());
        m_text->clearHistory();
        return true;
    } else {
        return m_hex->open(path, /*writable=*/true);
    }
}

bool BufferManager::saveTextSync(std::string_view path) {
    const std::string& target = path.empty() ? m_path : std::string{path};
    if (target.empty()) return false;

    std::ofstream f{target, std::ios::binary | std::ios::trunc};
    if (!f) return false;

    // Write through PieceTable's getText() – acceptable for Phase 1.
    // In a production build this would stream piece-by-piece to avoid the
    // full O(n) allocation.
    const std::string content = m_text->getText();
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f) return false;

    if (!path.empty()) m_path = std::string{path};
    m_text->markClean();
    return true;
}

bool BufferManager::saveSync(std::string_view path) {
    if (m_mode == BufferMode::Text) {
        return saveTextSync(path);
    } else {
        m_hex->flush();
        if (!path.empty() && path != m_path) {
            // "Save as" in hex mode requires a copy – not yet implemented
            return false;
        }
        m_hex->markClean();
        return true;
    }
}

std::future<bool> BufferManager::saveAsync(std::string_view path) {
    // Capture the content synchronously on the calling thread,
    // then do the actual file I/O asynchronously.
    if (m_mode == BufferMode::Text) {
        const std::string content = m_text->getText();
        const std::string target  = path.empty() ? m_path : std::string{path};

        return std::async(std::launch::async, [content, target, this]() -> bool {
            std::ofstream f{target, std::ios::binary | std::ios::trunc};
            if (!f) return false;
            f.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!f) return false;
            m_text->markClean();
            return true;
        });
    } else {
        return std::async(std::launch::async, [this]() -> bool {
            m_hex->flush();
            m_hex->markClean();
            return true;
        });
    }
}

// ── Mode switching ───────────────────────────────────────────────────────────

void BufferManager::switchMode(BufferMode newMode) {
    if (m_mode == newMode) return;
    m_mode = newMode;

    if (newMode == BufferMode::Hex && !m_hex->isOpen() && !m_path.empty()) {
        m_hex->open(m_path, /*writable=*/true);
    }
}

// ── Text-mode API ────────────────────────────────────────────────────────────

void BufferManager::insert(std::size_t offset, std::string_view text) {
    if (m_mode != BufferMode::Text) return;
    m_text->insert(offset, text);
}

void BufferManager::erase(std::size_t offset, std::size_t length) {
    if (m_mode != BufferMode::Text) return;
    m_text->erase(offset, length);
}

std::string BufferManager::getLine(std::size_t lineIndex) const {
    if (m_mode != BufferMode::Text) return {};
    return m_text->getLine(lineIndex);
}

std::size_t BufferManager::lineCount() const {
    if (m_mode != BufferMode::Text) return 0;
    return m_text->lineCount();
}

std::size_t BufferManager::textLength() const {
    if (m_mode != BufferMode::Text) return 0;
    return m_text->length();
}

std::size_t BufferManager::cursorToOffset(std::size_t line,
                                           std::size_t col) const {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < line; ++i) {
        const std::string ln = m_text->getLine(i);
        offset += ln.size() + 1; // +1 for '\n'
    }
    const std::string ln = m_text->getLine(line);
    offset += std::min(col, ln.size());
    return offset;
}

void BufferManager::undo() { if (m_text) m_text->undo(); }
void BufferManager::redo() { if (m_text) m_text->redo(); }
bool BufferManager::canUndo() const { return m_text && m_text->canUndo(); }
bool BufferManager::canRedo() const { return m_text && m_text->canRedo(); }

// ── Hex-mode API ─────────────────────────────────────────────────────────────

std::span<const uint8_t> BufferManager::readBytes(std::size_t offset,
                                                    std::size_t length) const {
    if (m_mode != BufferMode::Hex) return {};
    return m_hex->readBytes(offset, length);
}

bool BufferManager::writeByte(std::size_t offset, uint8_t value) {
    if (m_mode != BufferMode::Hex) return false;
    return m_hex->writeByte(offset, value);
}

bool BufferManager::writeBytes(std::size_t offset, std::span<const uint8_t> data) {
    if (m_mode != BufferMode::Hex) return false;
    return m_hex->writeBytes(offset, data);
}

// ── Universal ────────────────────────────────────────────────────────────────

std::size_t BufferManager::fileSize() const {
    if (m_mode == BufferMode::Text) return m_text ? m_text->length() : 0;
    return m_hex ? m_hex->fileSize() : 0;
}

bool BufferManager::isDirty() const {
    if (m_mode == BufferMode::Text) return m_text && m_text->isDirty();
    return m_hex && m_hex->isDirty();
}

void BufferManager::markClean() {
    if (m_text) m_text->markClean();
    if (m_hex)  m_hex->markClean();
}

} // namespace vimith::core
