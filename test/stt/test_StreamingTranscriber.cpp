#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "TestAudio.h"
#include "tpl/stt/Resampler.h"
#include "tpl/stt/Segment.h"
#include "tpl/stt/StreamingTranscriber.h"
#include "tpl/stt/Transcriber.h"
#include "tpl/stt/Words.h"

namespace {

using tpl::stt::Segment;
using tpl::stt::StreamingConfig;
using tpl::stt::StreamingTranscriber;
namespace test = tpl::stt::test;

constexpr double k_host_rate = 48000.0;
constexpr std::size_t k_host_block = 480;

StreamingConfig config() {
    return {.m_model_dir = test::model_dir(),
            .m_vad_model = test::vad_model(),
            .m_num_threads = 1,
            .m_vad = {},
            .m_ring_seconds = 120.0,
            .m_segment_audio = {}};
}

std::vector<float> to_host_rate(const std::vector<float>& audio_16k) {
    tpl::stt::Resampler up;
    up.prepare(16000.0, k_host_rate, audio_16k.size());
    std::vector<float> out(up.max_output(audio_16k.size()));
    out.resize(up.process(audio_16k, out));
    return out;
}

void push(StreamingTranscriber& transcriber, const std::vector<float>& host_audio) {
    for (std::size_t start = 0; start < host_audio.size(); start += k_host_block) {
        const auto count = std::min(k_host_block, host_audio.size() - start);
        transcriber.push_audio(std::span(host_audio).subspan(start, count));
    }
}

void push_silence(StreamingTranscriber& transcriber, double seconds) {
    push(transcriber, std::vector<float>(static_cast<std::size_t>(seconds * k_host_rate), 0.0f));
}

std::vector<Segment> pop_all(StreamingTranscriber& transcriber) {
    std::vector<Segment> segments;
    Segment segment;
    while (transcriber.pop_segment(segment)) { segments.push_back(segment); }
    return segments;
}

/// Models load once for the suite (about a second).
class StreamingTranscriberTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_transcriber = std::make_unique<StreamingTranscriber>(config());
        ASSERT_TRUE(s_transcriber->wait_until_loaded()) << s_transcriber->error();
        s_transcriber->prepare(k_host_rate, static_cast<int>(k_host_block));
        s_offline = std::make_unique<tpl::stt::Transcriber>(
            tpl::stt::TranscriberConfig{.m_model_dir = test::model_dir()});
    }
    static void TearDownTestSuite() {
        s_transcriber.reset();
        s_offline.reset();
    }
    void SetUp() override { s_transcriber->reset(); }

    // NOLINTNEXTLINE(readability-identifier-naming)
    static std::unique_ptr<StreamingTranscriber> s_transcriber;
    /// The offline path the streaming segments must reproduce exactly.
    // NOLINTNEXTLINE(readability-identifier-naming)
    static std::unique_ptr<tpl::stt::Transcriber> s_offline;
};
std::unique_ptr<StreamingTranscriber> StreamingTranscriberTest::s_transcriber;
std::unique_ptr<tpl::stt::Transcriber> StreamingTranscriberTest::s_offline;

/// The three test clips at 16 kHz, 1 s of silence before and 1.5 s after each.
std::vector<float> clip_stream_16k() {
    std::ifstream clips_file(test::data_dir() / "clips.json");
    std::vector<float> signal(16000, 0.0f);
    for (const auto& clip : nlohmann::json::parse(clips_file)) {
        const auto audio =
            test::read_wav_16k_mono(test::data_dir() / clip["file"].get<std::string>());
        signal.insert(signal.end(), audio.begin(), audio.end());
        signal.insert(signal.end(), 24000, 0.0f);
    }
    return signal;
}

/// Checks each streaming segment against the offline Transcriber run on exactly the samples
/// [start, end) of `signal_16k` (what the worker saw): same text, same words, same times.
void expect_matches_offline(const std::vector<Segment>& segments,
                            const std::vector<float>& signal_16k,
                            tpl::stt::Transcriber& offline) {
    for (const auto& segment : segments) {
        ASSERT_GE(segment.m_start_sample, 0);
        ASSERT_LE(segment.m_end_sample, static_cast<std::int64_t>(signal_16k.size()));
        const auto span =
            std::span(signal_16k)
                .subspan(static_cast<std::size_t>(segment.m_start_sample),
                         static_cast<std::size_t>(segment.m_end_sample - segment.m_start_sample));
        const auto transcript = offline.transcribe(span);
        const auto words = tpl::stt::words_from_tokens(transcript.m_tokens,
                                                       offline.vocabulary(),
                                                       segment.m_start_sample,
                                                       segment.m_end_sample);
        std::string text;
        for (const auto& word : words) { text += (text.empty() ? "" : " ") + word.m_text; }
        EXPECT_EQ(segment.m_text, text);
        EXPECT_EQ(segment.m_words, words) << segment.m_text;
    }
}

