#pragma once

#include "vimith/input/key_event.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>

namespace vimith::input {

// ---------------------------------------------------------------------------
// RingBuffer<T, Capacity>
//
// Single-producer / single-consumer lock-free ring buffer.
//   - push() : called from the IO thread (producer)
//   - pop()  : called from the main thread (consumer)
//
// Uses relaxed / release-acquire orderings on head_ and tail_ atomics.
// No mutex – safe for one concurrent producer and one concurrent consumer.
// ---------------------------------------------------------------------------
template <typename T, std::size_t Capacity = 256>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
public:
    // Producer side: returns false if the buffer is full
    bool push(const T& value) noexcept {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & kMask;
        if (next == m_tail.load(std::memory_order_acquire)) return false; // full
        m_data[head] = value;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // Consumer side: returns nullopt if the buffer is empty
    std::optional<T> pop() noexcept {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) return std::nullopt;
        T value = m_data[tail];
        m_tail.store((tail + 1) & kMask, std::memory_order_release);
        return value;
    }

    [[nodiscard]] bool empty() const noexcept {
        return m_head.load(std::memory_order_acquire)
            == m_tail.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    std::array<T, Capacity>  m_data{};
    std::atomic<std::size_t> m_head{0};
    std::atomic<std::size_t> m_tail{0};
};

// ---------------------------------------------------------------------------
// InputHandler
//
// Converts ftxui Event objects into our KeyEvent struct and deposits them
// into a lock-free ring buffer.
//
// The handler integrates with ftxui's CatchEvent mechanism: the main thread
// calls feedFtxuiEvent() from within a ftxui CatchEvent lambda, which runs
// on the same thread as the event loop.  The ring buffer therefore acts as a
// staging area that decouples event arrival from processing.
//
// If you want to feed events from a raw jthread (e.g. in tests or without
// ftxui), use feedKeyEvent() directly.
// ---------------------------------------------------------------------------
class InputHandler {
public:
    InputHandler()  = default;
    ~InputHandler() = default;

    // Disable copy and move – InputHandler is typically owned by EditorState
    InputHandler(const InputHandler&)            = delete;
    InputHandler& operator=(const InputHandler&) = delete;

    // ── Event injection ────────────────────────────────────────────────────

    // Push a pre-built KeyEvent into the queue.
    // Thread-safe for one producer at a time.
    bool feedKeyEvent(const KeyEvent& ev) noexcept { return m_queue.push(ev); }

    // ── Event consumption (main / render thread) ───────────────────────────

    // Returns the next event from the queue, or nullopt if empty.
    std::optional<KeyEvent> poll() noexcept { return m_queue.pop(); }

    // ── Statistics (for debugging) ─────────────────────────────────────────
    std::size_t droppedEvents() const noexcept {
        return m_dropped.load(std::memory_order_relaxed);
    }

private:
    RingBuffer<KeyEvent, 512> m_queue;
    std::atomic<std::size_t>  m_dropped{0};
};

// ---------------------------------------------------------------------------
// ftxui → KeyEvent converter (free function, used by the Renderer's
// CatchEvent lambda)
// ---------------------------------------------------------------------------
// Forward-declared here; defined in input_handler.cpp to avoid pulling in
// the full ftxui headers everywhere.
struct FtxuiEventOpaque; // placeholder – real ftxui::Event is passed by void*

// Converts an ftxui::Event to a vimith::input::KeyEvent.
// Returns nullopt for events we don't handle (mouse, resize, etc.).
// Declared here; implementation requires ftxui headers so it lives in
// renderer.cpp where ftxui is already included.

} // namespace vimith::input
