#include "vimith/core/piece_table.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <stdexcept>

namespace vimith::core {

// ── Constructor ─────────────────────────────────────────────────────────────

PieceTable::PieceTable(std::string original)
    : m_original(std::move(original))
{
    if (!m_original.empty()) {
        m_pieces.push_back({Piece::Source::Original, 0, m_original.size()});
    }
}

// ── Private helpers ─────────────────────────────────────────────────────────

std::string_view PieceTable::pieceView(const Piece& p) const noexcept {
    const std::string& buf =
        (p.source == Piece::Source::Original) ? m_original : m_addBuffer;
    return std::string_view{buf}.substr(p.offset, p.length);
}

void PieceTable::splitAt(std::size_t offset) {
    std::size_t pos = 0;
    for (auto it = m_pieces.begin(); it != m_pieces.end(); ++it) {
        std::size_t pieceEnd = pos + it->length;

        if (pos == offset) return; // already at boundary

        if (pieceEnd > offset && pos < offset) {
            // offset falls inside this piece – split it
            std::size_t localOff = offset - pos;
            Piece left  {it->source, it->offset,             localOff             };
            Piece right {it->source, it->offset + localOff,  it->length - localOff};
            auto next = m_pieces.erase(it);
            next = m_pieces.insert(next, right);
            m_pieces.insert(next, left);
            return;
        }

        pos = pieceEnd;
        if (pos >= offset) return; // offset is at or past the current end
    }
}

void PieceTable::saveSnapshot() {
    m_undoStack.push_back({m_pieces, m_addBuffer.size()});
    // Cap history at 2000 snapshots to bound memory usage
    if (m_undoStack.size() > 2000) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

// ── Editing ─────────────────────────────────────────────────────────────────

void PieceTable::insert(std::size_t offset, std::string_view text) {
    if (text.empty()) return;

    saveSnapshot();
    m_redoStack.clear();
    m_dirty = true;

    // Append text to the add buffer
    const std::size_t addOff = m_addBuffer.size();
    m_addBuffer.append(text);

    const Piece newPiece{Piece::Source::Add, addOff, text.size()};

    // Empty document: just push the new piece
    if (m_pieces.empty()) {
        m_pieces.push_back(newPiece);
        return;
    }

    // Find the piece that contains `offset`
    std::size_t pos = 0;
    for (auto it = m_pieces.begin(); it != m_pieces.end(); ++it) {
        const std::size_t pieceEnd = pos + it->length;

        if (offset > pieceEnd) {
            pos = pieceEnd;
            continue;
        }

        const std::size_t localOff = offset - pos;

        if (localOff == 0) {
            // Insert before this piece (including offset == 0)
            m_pieces.insert(it, newPiece);
        } else if (localOff == it->length) {
            // Insert after this piece
            m_pieces.insert(std::next(it), newPiece);
        } else {
            // Insert in the middle – split the piece
            Piece left {it->source, it->offset,             localOff             };
            Piece right{it->source, it->offset + localOff,  it->length - localOff};
            auto next = m_pieces.erase(it);
            next = m_pieces.insert(next, right);
            next = m_pieces.insert(next, newPiece);
            m_pieces.insert(next, left);
        }
        return;
    }

    // offset >= total length: append at end
    m_pieces.push_back(newPiece);
}

void PieceTable::erase(std::size_t offset, std::size_t length) {
    if (length == 0) return;

    saveSnapshot();
    m_redoStack.clear();
    m_dirty = true;

    const std::size_t endOff = offset + length;

    // Rebuild the piece list keeping only the portions outside [offset, endOff)
    PieceList result;
    std::size_t pos = 0;

    for (const auto& p : m_pieces) {
        const std::size_t pEnd = pos + p.length;

        if (pEnd <= offset) {
            // Entirely before deletion range
            result.push_back(p);
        } else if (pos >= endOff) {
            // Entirely after deletion range
            result.push_back(p);
        } else {
            // Overlaps – keep the parts outside the deletion window
            if (pos < offset) {
                // Keep left tail
                result.push_back({p.source, p.offset, offset - pos});
            }
            if (pEnd > endOff) {
                // Keep right head
                const std::size_t rightOff = endOff - pos;
                result.push_back({p.source, p.offset + rightOff, pEnd - endOff});
            }
        }

        pos = pEnd;
    }

    m_pieces = std::move(result);
}

// ── Queries ─────────────────────────────────────────────────────────────────

std::string PieceTable::getText() const {
    std::string result;
    result.reserve(length());
    for (const auto& p : m_pieces) {
        result += pieceView(p);
    }
    return result;
}

std::string PieceTable::getLine(std::size_t lineIndex) const {
    std::size_t currentLine = 0;
    std::string lineAccum;

    for (const auto& p : m_pieces) {
        std::string_view sv = pieceView(p);
        std::size_t start = 0;

        for (std::size_t i = 0; i <= sv.size(); ++i) {
            const bool atEnd     = (i == sv.size());
            const bool atNewline = (!atEnd && sv[i] == '\n');

            if (atNewline || atEnd) {
                lineAccum += sv.substr(start, i - start);

                if (atNewline) {
                    if (currentLine == lineIndex) {
                        return lineAccum;
                    }
                    ++currentLine;
                    lineAccum.clear();
                    start = i + 1;
                }
                // atEnd: accumulated fragment continues into next piece
            }
        }

        // Any remaining fragment after the last newline in this piece
        if (start < sv.size()) {
            lineAccum += sv.substr(start);
        }
    }

    // Last line (no trailing newline)
    if (currentLine == lineIndex) {
        return lineAccum;
    }

    return {};
}

std::size_t PieceTable::lineCount() const {
    if (m_pieces.empty()) return 0;

    std::size_t count = 1;
    for (const auto& p : m_pieces) {
        const auto sv = pieceView(p);
        count += static_cast<std::size_t>(
            std::count(sv.begin(), sv.end(), '\n'));
    }
    return count;
}

std::size_t PieceTable::length() const {
    std::size_t total = 0;
    for (const auto& p : m_pieces) {
        total += p.length;
    }
    return total;
}

// ── History ─────────────────────────────────────────────────────────────────

void PieceTable::undo() {
    if (m_undoStack.empty()) return;

    m_redoStack.push_back({m_pieces, m_addBuffer.size()});
    auto& snap = m_undoStack.back();
    m_pieces = std::move(snap.pieces);
    // Restore add-buffer length (trim characters added after this snapshot)
    m_addBuffer.resize(snap.addBufferLen);
    m_undoStack.pop_back();
    m_dirty = true;
}

void PieceTable::redo() {
    if (m_redoStack.empty()) return;

    m_undoStack.push_back({m_pieces, m_addBuffer.size()});
    auto& snap = m_redoStack.back();
    m_pieces = std::move(snap.pieces);
    m_addBuffer.resize(snap.addBufferLen);
    m_redoStack.pop_back();
    m_dirty = true;
}

void PieceTable::markClean() {
    m_dirty = false;
}

void PieceTable::clearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_dirty = false;
}

} // namespace vimith::core
