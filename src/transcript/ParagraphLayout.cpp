#include "tpl/transcript/ParagraphLayout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpl::transcript {

std::vector<std::string> ParagraphLayout::split_words(std::string_view text) {
    std::vector<std::string> words;
    std::size_t pos = 0;
    const auto is_space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (pos < text.size()) {
        while (pos < text.size() && is_space(text[pos])) { ++pos; }
        const auto begin = pos;
        while (pos < text.size() && !is_space(text[pos])) { ++pos; }
        if (pos > begin) { words.emplace_back(text.substr(begin, pos - begin)); }
    }
    return words;
}

void ParagraphLayout::layout(std::string_view text,
                             float max_width,
                             float space_width,
                             float line_height,
                             const Measure& measure) {
    m_words.clear();
    m_lines.clear();
    m_line_height = line_height;

    float x = 0.0f;
    for (auto& text_word : split_words(text)) {
        const float width = measure(text_word);
        const bool line_empty =
            m_lines.empty() || m_lines.back().m_end_word == m_lines.back().m_first_word;
        if (m_lines.empty() || (!line_empty && x + space_width + width > max_width)) {
            const auto index = m_words.size();
            m_lines.push_back({.m_first_word = index,
                               .m_end_word = index,
                               .m_y = static_cast<float>(m_lines.size()) * line_height});
            x = 0.0f;
        } else if (!line_empty) {
            x += space_width;
        }
        m_words.push_back({.m_text = std::move(text_word),
                           .m_line = m_lines.size() - 1,
                           .m_x = x,
                           .m_width = width});
        m_lines.back().m_end_word = m_words.size();
        x += width;
    }
}

float ParagraphLayout::height() const noexcept {
    return static_cast<float>(m_lines.size()) * m_line_height;
}

std::optional<std::size_t> ParagraphLayout::word_at(float x, float y) const {
    if (m_lines.empty()) { return std::nullopt; }
    const auto row = m_line_height > 0.0f ? std::floor(y / m_line_height) : 0.0f;
    const auto line_index =
        static_cast<std::size_t>(std::clamp(row, 0.0f, static_cast<float>(m_lines.size() - 1)));
    const auto& line = m_lines[line_index];
    for (std::size_t w = line.m_first_word; w < line.m_end_word; ++w) {
        // A word owns the gap after it, up to the next word.
        const bool last = w + 1 == line.m_end_word;
        if (last || x < m_words[w + 1].m_x) { return w; }
    }
    return line.m_first_word;
}

std::string ParagraphLayout::text_between(std::size_t first, std::size_t last) const {
    if (m_words.empty()) { return {}; }
    if (first > last) { std::swap(first, last); }
    last = std::min(last, m_words.size() - 1);
    std::string out;
    for (std::size_t w = first; w <= last; ++w) {
        if (!out.empty()) { out += ' '; }
        out += m_words[w].m_text;
    }
    return out;
}

std::string ParagraphLayout::line_text(std::size_t line) const {
    if (line >= m_lines.size() || m_lines[line].m_end_word == m_lines[line].m_first_word) {
        return {};
    }
    return text_between(m_lines[line].m_first_word, m_lines[line].m_end_word - 1);
}

}  // namespace tpl::transcript
