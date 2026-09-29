#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tpl::transcript {

using SessionId = std::int64_t;

/// One start/stop recording run: an entry in the sidebar.
struct SessionInfo {
    SessionId m_id = 0;
    std::string m_title;
    std::int64_t m_started_utc_ms = 0;           ///< Unix epoch, milliseconds
    std::optional<std::int64_t> m_ended_utc_ms;  ///< empty while recording
    std::string m_language;                      ///< "en", "de"
    std::int64_t m_segment_count = 0;

    friend bool operator==(const SessionInfo&, const SessionInfo&) = default;
};

/// A word with times in milliseconds since its session started.
struct StoredWord {
    std::string m_text;
    std::int64_t m_start_ms = 0;
    std::int64_t m_end_ms = 0;

    friend bool operator==(const StoredWord&, const StoredWord&) = default;
};

/// A paragraph of a session as stored: `m_seq` is its 0-based position in the session.
struct StoredSegment {
    SessionId m_session_id = 0;
    std::int64_t m_seq = 0;
    std::uint64_t m_source_id = 0;  ///< tpl::stt::Segment::m_id; a revision updates in place
    std::int64_t m_start_ms = 0;    ///< since session start
    std::int64_t m_end_ms = 0;
    std::string m_text;
    std::vector<StoredWord> m_words;
    bool m_is_final = true;

    friend bool operator==(const StoredSegment&, const StoredSegment&) = default;
};

/// A full-text search match.
struct SearchHit {
    SessionId m_session_id = 0;
    std::int64_t m_seq = 0;
    std::string m_session_title;
    std::int64_t m_session_started_utc_ms = 0;
    std::string m_snippet;  ///< excerpt around the match, without markers
    /// Byte ranges [first, second) of the matched terms inside m_snippet.
    std::vector<std::pair<std::size_t, std::size_t>> m_highlights;
};

}  // namespace tpl::transcript
