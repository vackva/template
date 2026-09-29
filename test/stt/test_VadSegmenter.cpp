#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "tpl/stt/VadSegmenter.h"

namespace {

using tpl::stt::SpeechSpan;
using tpl::stt::VadConfig;
using tpl::stt::VadSegmenter;

// Small numbers: chunk 10, 2 chunks of silence end a segment, speech needs 3 chunks,
// padding 5, splits after 10 chunks.
VadConfig config() {
    return {.m_chunk = 10,
            .m_threshold = 0.5f,
            .m_off_threshold = 0.3f,
            .m_min_silence = 20,
            .m_min_speech = 30,
            .m_padding = 5,
            .m_max_segment = 100};
}

/// Pushes each probability; returns the spans that closed.
std::vector<SpeechSpan> feed(VadSegmenter& segmenter, const std::vector<float>& probabilities) {
    std::vector<SpeechSpan> spans;
    for (const float p : probabilities) {
        if (const auto span = segmenter.push(p)) { spans.push_back(*span); }
    }
    return spans;
}

TEST(VadSegmenter, SilenceProducesNothing) {
    VadSegmenter segmenter(config());
    EXPECT_TRUE(feed(segmenter, std::vector<float>(50, 0.1f)).empty());
    EXPECT_FALSE(segmenter.in_speech());
    EXPECT_EQ(segmenter.position(), 500);
    EXPECT_FALSE(segmenter.flush().has_value());
}

TEST(VadSegmenter, PauseEndsASegmentWithPadding) {
    VadSegmenter segmenter(config());
    // silence x2, speech x4 (20..60), silence x2 -> closes at the pause start (60).
    const auto spans = feed(segmenter, {0.0f, 0.0f, 0.9f, 0.9f, 0.9f, 0.9f, 0.1f, 0.1f});
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], (SpeechSpan{.m_start = 15, .m_end = 65}));
}

TEST(VadSegmenter, HysteresisBridgesShortDips) {
    VadSegmenter segmenter(config());
    // A dip to 0.4 (between the thresholds) and one silent chunk do not end the speech.
    const auto spans = feed(segmenter, {0.9f, 0.4f, 0.9f, 0.1f, 0.9f, 0.9f, 0.0f, 0.0f});
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].m_start, 0);  // padding clamps at 0
    EXPECT_EQ(spans[0].m_end, 65);
}

TEST(VadSegmenter, ShortBlipsAreDropped) {
    VadSegmenter segmenter(config());
    EXPECT_TRUE(feed(segmenter, {0.9f, 0.9f, 0.0f, 0.0f, 0.0f}).empty());
}

TEST(VadSegmenter, LongSpeechIsSplit) {
    VadSegmenter segmenter(config());
    const auto spans = feed(segmenter, std::vector<float>(25, 0.9f));
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], (SpeechSpan{.m_start = 0, .m_end = 100}));
    EXPECT_EQ(spans[1], (SpeechSpan{.m_start = 100, .m_end = 200}));  // no overlap, no padding
    EXPECT_TRUE(segmenter.in_speech());
    EXPECT_EQ(segmenter.flush(), (SpeechSpan{.m_start = 200, .m_end = 250}));
}

TEST(VadSegmenter, FlushEndsAtTheSilenceStart) {
    VadSegmenter segmenter(config());
    (void)feed(segmenter, {0.9f, 0.9f, 0.9f, 0.9f, 0.1f});
    EXPECT_EQ(segmenter.flush(), (SpeechSpan{.m_start = 0, .m_end = 40}));
    EXPECT_FALSE(segmenter.in_speech());
}

TEST(VadSegmenter, KeepFromTracksWhatMayStillBeNeeded) {
    VadSegmenter segmenter(config());
    (void)feed(segmenter, std::vector<float>(10, 0.0f));
    EXPECT_EQ(segmenter.keep_from(), 95);  // position 100 - padding
    (void)feed(segmenter, {0.9f, 0.9f});
    EXPECT_EQ(segmenter.keep_from(), 95);  // speech started at 100
}

TEST(VadSegmenter, ResetStartsOver) {
    VadSegmenter segmenter(config());
    (void)feed(segmenter, {0.9f, 0.9f});
    segmenter.reset();
    EXPECT_EQ(segmenter.position(), 0);
    EXPECT_FALSE(segmenter.in_speech());
    EXPECT_EQ(segmenter.config().m_chunk, 10);
}

}  // namespace
