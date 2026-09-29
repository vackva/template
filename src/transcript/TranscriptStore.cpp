#include "tpl/transcript/TranscriptStore.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Sqlite.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

namespace {

// Schema version 1. Segment times are milliseconds since their session started.
constexpr const char* k_schema = R"sql(
CREATE TABLE IF NOT EXISTS sessions (
    id              INTEGER PRIMARY KEY,
    title           TEXT    NOT NULL,
    started_utc_ms  INTEGER NOT NULL,
    ended_utc_ms    INTEGER,
    heartbeat_utc_ms INTEGER NOT NULL,
    language        TEXT    NOT NULL
);
CREATE TABLE IF NOT EXISTS segments (
    id          INTEGER PRIMARY KEY,
    session_id  INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    seq         INTEGER NOT NULL,
    source_id   INTEGER NOT NULL,
    start_ms    INTEGER NOT NULL,
    end_ms      INTEGER NOT NULL,
    text        TEXT    NOT NULL,
    words       BLOB,
    is_final    INTEGER NOT NULL,
    UNIQUE (session_id, seq),
    UNIQUE (session_id, source_id)
);
CREATE VIRTUAL TABLE IF NOT EXISTS segments_fts USING fts5(
    text, content='segments', content_rowid='id', tokenize='unicode61 remove_diacritics 2'
);
CREATE TRIGGER IF NOT EXISTS segments_ai AFTER INSERT ON segments BEGIN
    INSERT INTO segments_fts(rowid, text) VALUES (new.id, new.text);
END;
CREATE TRIGGER IF NOT EXISTS segments_ad AFTER DELETE ON segments BEGIN
    INSERT INTO segments_fts(segments_fts, rowid, text) VALUES ('delete', old.id, old.text);
END;
CREATE TRIGGER IF NOT EXISTS segments_au AFTER UPDATE OF text ON segments BEGIN
    INSERT INTO segments_fts(segments_fts, rowid, text) VALUES ('delete', old.id, old.text);
    INSERT INTO segments_fts(rowid, text) VALUES (new.id, new.text);
END;
)sql";

constexpr int k_schema_version = 1;

// Snippet markers: control characters that never occur in transcript text.
constexpr char k_mark_open = '\x01';
constexpr char k_mark_close = '\x02';

// --- words blob: per word int64 start, int64 end, uint32 length, UTF-8 bytes (little endian)

template <typename T>
void put(std::vector<std::uint8_t>& out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(
            static_cast<std::uint8_t>((static_cast<std::uint64_t>(value) >> (8 * i)) & 0xFFu));
    }
}

template <typename T>
T take(std::span<const std::uint8_t>& in) {
    if (in.size() < sizeof(T)) {
        throw std::runtime_error("tpl::transcript: truncated words blob");
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<std::uint64_t>(in[i]) << (8 * i);
    }
    in = in.subspan(sizeof(T));
    return static_cast<T>(value);
}

std::vector<std::uint8_t> encode_words(const std::vector<StoredWord>& words) {
    std::vector<std::uint8_t> out;
    for (const auto& word : words) {
        put<std::int64_t>(out, word.m_start_ms);
        put<std::int64_t>(out, word.m_end_ms);
        put<std::uint32_t>(out, static_cast<std::uint32_t>(word.m_text.size()));
        out.insert(out.end(), word.m_text.begin(), word.m_text.end());
    }
    return out;
}

std::vector<StoredWord> decode_words(std::span<const std::uint8_t> in) {
    std::vector<StoredWord> words;
    while (!in.empty()) {
        StoredWord word;
        word.m_start_ms = take<std::int64_t>(in);
        word.m_end_ms = take<std::int64_t>(in);
        const auto size = take<std::uint32_t>(in);
        if (in.size() < size) { throw std::runtime_error("tpl::transcript: truncated words blob"); }
        word.m_text.assign(reinterpret_cast<const char*>(in.data()), size);
        in = in.subspan(size);
        words.push_back(std::move(word));
    }
    return words;
}

