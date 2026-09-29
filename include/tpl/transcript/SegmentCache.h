#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

/// The segments of one session as the transcript view needs them: loaded from the store a
/// page at a time around what is on screen, at most `max_pages` pages kept (least recently
/// used evicted), so a 24 h session stays small in memory. Single-threaded: use it on the
/// UI thread only.
class TPL_API SegmentCache {
public:
    explicit SegmentCache(const TranscriptStore& store,
                          std::size_t page_size = 128,
                          std::size_t max_pages = 16);

    /// Shows `session` (dropping all cached pages); nullopt shows nothing.
    void open(std::optional<SessionId> session);
    [[nodiscard]] std::optional<SessionId> session() const noexcept { return m_session; }

    /// Number of segments, as of open() and the notifications since.
    [[nodiscard]] std::size_t size() const noexcept { return m_size; }

    /// Segment `seq` (< size()), loading its page if needed. The reference stays valid
    /// until the next call on this cache.
    [[nodiscard]] const StoredSegment& at(std::size_t seq);

    /// The store changed segment `seq` of `session` (appended or revised): grows size()
    /// and drops the stale page. Other sessions are ignored.
    void segment_stored(SessionId session, std::int64_t seq);

    [[nodiscard]] std::size_t cached_pages() const noexcept { return m_pages.size(); }

private:
    const TranscriptStore& m_store;
    std::size_t m_page_size;
    std::size_t m_max_pages;
    std::optional<SessionId> m_session;
    std::size_t m_size = 0;
    std::map<std::size_t, std::vector<StoredSegment>> m_pages;  // page index -> segments
    std::list<std::size_t> m_lru;                               // most recent first
};

}  // namespace tpl::transcript
