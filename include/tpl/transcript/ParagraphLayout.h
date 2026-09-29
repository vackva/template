#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tpl/Exports.h"

namespace tpl::transcript {

/// Word-wrapped layout of one transcript paragraph, independent of any UI toolkit: the
/// caller measures words (in its font), the layout places them on lines and answers
/// which word is under a point. Word granularity is what the transcript view selects,
/// and it keeps hit-testing exact whatever the font shaping does inside a word.
class TPL_API ParagraphLayout {
public:
    using Measure = std::function<float(std::string_view word)>;

    struct Word {
        std::string m_text;
        std::size_t m_line = 0;
        float m_x = 0.0f;
        float m_width = 0.0f;
    };

    struct Line {
        std::size_t m_first_word = 0;
        std::size_t m_end_word = 0;  ///< one past the last word
        float m_y = 0.0f;            ///< top edge
    };

    /// Splits at ASCII whitespace; runs of whitespace count once.
    [[nodiscard]] static std::vector<std::string> split_words(std::string_view text);

    /// Greedy line breaking: a word that does not fit starts a new line; a word wider
    /// than `max_width` sits alone on its line.
    void layout(std::string_view text,
                float max_width,
                float space_width,
                float line_height,
                const Measure& measure);

    [[nodiscard]] float height() const noexcept;
    [[nodiscard]] const std::vector<Word>& words() const noexcept { return m_words; }
    [[nodiscard]] const std::vector<Line>& lines() const noexcept { return m_lines; }
    [[nodiscard]] float line_height() const noexcept { return m_line_height; }

    /// The word nearest to (x, y) on the line at `y` (clamped to the first/last line);
    /// nullopt for an empty paragraph.
    [[nodiscard]] std::optional<std::size_t> word_at(float x, float y) const;

    /// Text of words [first, last] (either order) joined by single spaces.
    [[nodiscard]] std::string text_between(std::size_t first, std::size_t last) const;

    /// Text of one line (words joined by single spaces), for drawing it in one call.
    [[nodiscard]] std::string line_text(std::size_t line) const;

private:
    std::vector<Word> m_words;
    std::vector<Line> m_lines;
    float m_line_height = 0.0f;
};

}  // namespace tpl::transcript