/// Every whitespace-separated word of the user's query becomes a quoted prefix term, so
/// FTS5 syntax characters in the query are taken literally.
std::string to_fts_query(std::string_view query) {
    std::string out;
    std::size_t pos = 0;
    while (pos < query.size()) {
        while (pos < query.size() && std::isspace(static_cast<unsigned char>(query[pos])) != 0) {
            ++pos;
        }
        const std::size_t begin = pos;
        while (pos < query.size() && std::isspace(static_cast<unsigned char>(query[pos])) == 0) {
            ++pos;
        }
        if (begin == pos) { break; }
        std::string term;
        for (const char c : query.substr(begin, pos - begin)) {
            if (c == '"') { term += '"'; }
            term += c;
        }
        if (!out.empty()) { out += ' '; }
        out += '"' + term + "\"*";
    }
    return out;
}

void strip_markers(SearchHit& hit, const std::string& marked) {
    std::size_t open = 0;
    for (const char c : marked) {
        if (c == k_mark_open) {
            open = hit.m_snippet.size();
        } else if (c == k_mark_close) {
            hit.m_highlights.emplace_back(open, hit.m_snippet.size());
        } else {
            hit.m_snippet += c;
        }
    }
}

}  // namespace

namespace {

std::filesystem::path database_file(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    return directory / TranscriptStore::k_file_name;
}

}  // namespace

struct TranscriptStore::Impl {
    explicit Impl(const std::filesystem::path& directory)
        : m_file(database_file(directory)), m_db(m_file) {
        m_db.exec("PRAGMA journal_mode = WAL");
        m_db.exec("PRAGMA synchronous = NORMAL");
        m_db.exec("PRAGMA foreign_keys = ON");

        sqlite::Statement version(m_db, "PRAGMA user_version");
        version.step();
        const auto current = version.column_int(0);
        if (current > k_schema_version) {
            throw std::runtime_error("tpl::transcript: " + m_file.string() +
                                     " was written by a newer version");
        }
        if (current < k_schema_version) {
            sqlite::Transaction transaction(m_db);
            m_db.exec(k_schema);
            m_db.exec(("PRAGMA user_version = " + std::to_string(k_schema_version)).c_str());
            transaction.commit();
        }
    }

    std::filesystem::path m_file;
    mutable std::mutex m_mutex;
    mutable sqlite::Database m_db;
};

TranscriptStore::TranscriptStore(const std::filesystem::path& directory)
    : m_impl(std::make_unique<Impl>(directory)) {}

TranscriptStore::~TranscriptStore() = default;

const std::filesystem::path& TranscriptStore::file() const noexcept {
    return m_impl->m_file;
}

SessionId TranscriptStore::create_session(const std::string& title,
                                          std::int64_t started_utc_ms,
                                          const std::string& language) {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement(m_impl->m_db,
                      "INSERT INTO sessions (title, started_utc_ms, heartbeat_utc_ms, language) "
                      "VALUES (?, ?, ?, ?)")
        .bind(1, title)
        .bind(2, started_utc_ms)
        .bind(3, started_utc_ms)
        .bind(4, language)
        .run();
    return m_impl->m_db.last_insert_rowid();
}

void TranscriptStore::finish_session(SessionId id, std::int64_t ended_utc_ms) {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement(m_impl->m_db, "UPDATE sessions SET ended_utc_ms = ? WHERE id = ?")
        .bind(1, ended_utc_ms)
        .bind(2, id)
        .run();
}

void TranscriptStore::rename_session(SessionId id, const std::string& title) {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement(m_impl->m_db, "UPDATE sessions SET title = ? WHERE id = ?")
        .bind(1, title)
        .bind(2, id)
        .run();
}

