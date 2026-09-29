#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/stt/Segment.h"
#include "tpl/stt/TdtGreedyDecoder.h"
#include "tpl/stt/Vocabulary.h"

namespace tpl::stt {

/// 16 kHz samples per Parakeet encoder frame: 10 ms feature hop x subsampling factor 8.
inline constexpr std::int64_t k_samples_per_frame = 1280;

/// The words of a transcribed segment. The words are the decoded text
/// (`vocabulary.decode(token ids)`) split at its spaces, so joined with single spaces they
/// are that text without its leading space (which decode keeps, like onnx-asr, when a bare
/// word marker opens the segment), whatever the tokens: a bare marker, punctuation with or
/// without one, special tokens. A word starts at the frame of the token that produced its first
/// character, relative to `segment_start` (16 kHz samples), and ends where the next word
/// starts; the last one ends at `segment_end`. Times are clamped into the segment.
[[nodiscard]] TPL_API std::vector<Word> words_from_tokens(std::span<const Token> tokens,
                                                          const Vocabulary& vocabulary,
                                                          std::int64_t segment_start,
                                                          std::int64_t segment_end);

}  // namespace tpl::stt
