#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestDir.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

namespace {

using tpl::transcript::test::must;

using tpl::transcript::SessionId;
using tpl::transcript::StoredSegment;
using tpl::transcript::TranscriptStore;

StoredSegment segment(SessionId session,
                      std::uint64_t source_id,
                      std::string text,
                      std::int64_t start_ms = 0) {
    return {.m_session_id = session,
            .m_seq = 0,
            .m_source_id = source_id,
            .m_start_ms = start_ms,
            .m_end_ms = start_ms + 1000,
            .m_text = std::move(text),
            .m_words = {},
            .m_is_final = true};
}

TEST(TranscriptStore, CreatesDirectoryAndFile) {
    const auto dir = tpl::transcript::test::fresh_dir() / "nested" / "store";
    const TranscriptStore store(dir);
    EXPECT_EQ(store.file(), dir / TranscriptStore::k_file_name);
    EXPECT_TRUE(std::filesystem::exists(store.file()));
}

TEST(TranscriptStore, SessionLifecycle) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("Morning", 1'000, "de");
    auto info = store.session(id);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(must(info).m_title, "Morning");
    EXPECT_EQ(must(info).m_language, "de");
    EXPECT_EQ(must(info).m_started_utc_ms, 1'000);
    EXPECT_FALSE(must(info).m_ended_utc_ms.has_value());

    store.rename_session(id, "Standup");
    store.finish_session(id, 5'000);
    info = store.session(id);
    EXPECT_EQ(must(info).m_title, "Standup");
    EXPECT_EQ(must(info).m_ended_utc_ms, 5'000);

    store.remove_session(id);
    EXPECT_FALSE(store.session(id).has_value());
    EXPECT_TRUE(store.list_sessions().empty());
}

TEST(TranscriptStore, ListsNewestFirstWithCounts) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto old_id = store.create_session("old", 100, "en");
    const auto new_id = store.create_session("new", 200, "en");
    store.upsert_segment(segment(new_id, 1, "a"));
    store.upsert_segment(segment(new_id, 2, "b"));
    const auto sessions = store.list_sessions();
    ASSERT_EQ(sessions.size(), 2u);
    EXPECT_EQ(sessions[0].m_id, new_id);
    EXPECT_EQ(sessions[0].m_segment_count, 2);
    EXPECT_EQ(sessions[1].m_id, old_id);
    EXPECT_EQ(sessions[1].m_segment_count, 0);
}

TEST(TranscriptStore, AppendsInOrderAndRoundTripsWords) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    auto first = segment(id, 10, "Hello, Kapit\xC3\xA4n.");
    first.m_words = {{.m_text = "Hello,", .m_start_ms = 0, .m_end_ms = 400},
                     {.m_text = "Kapit\xC3\xA4n.", .m_start_ms = 400, .m_end_ms = 1000}};
    EXPECT_EQ(store.upsert_segment(first), 0);
    EXPECT_EQ(store.upsert_segment(segment(id, 11, "Second", 2000)), 1);
    EXPECT_EQ(store.segment_count(id), 2);

    const auto loaded = store.load_segments(id, 0, 10);
    ASSERT_EQ(loaded.size(), 2u);
    first.m_seq = 0;
    EXPECT_EQ(loaded[0], first);
    EXPECT_EQ(loaded[1].m_seq, 1);
    EXPECT_EQ(loaded[1].m_text, "Second");
}

TEST(TranscriptStore, RevisionReplacesInPlace) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    auto provisional = segment(id, 7, "hel");
    provisional.m_is_final = false;
    EXPECT_EQ(store.upsert_segment(provisional), 0);
    EXPECT_EQ(store.upsert_segment(segment(id, 8, "next")), 1);
    EXPECT_EQ(store.upsert_segment(segment(id, 7, "hello")), 0);

    const auto loaded = store.load_segments(id, 0, 10);
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].m_text, "hello");
    EXPECT_TRUE(loaded[0].m_is_final);
    // The revision is searchable under its new text only.
    EXPECT_EQ(store.search("hello", 10).size(), 1u);
    EXPECT_TRUE(store.search("hel", 10).size() == 1u);
}

TEST(TranscriptStore, LoadSegmentsClampsTheRange) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    for (std::uint64_t i = 0; i < 5; ++i) {
        store.upsert_segment(segment(id, i, "t" + std::to_string(i)));
    }
    EXPECT_EQ(store.load_segments(id, 3, 10).size(), 2u);
    EXPECT_TRUE(store.load_segments(id, 5, 10).empty());
    EXPECT_TRUE(store.load_segments(id + 1, 0, 10).empty());
}

