#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "tpl/transcript/ParagraphLayout.h"

namespace {

using tpl::transcript::ParagraphLayout;

// Every byte is 10 units wide; spaces are 10 units; lines are 20 units tall.
float measure(std::string_view word) {
    return 10.0f * static_cast<float>(word.size());
}
const auto k_measure = &measure;

TEST(ParagraphLayout, SplitWords) {
    EXPECT_EQ(ParagraphLayout::split_words("  one\ttwo \n three  "),
              (std::vector<std::string>{"one", "two", "three"}));
    EXPECT_TRUE(ParagraphLayout::split_words("   ").empty());
}

TEST(ParagraphLayout, EmptyParagraph) {
    ParagraphLayout layout;
    layout.layout("", 100.0f, 10.0f, 20.0f, k_measure);
    EXPECT_FLOAT_EQ(layout.height(), 0.0f);
    EXPECT_FALSE(layout.word_at(0.0f, 0.0f).has_value());
    EXPECT_EQ(layout.text_between(0, 3), "");
    EXPECT_EQ(layout.line_text(0), "");
}

TEST(ParagraphLayout, WrapsGreedily) {
    ParagraphLayout layout;
    // "aa bb cc": aa(0-20) bb(30-50) cc would end at 80 > 70 -> next line.
    layout.layout("aa bb cc dd", 70.0f, 10.0f, 20.0f, k_measure);
    ASSERT_EQ(layout.lines().size(), 2u);
    EXPECT_EQ(layout.line_text(0), "aa bb");
    EXPECT_EQ(layout.line_text(1), "cc dd");
    EXPECT_FLOAT_EQ(layout.lines()[1].m_y, 20.0f);
    EXPECT_FLOAT_EQ(layout.words()[1].m_x, 30.0f);
    EXPECT_FLOAT_EQ(layout.words()[2].m_x, 0.0f);
    EXPECT_EQ(layout.words()[3].m_line, 1u);
    EXPECT_FLOAT_EQ(layout.height(), 40.0f);
    EXPECT_FLOAT_EQ(layout.line_height(), 20.0f);
}

TEST(ParagraphLayout, OverlongWordGetsItsOwnLine) {
    ParagraphLayout layout;
    layout.layout("a verylongword b", 50.0f, 10.0f, 20.0f, k_measure);
    ASSERT_EQ(layout.lines().size(), 3u);
    EXPECT_EQ(layout.line_text(1), "verylongword");
}

TEST(ParagraphLayout, WordAtPoint) {
    ParagraphLayout layout;
    layout.layout("aa bb cc dd", 70.0f, 10.0f, 20.0f, k_measure);
    EXPECT_EQ(layout.word_at(5.0f, 5.0f), 0u);
    EXPECT_EQ(layout.word_at(25.0f, 5.0f), 0u);  // gap after "aa"
    EXPECT_EQ(layout.word_at(30.0f, 5.0f), 1u);
    EXPECT_EQ(layout.word_at(500.0f, 5.0f), 1u);  // right of the line: its last word
    EXPECT_EQ(layout.word_at(35.0f, 25.0f), 3u);
    EXPECT_EQ(layout.word_at(-10.0f, -10.0f), 0u);  // clamped to the first line
    EXPECT_EQ(layout.word_at(0.0f, 1000.0f), 2u);   // clamped to the last line
}

TEST(ParagraphLayout, TextBetweenInEitherOrder) {
    ParagraphLayout layout;
    layout.layout("aa bb cc dd", 70.0f, 10.0f, 20.0f, k_measure);
    EXPECT_EQ(layout.text_between(1, 2), "bb cc");
    EXPECT_EQ(layout.text_between(2, 1), "bb cc");
    EXPECT_EQ(layout.text_between(3, 99), "dd");
    EXPECT_EQ(layout.line_text(5), "");
}

TEST(ParagraphLayout, RelayoutReplacesThePrevious) {
    ParagraphLayout layout;
    layout.layout("aa bb cc dd", 70.0f, 10.0f, 20.0f, k_measure);
    layout.layout("aa bb cc dd", 1000.0f, 10.0f, 30.0f, k_measure);
    EXPECT_EQ(layout.lines().size(), 1u);
    EXPECT_FLOAT_EQ(layout.height(), 30.0f);
}

}  // namespace