// English and German clips, 48 kHz, separated by pauses: one segment per clip, at the
// right place, with words, and the reference text within the offline WER bounds.
TEST_F(StreamingTranscriberTest, SegmentsAndTranscribesAStream) {
    std::ifstream clips_file(test::data_dir() / "clips.json");
    const auto clips = nlohmann::json::parse(clips_file);
    std::vector<std::int64_t> clip_starts;
    std::int64_t position = 0;
    const auto silence = [&](double seconds) {
        push_silence(*s_transcriber, seconds);
        position += static_cast<std::int64_t>(seconds * 16000.0);
    };
    silence(1.0);
    for (const auto& clip : clips) {
        const auto audio =
            test::read_wav_16k_mono(test::data_dir() / clip["file"].get<std::string>());
        clip_starts.push_back(position);
        push(*s_transcriber, to_host_rate(audio));
        position += static_cast<std::int64_t>(audio.size());
        silence(1.5);
    }
    s_transcriber->reset();  // flush
    const auto segments = pop_all(*s_transcriber);

    ASSERT_EQ(segments.size(), clips.size());
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        const auto& clip = clips[i];
        // Speech starts up to 0.8 s into a clip (the German ones open with silence).
        EXPECT_GE(segment.m_start_sample, clip_starts[i] - 16000) << i;
        EXPECT_LE(segment.m_start_sample, clip_starts[i] + 16000) << i;
        EXPECT_LT(segment.m_start_sample, segment.m_end_sample);
        EXPECT_TRUE(segment.m_is_final);
        EXPECT_GT(segment.m_id, i == 0 ? 0u : segments[i - 1].m_id);
        const double bound = clip["language"] == "en" ? 0.05 : 0.15;
        EXPECT_LE(test::word_error_rate(clip["reference"].get<std::string>(), segment.m_text),
                  bound)
            << segment.m_text;

        ASSERT_FALSE(segment.m_words.empty());
        std::string joined;
        for (const auto& word : segment.m_words) {
            EXPECT_GE(word.m_start_sample, segment.m_start_sample);
            EXPECT_LE(word.m_end_sample, segment.m_end_sample);
            EXPECT_LE(word.m_start_sample, word.m_end_sample);
            if (!joined.empty()) { joined += ' '; }
            joined += word.m_text;
        }
        EXPECT_EQ(joined, segment.m_text);
    }
    EXPECT_EQ(s_transcriber->dropped_samples(), 0u);
}

TEST_F(StreamingTranscriberTest, SilenceGivesNoSegments) {
    push_silence(*s_transcriber, 5.0);
    s_transcriber->reset();
    EXPECT_TRUE(pop_all(*s_transcriber).empty());
}

// reset() mid-speech ends the segment there, and positions start over afterwards.
TEST_F(StreamingTranscriberTest, ResetFlushesTheOpenSegmentAndRestartsPositions) {
    const auto audio = to_host_rate(
        test::read_wav_16k_mono(test::data_dir() / "en_librispeech_1272-128104-0000.wav"));
    push(*s_transcriber,
         std::vector<float>(audio.begin(),
                            audio.begin() + static_cast<std::ptrdiff_t>(audio.size() / 2)));
    s_transcriber->reset();
    auto segments = pop_all(*s_transcriber);
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_NE(segments[0].m_text.find("Quilter"), std::string::npos) << segments[0].m_text;

    push(*s_transcriber, audio);
    s_transcriber->reset();
    segments = pop_all(*s_transcriber);
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_LT(segments[0].m_start_sample, 16000);
}

// A second instance on the same model shares it: it is ready at once (no second 700 MB
// load) and transcribes like the first.
TEST_F(StreamingTranscriberTest, InstancesShareTheModel) {
    StreamingTranscriber second(config());
    ASSERT_TRUE(second.wait_until_loaded()) << second.error();
    second.prepare(k_host_rate, static_cast<int>(k_host_block));
    const auto audio = to_host_rate(
        test::read_wav_16k_mono(test::data_dir() / "en_librispeech_1272-128104-0000.wav"));
    push(second, audio);
    push(*s_transcriber, audio);
    second.reset();
    s_transcriber->reset();
    const auto a = pop_all(second);
    const auto b = pop_all(*s_transcriber);
    ASSERT_EQ(a.size(), 1u);
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(a[0].m_text, b[0].m_text);
}

/// The samples the worker handed to the model, per segment start (set up by tap_audio()).
struct AudioTap {
    std::mutex m_mutex;
    std::vector<std::pair<std::int64_t, std::vector<float>>> m_segments;
};

