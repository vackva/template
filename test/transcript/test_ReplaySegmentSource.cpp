#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "tpl/stt/Segment.h"
#include "tpl/transcript/ReplaySegmentSource.h"

namespace {

using tpl::stt::Segment;
using tpl::transcript::ReplaySegmentSource;

std::vector<Segment> script() {
    return {
        {.m_id = 0, .m_start_sample = 0, .m_end_sample = 16'000, .m_text = "one", .m_words = {}},
        {.m_id = 0,
         .m_start_sample = 20'000,
         .m_end_sample = 32'000,
         .m_text = "two",
         .m_words = {}}};
}

void push(ReplaySegmentSource& source, std::size_t samples) {
    const std::vector<float> block(samples, 0.0f);
    source.push_audio(block);
}

TEST(ReplaySegmentSource, ReleasesSegmentsAsAudioArrives) {
    ReplaySegmentSource source(script());
    source.prepare(16000.0, 512);
    Segment out;
    EXPECT_FALSE(source.pop_segment(out));
    push(source, 15'999);
    EXPECT_FALSE(source.pop_segment(out));
    push(source, 1);
    ASSERT_TRUE(source.pop_segment(out));
    EXPECT_EQ(out.m_text, "one");
    EXPECT_EQ(out.m_id, 1u);
    EXPECT_FALSE(source.pop_segment(out));
    push(source, 16'000);
    ASSERT_TRUE(source.pop_segment(out));
    EXPECT_EQ(out.m_text, "two");
    EXPECT_EQ(out.m_id, 2u);
}

TEST(ReplaySegmentSource, ConvertsTheHostRate) {
    ReplaySegmentSource source(script());
    source.prepare(48000.0, 512);
    push(source, 48'000);  // one second at 48 kHz
    EXPECT_EQ(source.position(), 16'000);
}

TEST(ReplaySegmentSource, LatencyDelaysRelease) {
    ReplaySegmentSource source(script(), 8'000);
    source.prepare(16000.0, 512);
    push(source, 16'000);
    Segment out;
    EXPECT_FALSE(source.pop_segment(out));
    push(source, 8'000);
    EXPECT_TRUE(source.pop_segment(out));
}

TEST(ReplaySegmentSource, LoopsShiftedInTime) {
    ReplaySegmentSource source(script());
    source.prepare(16000.0, 512);
    push(source, 100'000);
    Segment out;
    ASSERT_TRUE(source.pop_segment(out));
    ASSERT_TRUE(source.pop_segment(out));
    ASSERT_TRUE(source.pop_segment(out));  // second round: loop = 32000 + 16000
    EXPECT_EQ(out.m_text, "one");
    EXPECT_EQ(out.m_start_sample, 48'000);
    EXPECT_EQ(out.m_id, 3u);
}

TEST(ReplaySegmentSource, ResetRestartsPositionsButNotIds) {
    ReplaySegmentSource source(script());
    source.prepare(16000.0, 512);
    push(source, 16'000);
    Segment out;
    ASSERT_TRUE(source.pop_segment(out));
    source.reset();
    EXPECT_EQ(source.position(), 0);
    push(source, 16'000);
    ASSERT_TRUE(source.pop_segment(out));
    EXPECT_EQ(out.m_text, "one");
    EXPECT_EQ(out.m_start_sample, 0);
    EXPECT_EQ(out.m_id, 2u);
}

TEST(ReplaySegmentSource, ResetFlushesWhatWasHeard) {
    ReplaySegmentSource source(script(), 8'000);  // latency would hold "one" back
    source.prepare(16000.0, 512);
    push(source, 24'000);  // all of "one", a third of "two"
    source.reset();
    Segment out;
    ASSERT_TRUE(source.pop_segment(out));
    EXPECT_EQ(out.m_text, "one");
    ASSERT_TRUE(source.pop_segment(out));
    EXPECT_EQ(out.m_end_sample, 24'000);  // cut off at the reset
    EXPECT_FALSE(source.pop_segment(out));
}

TEST(ReplaySegmentSource, CutOffSegmentKeepsOnlyHeardWords) {
    auto segments = tpl::transcript::ReplaySegmentSource::synthetic_script(1, 10, 10.0);
    ReplaySegmentSource source(segments);
    source.prepare(16000.0, 512);
    push(source, std::size_t{3} * 16'000);  // 3 s of 7 s speech
    source.reset();
    Segment out;
    ASSERT_TRUE(source.pop_segment(out));
    EXPECT_EQ(out.m_words.size(), 5u);  // words start every 0.7 s: 0.0 .. 2.8
    EXPECT_EQ(out.m_text.find(segments[0].m_words[5].m_text + " " + segments[0].m_words[6].m_text),
              std::string::npos);
}

TEST(ReplaySegmentSource, EmptyScriptNeverEmits) {
    ReplaySegmentSource source({});
    source.prepare(16000.0, 512);
    push(source, 1'000'000);
    Segment out;
    EXPECT_FALSE(source.pop_segment(out));
}

TEST(ReplaySegmentSource, SyntheticScriptIsDeterministicAndTimed) {
    const auto a = ReplaySegmentSource::synthetic_script(5, 10, 8.0, 7);
    const auto b = ReplaySegmentSource::synthetic_script(5, 10, 8.0, 7);
    ASSERT_EQ(a.size(), 5u);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, ReplaySegmentSource::synthetic_script(5, 10, 8.0, 8));
    EXPECT_EQ(a[1].m_start_sample, 8 * 16'000);
    EXPECT_EQ(a[1].m_end_sample - a[1].m_start_sample, 8 * 16'000 * 7 / 10);
    ASSERT_EQ(a[0].m_words.size(), 10u);
    EXPECT_TRUE(a[0].m_text.back() == '.');
    EXPECT_TRUE(a[0].m_text[0] >= 'A' && a[0].m_text[0] <= 'Z');
    EXPECT_LE(a[0].m_words.back().m_end_sample, a[0].m_end_sample);
    EXPECT_TRUE(ReplaySegmentSource::synthetic_script(3, 0).at(0).m_text.empty());
}

}  // namespace