TEST(TranscriptStore, SearchIgnoresCaseAndDiacriticsAndMatchesPrefixes) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto a = store.create_session("A", 100, "de");
    const auto b = store.create_session("B", 200, "en");
    store.upsert_segment(segment(a, 1, "Der Kapit\xC3\xA4n f\xC3\xBChrt das Weltschiff."));
    store.upsert_segment(segment(b, 1, "The captain steers the ship."));

    auto hits = store.search("kapitan", 10);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].m_session_id, a);
    EXPECT_EQ(hits[0].m_session_title, "A");
    ASSERT_EQ(hits[0].m_highlights.size(), 1u);
    const auto [begin, end] = hits[0].m_highlights[0];
    EXPECT_EQ(hits[0].m_snippet.substr(begin, end - begin), "Kapit\xC3\xA4n");

    EXPECT_EQ(store.search("WELT", 10).size(), 1u);      // prefix of Weltschiff
    EXPECT_EQ(store.search("the ship", 10).size(), 1u);  // every word must match
    EXPECT_TRUE(store.search("ship kapit", 10).empty());
    EXPECT_TRUE(store.search("   ", 10).empty());
    EXPECT_TRUE(store.search("captain", 0).empty());
}

TEST(TranscriptStore, SearchTakesSyntaxCharactersLiterally) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    store.upsert_segment(segment(id, 1, "say \"hello\" OR goodbye"));
    EXPECT_NO_THROW((void)store.search("\"hello", 10));
    EXPECT_NO_THROW((void)store.search("OR NOT AND ( ) * :", 10));
    EXPECT_EQ(store.search("goodbye", 10).size(), 1u);
}

TEST(TranscriptStore, NewestSessionsComeFirstInSearch) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto older = store.create_session("older", 100, "en");
    const auto newer = store.create_session("newer", 200, "en");
    store.upsert_segment(segment(older, 1, "filter"));
    store.upsert_segment(segment(newer, 1, "filter"));
    const auto hits = store.search("filter", 10);
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].m_session_id, newer);
    EXPECT_EQ(store.search("filter", 1).size(), 1u);
}

TEST(TranscriptStore, RemoveSessionDropsItsSegmentsFromSearch) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    store.upsert_segment(segment(id, 1, "unique words here"));
    store.remove_session(id);
    EXPECT_TRUE(store.search("unique", 10).empty());
    EXPECT_EQ(store.segment_count(id), 0);
}

TEST(TranscriptStore, PurgeRemovesOnlyFinishedOldSessions) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto old_finished = store.create_session("old", 0, "en");
    store.finish_session(old_finished, 1'000);
    const auto recent = store.create_session("recent", 0, "en");
    store.finish_session(recent, 9'000);
    const auto recording = store.create_session("live", 0, "en");
    store.upsert_segment(segment(old_finished, 1, "gone"));

    EXPECT_EQ(store.purge_finished_before(5'000), 1);
    EXPECT_FALSE(store.session(old_finished).has_value());
    EXPECT_TRUE(store.session(recent).has_value());
    EXPECT_TRUE(store.session(recording).has_value());
    EXPECT_TRUE(store.search("gone", 10).empty());
}

TEST(TranscriptStore, RecoversOnlySessionsWithoutRecentHeartbeat) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto crashed = store.create_session("crashed", 1'000, "en");
    store.upsert_segment(segment(crashed, 1, "a", 0));
    store.upsert_segment(segment(crashed, 2, "b", 4'000));  // ends at 5 s
    const auto alive = store.create_session("alive", 1'000, "en");
    store.heartbeat(alive, 100'000);

    EXPECT_EQ(store.recover_abandoned_sessions(50'000), 1);
    EXPECT_EQ(must(store.session(crashed)).m_ended_utc_ms, 6'000);
    EXPECT_FALSE(must(store.session(alive)).m_ended_utc_ms.has_value());
    EXPECT_EQ(store.recover_abandoned_sessions(50'000), 0);
}

TEST(TranscriptStore, SecondConnectionSeesTheData) {
    const auto dir = tpl::transcript::test::fresh_dir();
    TranscriptStore writer(dir);
    const TranscriptStore reader(dir);
    const auto id = writer.create_session("shared", 0, "en");
    writer.upsert_segment(segment(id, 1, "between processes"));
    EXPECT_EQ(reader.segment_count(id), 1);
    EXPECT_EQ(reader.search("processes", 10).size(), 1u);
}

TEST(TranscriptStore, ReopenKeepsData) {
    const auto dir = tpl::transcript::test::fresh_dir();
    SessionId id = 0;
    {
        TranscriptStore store(dir);
        id = store.create_session("persisted", 0, "en");
        store.upsert_segment(segment(id, 1, "still here"));
    }
    const TranscriptStore store(dir);
    EXPECT_EQ(store.load_segments(id, 0, 1).at(0).m_text, "still here");
}

TEST(TranscriptStore, RejectsANonDatabaseFile) {
    const auto dir = tpl::transcript::test::fresh_dir();
    std::ofstream(dir / TranscriptStore::k_file_name) << "this is not a database, just text";
    EXPECT_THROW(TranscriptStore{dir}, std::runtime_error);
}

}  // namespace
