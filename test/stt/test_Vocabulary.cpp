#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "TestAudio.h"
#include "tpl/stt/Vocabulary.h"

namespace {

using tpl::stt::Vocabulary;

// "▁" (U+2581) marks the start of a word.
constexpr std::string_view k_marker = "\xE2\x96\x81";

Vocabulary small_vocabulary() {
    return Vocabulary({"<unk>",
                       std::string(k_marker) + "Hello",
                       ",",
                       std::string(k_marker) + "world",
                       ".",
                       std::string(k_marker) + "Kapit",
                       "\xC3\xA4n",
                       std::string(k_marker) + "\xC3\x84rger",
                       std::string(k_marker),
                       "?",
                       "<blk>"});
}

std::filesystem::path write_file(const std::string& name, const std::string& contents) {
    const auto path = tpl::stt::test::binary_dir() / name;
    std::ofstream(path, std::ios::binary) << contents;
    return path;
}

TEST(Vocabulary, BlankIsTheLastToken) {
    const auto vocabulary = small_vocabulary();
    EXPECT_EQ(vocabulary.size(), 11u);
    EXPECT_EQ(vocabulary.blank_id(), 10);
    EXPECT_EQ(vocabulary.token(10), "<blk>");
}

TEST(Vocabulary, TokenOutOfRangeThrows) {
    const auto vocabulary = small_vocabulary();
    EXPECT_THROW((void)vocabulary.token(-1), std::out_of_range);
    EXPECT_THROW((void)vocabulary.token(11), std::out_of_range);
}

TEST(Vocabulary, EmptyTokenListThrows) {
    EXPECT_THROW(Vocabulary({}), std::invalid_argument);
}

TEST(Vocabulary, DecodeJoinsWordsAndDropsLeadingSpace) {
    const std::array<std::int32_t, 4> ids{1, 2, 3, 4};
    EXPECT_EQ(small_vocabulary().decode(ids), "Hello, world.");
}

TEST(Vocabulary, DecodeKeepsSpaceBeforeNonAsciiWords) {
    const std::array<std::int32_t, 4> ids{5, 6, 7, 4};
    EXPECT_EQ(small_vocabulary().decode(ids), "Kapit\xC3\xA4n \xC3\x84rger.");
}

TEST(Vocabulary, DecodeDropsSpaceBeforePunctuationAndAtTheEnd) {
    const std::array<std::int32_t, 4> ids{1, 8, 9, 8};
    EXPECT_EQ(small_vocabulary().decode(ids), "Hello?");
}

TEST(Vocabulary, DecodeEmpty) {
    EXPECT_EQ(small_vocabulary().decode({}), "");
}

TEST(Vocabulary, LoadsTheExportedParakeetVocabulary) {
    const auto vocabulary = Vocabulary::load(tpl::stt::test::model_dir() / "vocab.txt");
    EXPECT_EQ(vocabulary.size(), 8193u);
    EXPECT_EQ(vocabulary.blank_id(), 8192);
    EXPECT_EQ(vocabulary.token(0), "<unk>");
    EXPECT_EQ(vocabulary.token(8192), "<blk>");
}

TEST(Vocabulary, LoadAcceptsCrLf) {
    const auto path = write_file("vocab_crlf.txt", "a 0\r\nb 1\r\n");
    const auto vocabulary = Vocabulary::load(path);
    EXPECT_EQ(vocabulary.token(1), "b");
}

TEST(Vocabulary, LoadRejectsBadFiles) {
    EXPECT_THROW((void)Vocabulary::load(tpl::stt::test::binary_dir() / "does_not_exist.txt"),
                 std::runtime_error);
    EXPECT_THROW((void)Vocabulary::load(write_file("vocab_empty.txt", "")), std::runtime_error);
    EXPECT_THROW((void)Vocabulary::load(write_file("vocab_no_id.txt", "token\n")),
                 std::runtime_error);
    EXPECT_THROW((void)Vocabulary::load(write_file("vocab_gap.txt", "a 0\nb 2\n")),
                 std::runtime_error);
    EXPECT_THROW((void)Vocabulary::load(write_file("vocab_text_id.txt", "a zero\n")),
                 std::runtime_error);
}

}  // namespace