void TranscriptStore::remove_session(SessionId id) {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Transaction transaction(m_impl->m_db);
    sqlite::Statement(m_impl->m_db, "DELETE FROM segments WHERE session_id = ?").bind(1, id).run();
    sqlite::Statement(m_impl->m_db, "DELETE FROM sessions WHERE id = ?").bind(1, id).run();
    transaction.commit();
}

void TranscriptStore::heartbeat(SessionId id, std::int64_t now_utc_ms) {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement(m_impl->m_db, "UPDATE sessions SET heartbeat_utc_ms = ? WHERE id = ?")
        .bind(1, now_utc_ms)
        .bind(2, id)
        .run();
}

namespace {

constexpr const char* k_select_sessions =
    "SELECT s.id, s.title, s.started_utc_ms, s.ended_utc_ms, s.language,"
    " (SELECT COUNT(*) FROM segments g WHERE g.session_id = s.id) FROM sessions s";

SessionInfo read_session(const sqlite::Statement& row) {
    return {.m_id = row.column_int(0),
            .m_title = row.column_text(1),
            .m_started_utc_ms = row.column_int(2),
            .m_ended_utc_ms = row.column_optional_int(3),
            .m_language = row.column_text(4),
            .m_segment_count = row.column_int(5)};
}

}  // namespace

std::vector<SessionInfo> TranscriptStore::list_sessions() const {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement statement(
        m_impl->m_db,
        (std::string(k_select_sessions) + " ORDER BY s.started_utc_ms DESC, s.id DESC").c_str());
    std::vector<SessionInfo> sessions;
    while (statement.step()) { sessions.push_back(read_session(statement)); }
    return sessions;
}

std::optional<SessionInfo> TranscriptStore::session(SessionId id) const {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement statement(m_impl->m_db,
                                (std::string(k_select_sessions) + " WHERE s.id = ?").c_str());
    statement.bind(1, id);
    if (!statement.step()) { return std::nullopt; }
    return read_session(statement);
}

