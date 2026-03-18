#pragma once

#include <cstddef>
#include <list>
#include <string>
#include <string_view>
#include <vector>

namespace vimith::core {

struct Piece {
    enum class Source : uint8_t { Original, Add };

    Source      source;
    std::size_t offset;
    std::size_t length;

    bool operator==(const Piece&) const = default;
};

// ---------------------------------------------------------------------------
// PieceTable
//
// A classic two-buffer piece table:
//   - m_original  : immutable view of the initial file content
//   - m_addBuffer : append-only buffer for all inserted text
//   - m_pieces    : ordered list of (source, offset, length) spans
//
// Complexity:
//   insert / erase : O(n) pieces traversal, typically O(1) amortised
//   getText        : O(total chars)
//   getLine(n)     : O(total chars)  [acceptable for Phase 1]
//   undo / redo    : O(pieces snapshot) per step
// ---------------------------------------------------------------------------
class PieceTable {
public:
    explicit PieceTable(std::string original = {});

    // ── Editing ────────────────────────────────────────────────────────────
    void insert(std::size_t offset, std::string_view text);
    void erase(std::size_t offset, std::size_t length);

    // ── Queries ────────────────────────────────────────────────────────────
    [[nodiscard]] std::string     getText()                     const;
    [[nodiscard]] std::string     getLine(std::size_t index)   const;
    [[nodiscard]] std::size_t     lineCount()                   const;
    [[nodiscard]] std::size_t     length()                      const;
    [[nodiscard]] bool            isDirty()                     const { return m_dirty; }

    // ── History ────────────────────────────────────────────────────────────
    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool canRedo() const { return !m_redoStack.empty(); }

    // Called after a successful save to clear the dirty flag
    void markClean();
    // Wipes history entirely (used when reloading a file)
    void clearHistory();

private:
    using PieceList = std::list<Piece>;

    struct Snapshot {
        PieceList   pieces;
        std::size_t addBufferLen; // length of add buffer at this snapshot
    };

    std::string m_original;
    std::string m_addBuffer;
    PieceList   m_pieces;

    std::vector<Snapshot> m_undoStack;
    std::vector<Snapshot> m_redoStack;
    bool                  m_dirty{false};

    // ── Internals ──────────────────────────────────────────────────────────
    [[nodiscard]] std::string_view pieceView(const Piece& p) const noexcept;

    // Splits the piece containing `offset` so that `offset` falls exactly at
    // a piece boundary.  No-op if offset is already at a boundary or past end.
    void splitAt(std::size_t offset);

    void saveSnapshot();
};

} // namespace vimith::core