StreamingConfig tapped_config(AudioTap& tap) {
    auto cfg = config();
    cfg.m_segment_audio = [&tap](std::int64_t start, std::span<const float> samples) {
        const std::scoped_lock lock(tap.m_mutex);
        tap.m_segments.emplace_back(start, std::vector<float>(samples.begin(), samples.end()));
    };
    return cfg;
}

/// Bit for bit: the model got exactly signal_16k[start, start + n) for every segment.
void expect_bit_exact_audio(AudioTap& tap,
                            const std::vector<float>& signal_16k,
                            std::size_t segments) {
    const std::scoped_lock lock(tap.m_mutex);
    ASSERT_EQ(tap.m_segments.size(), segments);
    for (const auto& [start, samples] : tap.m_segments) {
        ASSERT_GE(start, 0);
        ASSERT_LE(static_cast<std::size_t>(start) + samples.size(), signal_16k.size());
        const auto expected =
            std::span(signal_16k).subspan(static_cast<std::size_t>(start), samples.size());
        ASSERT_TRUE(std::equal(samples.begin(), samples.end(), expected.begin()))
            << "segment at " << start;
    }
}

// The real-time path at the model's own rate (no resampling): host blocks of seeded random
// size (1 - 1023 samples) stress the ring, the VAD's 512-sample chunking and the trimming of
// kept audio. Every segment must be byte for byte what the offline path makes of its samples.
TEST_F(StreamingTranscriberTest, RealTimePathMatchesOfflineExactlyAt16k) {
    AudioTap tap;
    StreamingTranscriber stream(tapped_config(tap));
    ASSERT_TRUE(stream.wait_until_loaded()) << stream.error();
    stream.prepare(16000.0, 1024);
    const auto signal = clip_stream_16k();
    std::uint32_t state = 12345;
    for (std::size_t start = 0; start < signal.size();) {
        state = state * 1664525u + 1013904223u;
        const auto count = std::min<std::size_t>(1 + (state >> 8u) % 1023, signal.size() - start);
        stream.push_audio(std::span(signal).subspan(start, count));
        start += count;
    }
    stream.reset();
    const auto segments = pop_all(stream);
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(stream.dropped_samples(), 0u);
    expect_bit_exact_audio(tap, signal, segments.size());
    expect_matches_offline(segments, signal, *s_offline);
}

// The whole real-time path at a 48 kHz host rate, resampling included: the test reproduces
// the 16 kHz signal the worker sees with a Resampler fed the same blocks (bit-identical), so
// again every segment must match the offline path exactly.
TEST_F(StreamingTranscriberTest, RealTimePathMatchesOfflineExactlyAt48k) {
    AudioTap tap;
    StreamingTranscriber stream(tapped_config(tap));
    ASSERT_TRUE(stream.wait_until_loaded()) << stream.error();
    stream.prepare(k_host_rate, static_cast<int>(k_host_block));
    const auto host = to_host_rate(clip_stream_16k());

    tpl::stt::Resampler resampler;
    resampler.prepare(k_host_rate, 16000.0, k_host_block);
    std::vector<float> seen_16k;
    std::vector<float> block(resampler.max_output(k_host_block));
    for (std::size_t start = 0; start < host.size(); start += k_host_block) {
        const auto in = std::span(host).subspan(start, std::min(k_host_block, host.size() - start));
        stream.push_audio(in);
        const auto written = resampler.process(in, block);
        seen_16k.insert(seen_16k.end(),
                        block.begin(),
                        block.begin() + static_cast<std::ptrdiff_t>(written));
    }
    stream.reset();
    const auto segments = pop_all(stream);
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(stream.dropped_samples(), 0u);
    expect_bit_exact_audio(tap, seen_16k, segments.size());
    expect_matches_offline(segments, seen_16k, *s_offline);
}

TEST(StreamingTranscriber, MissingModelFailsToLoad) {
    auto cfg = config();
    cfg.m_model_dir = test::binary_dir() / "no_model_here";
    StreamingTranscriber transcriber(cfg);
    EXPECT_FALSE(transcriber.wait_until_loaded());
    EXPECT_EQ(transcriber.state(), StreamingTranscriber::State::Failed);
    EXPECT_NE(transcriber.error().find("no_model_here"), std::string::npos) << transcriber.error();
    // Still a working SegmentSource: audio is discarded, reset() returns, nothing pops.
    transcriber.prepare(k_host_rate, static_cast<int>(k_host_block));
    transcriber.push_audio(std::vector<float>(k_host_block, 0.1f));
    transcriber.reset();
    Segment segment;
    EXPECT_FALSE(transcriber.pop_segment(segment));
}

TEST(StreamingTranscriber, DefaultVadModelSitsNextToTheParakeetModel) {
    EXPECT_EQ(tpl::stt::default_vad_model().parent_path().parent_path(),
              tpl::stt::default_model_dir().parent_path());
    EXPECT_EQ(tpl::stt::default_vad_model().filename(), "silero_vad.onnx");
}

}  // namespace