std::int64_t TranscriptStore::upsert_segment(const StoredSegment& segment) {
    const std::scoped_lock lock(m_impl->m_mutex);
    auto& db = m_impl->m_db;
    sqlite::Transaction transaction(db);

    const auto words = encode_words(segment.m_words);
    sqlite::Statement existing(db,
                               "SELECT seq FROM segments WHERE session_id = ? AND source_id = ?");
    existing.bind(1, segment.m_session_id).bind(2, static_cast<std::int64_t>(segment.m_source_id));
    std::int64_t seq = 0;
    if (existing.step()) {
        seq = existing.column_int(0);
        sqlite::Statement(
            db,
            "UPDATE segments SET start_ms = ?, end_ms = ?, text = ?, words = ?, is_final = ?"
            " WHERE session_id = ? AND seq = ?")
            .bind(1, segment.m_start_ms)
            .bind(2, segment.m_end_ms)
            .bind(3, segment.m_text)
            .bind_blob(4, words)
            .bind(5, std::int64_t{segment.m_is_final ? 1 : 0})
            .bind(6, segment.m_session_id)
            .bind(7, seq)
            .run();
    } else {
        sqlite::Statement next(db, "SELECT COUNT(*) FROM segments WHERE session_id = ?");
        next.bind(1, segment.m_session_id);
        next.step();
        seq = next.column_int(0);
        sqlite::Statement(db,
                          "INSERT INTO segments (session_id, seq, source_id, start_ms, end_ms, "
                          "text, words, is_final)"
                          " VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
            .bind(1, segment.m_session_id)
            .bind(2, seq)
            .bind(3, static_cast<std::int64_t>(segment.m_source_id))
            .bind(4, segment.m_start_ms)
            .bind(5, segment.m_end_ms)
            .bind(6, segment.m_text)
            .bind_blob(7, words)
            .bind(8, std::int64_t{segment.m_is_final ? 1 : 0})
            .run();
    }
    transaction.commit();
    return seq;
}

std::int64_t TranscriptStore::segment_count(SessionId id) const {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement statement(m_impl->m_db, "SELECT COUNT(*) FROM segments WHERE session_id = ?");
    statement.bind(1, id);
    statement.step();
    return statement.column_int(0);
}

std::vector<StoredSegment> TranscriptStore::load_segments(SessionId id,
                                                          std::int64_t first,
                                                          std::int64_t count) const {
    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement statement(
        m_impl->m_db,
        "SELECT seq, source_id, start_ms, end_ms, text, words, is_final FROM segments"
        " WHERE session_id = ? AND seq >= ? AND seq < ? ORDER BY seq");
    statement.bind(1, id).bind(2, first).bind(3, first + count);
    std::vector<StoredSegment> segments;
    while (statement.step()) {
        const auto blob = statement.column_blob(5);
        segments.push_back({.m_session_id = id,
                            .m_seq = statement.column_int(0),
                            .m_source_id = static_cast<std::uint64_t>(statement.column_int(1)),
                            .m_start_ms = statement.column_int(2),
                            .m_end_ms = statement.column_int(3),
                            .m_text = statement.column_text(4),
                            .m_words = decode_words(blob),
                            .m_is_final = statement.column_int(6) != 0});
    }
    return segments;
}

std::vector<SearchHit> TranscriptStore::search(const std::string& query, int limit) const {
    const auto fts_query = to_fts_query(query);
    if (fts_query.empty() || limit <= 0) { return {}; }

    const std::scoped_lock lock(m_impl->m_mutex);
    sqlite::Statement statement(
        m_impl->m_db,
        "SELECT g.session_id, g.seq, s.title, s.started_utc_ms,"
        " snippet(segments_fts, 0, char(1), char(2), '…', 16)"
        " FROM segments_fts f JOIN segments g ON g.id = f.rowid"
        " JOIN sessions s ON s.id = g.session_id"
        " WHERE segments_fts MATCH ? ORDER BY s.started_utc_ms DESC, g.seq LIMIT ?");
    statement.bind(1, fts_query).bind(2, std::int64_t{limit});
    std::vector<SearchHit> hits;
    while (statement.step()) {
        SearchHit hit{.m_session_id = statement.column_int(0),
                      .m_seq = statement.column_int(1),
                      .m_session_title = statement.column_text(2),
                      .m_session_started_utc_ms = statement.column_int(3),
                      .m_snippet = {},
                      .m_highlights = {}};
        strip_markers(hit, statement.column_text(4));
        hits.push_back(std::move(hit));
    }
    return hits;
}

int TranscriptStore::purge_finished_before(std::int64_t cutoff_utc_ms) {
    const std::scoped_lock lock(m_impl->m_mutex);
    auto& db = m_impl->m_db;
    sqlite::Transaction transaction(db);
    sqlite::Statement(
        db,
        "DELETE FROM segments WHERE session_id IN"
        " (SELECT id FROM sessions WHERE ended_utc_ms IS NOT NULL AND ended_utc_ms < ?)")
        .bind(1, cutoff_utc_ms)
        .run();
    sqlite::Statement(db,
                      "DELETE FROM sessions WHERE ended_utc_ms IS NOT NULL AND ended_utc_ms < ?")
        .bind(1, cutoff_utc_ms)
        .run();
    const int removed = db.changes();
    transaction.commit();
    return removed;
}

int TranscriptStore::recover_abandoned_sessions(std::int64_t stale_before_utc_ms) {
    const std::scoped_lock lock(m_impl->m_mutex);
    auto& db = m_impl->m_db;
    sqlite::Statement(
        db,
        "UPDATE sessions SET ended_utc_ms = started_utc_ms +"
        " COALESCE((SELECT MAX(end_ms) FROM segments WHERE session_id = sessions.id), 0)"
        " WHERE ended_utc_ms IS NULL AND heartbeat_utc_ms < ?")
        .bind(1, stale_before_utc_ms)
        .run();
    return db.changes();
}

}  // namespace tpl::transcript
