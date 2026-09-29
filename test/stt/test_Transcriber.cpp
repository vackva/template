#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestAudio.h"
#include "tpl/stt/Transcriber.h"

namespace {

using tpl::stt::Transcriber;
namespace test = tpl::stt::test;

nlohmann::json read_json(const std::string& name) {
    std::ifstream file(test::data_dir() / name);
    return nlohmann::json::parse(file);
}

/// One model load for the whole suite: loading the 650 MB encoder takes seconds.
class TranscriberTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_transcriber = new Transcriber({.m_model_dir = test::model_dir()});
    }
    static void TearDownTestSuite() {
        delete s_transcriber;
        s_transcriber = nullptr;
    }
    static Transcriber* s_transcriber;  // NOLINT(readability-identifier-naming)
};
Transcriber* TranscriberTest::s_transcriber = nullptr;

// Clips in clips.json against their human reference transcripts. The model makes real
// mistakes on the German clips ("seine Weltschiff", "Dinsten"), hence a WER bound.
TEST_F(TranscriberTest, WordErrorRateOnReferenceClips) {
    for (const auto& clip : read_json("clips.json")) {
        const auto file = clip["file"].get<std::string>();
        const auto transcript =
            s_transcriber->transcribe(test::read_wav_16k_mono(test::data_dir() / file));
        const double wer =
            test::word_error_rate(clip["reference"].get<std::string>(), transcript.m_text);
        const double bound = clip["language"] == "en" ? 0.05 : 0.15;
        EXPECT_LE(wer, bound) << file << ": " << transcript.m_text;
    }
}

// golden.json is the same model under onnx-asr, an independent Python implementation over
// the same ONNX graphs (ORT 1.26, CPU). int8 kernels differ slightly between CPU
// architectures and can flip a borderline token, so the comparison allows a few: at most
// 5 % of the tokens may differ, and the text (punctuation and case included) must agree
// word for word to 90 %.
TEST_F(TranscriberTest, MatchesGoldenOnnxAsrOutput) {
    for (const auto& golden : read_json("golden.json")) {
        const auto file = golden["file"].get<std::string>();
        const auto transcript =
            s_transcriber->transcribe(test::read_wav_16k_mono(test::data_dir() / file));
        const auto golden_ids = golden["token_ids"].get<std::vector<std::int32_t>>();
        const auto golden_text = golden["text"].get<std::string>();

        std::vector<std::int32_t> ids;
        ids.reserve(transcript.m_tokens.size());
        for (const auto& token : transcript.m_tokens) { ids.push_back(token.m_id); }
        const auto token_errors = test::edit_distance(golden_ids, ids);
        EXPECT_LE(static_cast<double>(token_errors), 0.05 * static_cast<double>(golden_ids.size()))
            << file << ": " << transcript.m_text << "\n golden: " << golden_text;
        EXPECT_LE(test::word_error_rate(golden_text, transcript.m_text), 0.10) << file;

        // The first token is emitted at the golden frame, give or take one (80 ms).
        ASSERT_FALSE(transcript.m_tokens.empty()) << file;
        const auto golden_first_frame = golden["frames"][0].get<std::size_t>();
        EXPECT_LE(transcript.m_tokens.front().m_frame, golden_first_frame + 1) << file;
        EXPECT_GE(transcript.m_tokens.front().m_frame + 1, golden_first_frame) << file;
    }
}

TEST_F(TranscriberTest, EmptyInputGivesEmptyTranscript) {
    const auto transcript = s_transcriber->transcribe({});
    EXPECT_TRUE(transcript.m_text.empty());
    EXPECT_TRUE(transcript.m_tokens.empty());
}

TEST_F(TranscriberTest, SilenceGivesNoWords) {
    const std::vector<float> silence(Transcriber::k_sample_rate, 0.0f);
    EXPECT_EQ(s_transcriber->transcribe(silence).m_text, "");
}

TEST_F(TranscriberTest, VeryShortInputDoesNotFail) {
    for (const std::size_t size : {1u, 159u, 160u, 400u, 1279u}) {
        const std::vector<float> samples(size, 0.1f);
        EXPECT_NO_THROW((void)s_transcriber->transcribe(samples)) << size << " samples";
    }
}

// Longer than the accurate tier's 30 s segment limit: the offline model still handles it.
TEST_F(TranscriberTest, TranscribesMoreThanThirtySeconds) {
    std::vector<float> samples;
    for (const auto& clip : read_json("clips.json")) {
        const auto audio =
            test::read_wav_16k_mono(test::data_dir() / clip["file"].get<std::string>());
        samples.insert(samples.end(), audio.begin(), audio.end());
    }
    ASSERT_GT(samples.size(), 30u * Transcriber::k_sample_rate);
    const auto transcript = s_transcriber->transcribe(samples);
    EXPECT_NE(transcript.m_text.find("Quilter"), std::string::npos) << transcript.m_text;
    EXPECT_NE(transcript.m_text.find("Lords"), std::string::npos) << transcript.m_text;
    const double last_token_seconds =
        static_cast<double>(transcript.m_tokens.back().m_frame) * Transcriber::k_seconds_per_frame;
    EXPECT_GT(last_token_seconds, 29.0);
}

TEST(Transcriber, MissingModelDirectoryThrows) {
    EXPECT_THROW(Transcriber({.m_model_dir = test::binary_dir() / "no_model_here"}),
                 std::runtime_error);
}

TEST(Transcriber, DefaultModelDirIsAbsolute) {
    EXPECT_TRUE(tpl::stt::default_model_dir().is_absolute());
}

}  // namespace
