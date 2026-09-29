#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "TestAudio.h"
#include "tpl/stt/Segment.h"
#include "tpl/stt/TdtGreedyDecoder.h"
#include "tpl/stt/Vocabulary.h"
#include "tpl/stt/Words.h"

namespace {

using tpl::stt::k_samples_per_frame;
using tpl::stt::Token;
using tpl::stt::Vocabulary;
using tpl::stt::Word;
using tpl::stt::words_from_tokens;

const std::string k_marker = "\xE2\x96\x81";  // NOLINT(bugprone-throwing-static-initialization)

// The shapes that make joining hard in the real vocabulary: a bare marker (id 7863 there),
// marker + punctuation ("▁-", "▁¿"), punctuation without a marker, a special token.
Vocabulary small_vocabulary() {
    return Vocabulary({k_marker + "Hello",  // 0
                       ",",                 // 1
                       k_marker,            // 2
                       k_marker + "world",  // 3
                       k_marker + "-",      // 4
                       "?",                 // 5
                       "<|pnc|>",           // 6
                       k_marker + "\xC2\xBF"
                                  "Qu\xC3\xA9",  // 7  "▁¿Qué"
                       "s",                      // 8
                       "<blk>"});
}

std::vector<Token> tokens(const std::vector<std::int32_t>& ids) {
    std::vector<Token> out;
    out.reserve(ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) { out.push_back({.m_id = ids[i], .m_frame = i}); }
    return out;
}

/// `text` without the leading space decode keeps after an opening bare marker.
std::string trimmed(std::string text) {
    if (!text.empty() && text.front() == ' ') { text.erase(0, 1); }
    return text;
}

std::string joined(const std::vector<Word>& words) {
    std::string out;
    for (const auto& word : words) {
        if (!out.empty()) { out += ' '; }
        out += word.m_text;
    }
    return out;
}

TEST(Words, EmptyTokensGiveNoWords) {
    EXPECT_TRUE(words_from_tokens({}, small_vocabulary(), 0, 1000).empty());
}

TEST(Words, PunctuationJoinsThePreviousWord) {
    const auto vocabulary = small_vocabulary();
    const auto words = words_from_tokens(tokens({0, 1, 3, 5}), vocabulary, 0, 100'000);
    ASSERT_EQ(words.size(), 2u);
    EXPECT_EQ(words[0].m_text, "Hello,");
    EXPECT_EQ(words[1].m_text, "world?");
}

// A bare marker followed by punctuation: the marker's space is dropped, so the comma must not
// become a word of its own.
TEST(Words, BareMarkerBeforePunctuationStaysAttached) {
    const auto vocabulary = small_vocabulary();
    const auto ids = std::vector<std::int32_t>{0, 2, 1, 3};
    const auto words = words_from_tokens(tokens(ids), vocabulary, 0, 100'000);
    EXPECT_EQ(joined(words), vocabulary.decode(ids));
    EXPECT_EQ(joined(words), "Hello, world");
}

TEST(Words, MarkedDashAndSpecialTokens) {
    const auto vocabulary = small_vocabulary();
    for (const auto& ids : std::vector<std::vector<std::int32_t>>{{0, 4, 3},
                                                                  {6, 0, 6, 3},
                                                                  {7, 8, 5},
                                                                  {2, 2, 0},
                                                                  {1, 1, 0},
                                                                  {4},
                                                                  {2},
                                                                  {6}}) {
        const auto words = words_from_tokens(tokens(ids), vocabulary, 0, 100'000);
        EXPECT_EQ(joined(words), trimmed(vocabulary.decode(ids))) << ::testing::PrintToString(ids);
        for (const auto& word : words) { EXPECT_FALSE(word.m_text.empty()); }
    }
}

TEST(Words, OpeningBareMarkerGivesNoEmptyWord) {
    const auto vocabulary = small_vocabulary();
    const std::vector<std::int32_t> ids{2, 2, 0};
    EXPECT_EQ(vocabulary.decode(ids), " Hello");  // onnx-asr keeps this space too
    const auto words = words_from_tokens(tokens(ids), vocabulary, 0, 100'000);
    ASSERT_EQ(words.size(), 1u);
    EXPECT_EQ(words[0].m_text, "Hello");
}

TEST(Words, TimesFollowTheFirstTokenOfEachWord) {
    const auto vocabulary = small_vocabulary();
    const std::int64_t start = 16'000;
    const std::vector<Token> timed{{.m_id = 0, .m_frame = 2},
                                   {.m_id = 1, .m_frame = 3},
                                   {.m_id = 3, .m_frame = 10}};
    const auto words =
        words_from_tokens(timed, vocabulary, start, start + 20 * k_samples_per_frame);
    ASSERT_EQ(words.size(), 2u);
    EXPECT_EQ(words[0].m_start_sample, start + 2 * k_samples_per_frame);
    EXPECT_EQ(words[0].m_end_sample, start + 10 * k_samples_per_frame);  // where "world" starts
    EXPECT_EQ(words[1].m_start_sample, start + 10 * k_samples_per_frame);
    EXPECT_EQ(words[1].m_end_sample, start + 20 * k_samples_per_frame);  // the segment end
}

TEST(Words, TimesAreClampedIntoTheSegment) {
    const auto vocabulary = small_vocabulary();
    const std::vector<Token> late{{.m_id = 0, .m_frame = 0}, {.m_id = 3, .m_frame = 500}};
    const auto words = words_from_tokens(late, vocabulary, 0, 10 * k_samples_per_frame);
    ASSERT_EQ(words.size(), 2u);
    EXPECT_EQ(words[1].m_start_sample, 10 * k_samples_per_frame);
    EXPECT_EQ(words[1].m_end_sample, 10 * k_samples_per_frame);
}

// Every reference case (all 8192 tokens, random mixes of the hard ones, the real transcripts):
// the words are the decoded text split at its spaces, in order, inside the segment.
TEST(Words, InvariantsHoldForEveryReferenceCase) {
    const auto vocabulary = Vocabulary::load(tpl::stt::test::model_dir() / "vocab.txt");
    std::ifstream file(tpl::stt::test::data_dir() / "detokenize_golden.json");
    const auto cases = nlohmann::json::parse(file);
    ASSERT_GT(cases.size(), 2000u);
    const std::int64_t start = 48'000;
    for (const auto& reference : cases) {
        const auto ids = reference["ids"].get<std::vector<std::int32_t>>();
        const auto end = start + static_cast<std::int64_t>(ids.size() + 1) * k_samples_per_frame;
        const auto words = words_from_tokens(tokens(ids), vocabulary, start, end);
        ASSERT_EQ(joined(words), trimmed(reference["text"].get<std::string>()))
            << ::testing::PrintToString(ids);
        std::int64_t previous = start;
        for (const auto& word : words) {
            ASSERT_FALSE(word.m_text.empty());
            ASSERT_EQ(word.m_text.find(' '), std::string::npos);
            ASSERT_GE(word.m_start_sample, previous);
            ASSERT_LE(word.m_start_sample, word.m_end_sample);
            ASSERT_LE(word.m_end_sample, end);
            previous = word.m_start_sample;
        }
    }
}

}  // namespace
