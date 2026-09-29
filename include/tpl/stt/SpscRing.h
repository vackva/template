#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

#include "tpl/Exports.h"

namespace tpl::stt {

/// Lock-free single-producer / single-consumer ring of samples: the audio thread writes,
/// one worker thread reads. write() never blocks and never allocates; what does not fit
/// is dropped (and counted), so a stalled reader cannot stall the audio thread.
template <typename T>
class SpscRing {
public:
    /// Capacity is rounded up to a power of two. Allocates; not real-time safe.
    explicit SpscRing(std::size_t capacity) {
        std::size_t size = 1;
        while (size < capacity) { size *= 2; }
        m_buffer.assign(size, T{});
        m_mask = size - 1;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return m_buffer.size(); }

    /// Producer. Returns how many items were written.
    std::size_t write(std::span<const T> items) noexcept TPL_NONBLOCKING {
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto tail = m_tail.load(std::memory_order_acquire);
        const auto free = m_buffer.size() - (head - tail);
        const auto count = std::min(free, items.size());
        for (std::size_t i = 0; i < count; ++i) { m_buffer[(head + i) & m_mask] = items[i]; }
        m_head.store(head + count, std::memory_order_release);
        if (count < items.size()) {
            m_dropped.fetch_add(items.size() - count, std::memory_order_relaxed);
        }
        return count;
    }

    /// Consumer. Returns how many items were read into `out`.
    std::size_t read(std::span<T> out) noexcept {
        const auto tail = m_tail.load(std::memory_order_relaxed);
        const auto head = m_head.load(std::memory_order_acquire);
        const auto count = std::min(head - tail, out.size());
        for (std::size_t i = 0; i < count; ++i) { out[i] = m_buffer[(tail + i) & m_mask]; }
        m_tail.store(tail + count, std::memory_order_release);
        return count;
    }

    /// Either side: items waiting (a lower bound for the producer, exact for the consumer).
    [[nodiscard]] std::size_t available() const noexcept {
        return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_acquire);
    }

    /// Items write() had to drop since construction.
    [[nodiscard]] std::size_t dropped() const noexcept {
        return m_dropped.load(std::memory_order_relaxed);
    }

private:
    std::vector<T> m_buffer;
    std::size_t m_mask = 0;
    std::atomic<std::size_t> m_head{0};  // written by the producer
    std::atomic<std::size_t> m_tail{0};  // written by the consumer
    std::atomic<std::size_t> m_dropped{0};
};

}  // namespace tpl::stt
