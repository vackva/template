#pragma once

#include <cstdint>
#include <optional>

#include "tpl/Exports.h"

namespace tpl::stt {

/// How speech probabilities become segments. All lengths in 16 kHz samples.
struct VadConfig {
    std::int64_t m_chunk = 512;         ///< samples per probability (Silero VAD)
    float m_threshold = 0.5f;           ///< speech starts at or above
    float m_off_threshold = 0.35f;      ///< ... and counts as silence below
    std::int64_t m_min_silence = 8000;  ///< 0.5 s of silence ends a segment
    std::int64_t m_min_speech = 4000;   ///< shorter segments are dropped
    std::int64_t m_padding = 6400;      ///< 0.4 s kept before and after speech
    std::int64_t m_max_segment = std::int64_t{30} * 16000;  ///< longer speech is split here
};

/// Sample range [m_start, m_end) of one segment, padding included.
struct SpeechSpan {
    std::int64_t m_start = 0;
    std::int64_t m_end = 0;

    friend bool operator==(const SpeechSpan&, const SpeechSpan&) = default;
};

/// Turns one speech probability per chunk into segments: hysteresis between the two
/// thresholds, a pause of m_min_silence ends a segment, speech longer than m_max_segment is
/// split, and a segment shorter than m_min_speech is dropped. Pure logic, no audio.
class TPL_API VadSegmenter {
public:
    explicit VadSegmenter(VadConfig config = {});

    /// The probability of the next chunk; returns a segment when one ends with it.
    std::optional<SpeechSpan> push(float probability);
    /// Ends an open segment at the current position (session stop).
    std::optional<SpeechSpan> flush();
    /// Back to position 0, no open segment.
    void reset();

    [[nodiscard]] std::int64_t position() const noexcept { return m_position; }
    [[nodiscard]] bool in_speech() const noexcept { return m_in_speech; }
    /// Earliest sample a future segment can still include; audio before it can go.
    [[nodiscard]] std::int64_t keep_from() const noexcept;
    [[nodiscard]] const VadConfig& config() const noexcept { return m_config; }

private:
    std::optional<SpeechSpan> close(std::int64_t speech_end, bool pad_end);

    VadConfig m_config;
    std::int64_t m_position = 0;
    bool m_in_speech = false;
    std::int64_t m_speech_start = 0;
    std::optional<std::int64_t> m_silence_start;
    std::int64_t m_last_end = 0;  // segments never overlap
};

}  // namespace tpl::stt
