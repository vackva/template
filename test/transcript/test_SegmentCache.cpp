#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "TestDir.h"
#include "tpl/transcript/SegmentCache.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

namespace {

using tpl::transcript::SegmentCache;
using tpl::transcript::SessionId;
using tpl::transcript::TranscriptStore;

void append(TranscriptStore& store,
            SessionId session,
            std::uint64_t source_id,
            const std::string& text) {
    store.upsert_segment({.m_session_id = session,
                          .m_seq = 0,
                          .m_source_id = source_id,
                          .m_start_ms = 0,
                          .m_end_ms = 1,
                          .m_text = text,
                          .m_words = {},
                          .m_is_final = true});
}

TEST(SegmentCache, EmptyUntilOpened) {
    const TranscriptStore store(tpl::transcript::test::fresh_dir());
    SegmentCache cache(store);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.session().has_value());
    EXPECT_THROW((void)cache.at(0), std::out_of_range);
}

TEST(SegmentCache, PagesInAndEvictsOldest) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    for (std::uint64_t i = 0; i < 50; ++i) { append(store, id, i, "segment " + std::to_string(i)); }

    SegmentCache cache(store, 10, 2);
    cache.open(id);
    EXPECT_EQ(cache.size(), 50u);
    EXPECT_EQ(cache.at(0).m_text, "segment 0");
    EXPECT_EQ(cache.at(49).m_text, "segment 49");
    EXPECT_EQ(cache.cached_pages(), 2u);
    EXPECT_EQ(cache.at(25).m_text, "segment 25");  // evicts page 0
    EXPECT_EQ(cache.cached_pages(), 2u);
    EXPECT_EQ(cache.at(3).m_seq, 3);
    EXPECT_THROW((void)cache.at(50), std::out_of_range);
}

TEST(SegmentCache, LiveAppendsAndRevisions) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    const auto other = store.create_session("other", 0, "en");
    SegmentCache cache(store, 4, 4);
    cache.open(id);
    EXPECT_EQ(cache.size(), 0u);

    append(store, id, 1, "first");
    cache.segment_stored(id, 0);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.at(0).m_text, "first");

    append(store, id, 1, "first, revised");  // same source id -> same seq
    cache.segment_stored(id, 0);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.at(0).m_text, "first, revised");

    append(store, other, 1, "elsewhere");
    cache.segment_stored(other, 0);
    cache.segment_stored(id, -1);
    EXPECT_EQ(cache.size(), 1u);
}

TEST(SegmentCache, ReopenSwitchesSession) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto a = store.create_session("a", 0, "en");
    const auto b = store.create_session("b", 0, "en");
    append(store, a, 1, "from a");
    append(store, b, 1, "from b");
    append(store, b, 2, "from b again");

    SegmentCache cache(store);
    cache.open(a);
    EXPECT_EQ(cache.at(0).m_text, "from a");
    cache.open(b);
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.at(0).m_text, "from b");
    cache.open(std::nullopt);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(SegmentCache, DeletedMeanwhileThrows) {
    TranscriptStore store(tpl::transcript::test::fresh_dir());
    const auto id = store.create_session("s", 0, "en");
    append(store, id, 1, "soon gone");
    SegmentCache cache(store);
    cache.open(id);
    store.remove_session(id);
    EXPECT_THROW((void)cache.at(0), std::out_of_range);
}

}  // namespace
