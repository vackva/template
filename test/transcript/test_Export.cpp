#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "tpl/transcript/Export.h"
#include "tpl/transcript/Types.h"

namespace {

using tpl::transcript::SessionInfo;
using tpl::transcript::StoredSegment;

std::vector<StoredSegment> two_segments() {
    return {{.m_session_id = 1,
             .m_seq = 0,
             .m_source_id = 1,
             .m_start_ms = 1'500,
             .m_end_ms = 4'250,
             .m_text = "First paragraph.",
             .m_words = {},
             .m_is_final = true},
            {.m_session_id = 1,
             .m_seq = 1,
             .m_source_id = 2,
             .m_start_ms = 25LL * 3'600'000 + 61'001,
             .m_end_ms = 25LL * 3'600'000 + 62'000,
             .m_text = "Zweiter Absatz.",
             .m_words = {},
             .m_is_final = true}};
}

TEST(Export, SrtTimeFormat) {
    EXPECT_EQ(tpl::transcript::format_srt_time(0), "00:00:00,000");
    EXPECT_EQ(tpl::transcript::format_srt_time(3'723'004), "01:02:03,004");
    EXPECT_EQ(tpl::transcript::format_srt_time(25LL * 3'600'000), "25:00:00,000");  // past 24 h
    EXPECT_EQ(tpl::transcript::format_srt_time(-5), "00:00:00,000");
}

TEST(Export, Text) {
    EXPECT_EQ(tpl::transcript::export_text(two_segments()),
              "First paragraph.\n\nZweiter Absatz.\n");
    EXPECT_EQ(tpl::transcript::export_text({}), "");
}

TEST(Export, SrtCues) {
    EXPECT_EQ(tpl::transcript::export_srt(two_segments()),
              "1\n00:00:01,500 --> 00:00:04,250\nFirst paragraph.\n"
              "\n"
              "2\n25:01:01,001 --> 25:01:02,000\nZweiter Absatz.\n");
}

TEST(Export, SkipsEmptySegments) {
    auto segments = two_segments();
    segments[0].m_text.clear();
    EXPECT_EQ(tpl::transcript::export_text(segments), "Zweiter Absatz.\n");
    EXPECT_EQ(tpl::transcript::export_srt(segments).substr(0, 2), "1\n");
}

TEST(Export, MarkdownHasTitleDateAndParagraphs) {
    const SessionInfo session{.m_id = 1, .m_title = "Standup", .m_started_utc_ms = 0};
    const auto markdown = tpl::transcript::export_markdown(session, two_segments());
    EXPECT_EQ(markdown.rfind("# Standup\n\n_", 0), 0u);
    EXPECT_NE(markdown.find("_\n\nFirst paragraph.\n\nZweiter Absatz.\n"), std::string::npos);
    EXPECT_EQ(tpl::transcript::export_markdown(session, {}).find("First"), std::string::npos);
}

TEST(Export, LocalDateTimeShape) {
    const auto text = tpl::transcript::format_local_date_time(1'790'000'000'000);  // Sep 2026
    ASSERT_EQ(text.size(), 16u);  // YYYY-MM-DD HH:MM
    EXPECT_EQ(text.substr(0, 5), "2026-");
    EXPECT_EQ(text[10], ' ');
    EXPECT_EQ(text[13], ':');
}

}  // namespace
