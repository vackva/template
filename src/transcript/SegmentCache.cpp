#include "tpl/transcript/SegmentCache.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

SegmentCache::SegmentCache(const TranscriptStore& store,
                           std::size_t page_size,
                           std::size_t max_pages)
    : m_store(store)
    , m_page_size(std::max<std::size_t>(1, page_size))
    , m_max_pages(std::max<std::size_t>(1, max_pages)) {}

void SegmentCache::open(std::optional<SessionId> session) {
    m_session = session;
    m_pages.clear();
    m_lru.clear();
    m_size = session ? static_cast<std::size_t>(m_store.segment_count(*session)) : 0;
}

const StoredSegment& SegmentCache::at(std::size_t seq) {
    if (!m_session || seq >= m_size) { throw std::out_of_range("SegmentCache: seq out of range"); }
    const std::size_t page = seq / m_page_size;
    auto found = m_pages.find(page);
    if (found == m_pages.end()) {
        auto segments = m_store.load_segments(
            m_session.value(),
            static_cast<std::int64_t>(page) * static_cast<std::int64_t>(m_page_size),
            static_cast<std::int64_t>(m_page_size));
        found = m_pages.emplace(page, std::move(segments)).first;
        m_lru.push_front(page);
        if (m_pages.size() > m_max_pages) {
            m_pages.erase(m_lru.back());
            m_lru.pop_back();
        }
    } else {
        m_lru.remove(page);
        m_lru.push_front(page);
    }
    const auto& segments = found->second;
    const std::size_t index = seq % m_page_size;
    if (index >= segments.size()) {
        // The store has fewer rows than announced (deleted meanwhile).
        throw std::out_of_range("SegmentCache: segment missing from store");
    }
    return segments[index];
}

void SegmentCache::segment_stored(SessionId session, std::int64_t seq) {
    if (!m_session || *m_session != session || seq < 0) { return; }
    const auto position = static_cast<std::size_t>(seq);
    m_size = std::max(m_size, position + 1);
    const std::size_t page = position / m_page_size;
    if (m_pages.erase(page) > 0) { m_lru.remove(page); }
}

}  // namespace tpl::transcript
