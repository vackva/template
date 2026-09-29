#include "tpl/stt/Words.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "tpl/stt/Segment.h"
#include "tpl/stt/TdtGreedyDecoder.h"
#include "tpl/stt/Vocabulary.h"

namespace tpl::stt {

std::vector<Word> words_from_tokens(std::span<const Token> tokens,
                                    const Vocabulary& vocabulary,
                                    std::int64_t segment_start,
                                    std::int64_t segment_end) {
    std::vector<std::int32_t> ids;
    ids.reserve(tokens.size());
    for (const auto& token : tokens) { ids.push_back(token.m_id); }
    const auto text = vocabulary.decode(ids);

    // decode(first k + 1 tokens) is a prefix of decode(all): appending a token only adds
    // characters (and may restore the space a prefix dropped at its end). So the token that
    // produced character c is the first k whose prefix is longer than c.
    std::vector<std::size_t> prefix_length(ids.size());
    for (std::size_t k = 0; k < ids.size(); ++k) {
        prefix_length[k] = vocabulary.decode(std::span(ids).first(k + 1)).size();
    }
    const auto token_at = [&](std::size_t character) {
        const auto found = std::ranges::upper_bound(prefix_length, character);
        return static_cast<std::size_t>(found - prefix_length.begin());
    };
    const auto start_of = [&](std::size_t token) {
        const auto frame =
            static_cast<std::int64_t>(tokens[std::min(token, tokens.size() - 1)].m_frame);
        return std::clamp(segment_start + frame * k_samples_per_frame, segment_start, segment_end);
    };

    // The text has single spaces between words, and a leading space in one case: a bare
    // marker opening the segment before a word (onnx-asr's rule only drops the very first
    // character). Empty pieces are skipped.
    std::vector<Word> words;
    std::size_t begin = 0;
    while (begin < text.size()) {
        const auto end = std::min(text.find(' ', begin), text.size());
        if (end > begin) {
            auto start = start_of(token_at(begin));
            if (!words.empty()) { start = std::max(start, words.back().m_start_sample); }
            words.push_back({.m_text = text.substr(begin, end - begin),
                             .m_start_sample = start,
                             .m_end_sample = start});
        }
        begin = end + 1;
    }
    for (std::size_t w = 0; w < words.size(); ++w) {
        words[w].m_end_sample = w + 1 < words.size() ? words[w + 1].m_start_sample : segment_end;
    }
    return words;
}

}  // namespace tpl::stt
