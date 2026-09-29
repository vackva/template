#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

/// Persistent transcript database: SQLite in WAL mode with an FTS5 index, one file
/// ("transcripts.db") per storage directory. Several processes (plugin instances in
/// different hosts) may open the same file.
///
/// Thread-safe: every call is serialised on an internal mutex and runs in its own
/// transaction, so a crash loses at most the call in flight. Calls block on disk I/O —
/// never use it from an audio callback.
class TPL_API TranscriptStore {
public:
    static constexpr const char* k_file_name = "transcripts.db";

    /// Opens or creates `<directory>/transcripts.db` (creating the directory) and migrates
    /// the schema. Throws std::runtime_error if it cannot.
    explicit TranscriptStore(const std::filesystem::path& directory);
    ~TranscriptStore();
    TranscriptStore(const TranscriptStore&) = delete;
    TranscriptStore& operator=(const TranscriptStore&) = delete;
    TranscriptStore(TranscriptStore&&) = delete;
    TranscriptStore& operator=(TranscriptStore&&) = delete;

    [[nodiscard]] const std::filesystem::path& file() const noexcept;

    // --- sessions -----------------------------------------------------------------------
    SessionId create_session(const std::string& title,
                             std::int64_t started_utc_ms,
                             const std::string& language);
    void finish_session(SessionId id, std::int64_t ended_utc_ms);
    void rename_session(SessionId id, const std::string& title);
    /// Deletes the session and all its segments.
    void remove_session(SessionId id);
    /// Marks a recording session as alive; recover_abandoned_sessions() leaves it open.
    void heartbeat(SessionId id, std::int64_t now_utc_ms);

    /// Newest first.
    [[nodiscard]] std::vector<SessionInfo> list_sessions() const;
    [[nodiscard]] std::optional<SessionInfo> session(SessionId id) const;

    // --- segments -----------------------------------------------------------------------
    /// Inserts `segment` as the next paragraph of its session, or replaces the one with
    /// the same m_source_id (a revision). Ignores m_seq on input; returns the stored seq.
    std::int64_t upsert_segment(const StoredSegment& segment);
    [[nodiscard]] std::int64_t segment_count(SessionId id) const;
    /// Segments [first, first + count) of a session, clamped to what exists.
    [[nodiscard]] std::vector<StoredSegment> load_segments(SessionId id,
                                                           std::int64_t first,
                                                           std::int64_t count) const;

    // --- search and housekeeping ------------------------------------------------------------
    /// Full-text search over all sessions, newest first. Every word of `query` must match
    /// (as a prefix); case and diacritics are ignored ("kapitan" finds "Kapitän").
    [[nodiscard]] std::vector<SearchHit> search(const std::string& query, int limit) const;

    /// Deletes finished sessions that ended before `cutoff_utc_ms`; returns how many.
    int purge_finished_before(std::int64_t cutoff_utc_ms);

    /// Closes sessions left open by a crash: no heartbeat since `stale_before_utc_ms`.
    /// Their end becomes the end of their last segment. Returns how many were closed.
    int recover_abandoned_sessions(std::int64_t stale_before_utc_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace tpl::transcript
