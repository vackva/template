#include "tpl/stt/VadSegmenter.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace tpl::stt {

VadSegmenter::VadSegmenter(VadConfig config) : m_config(config) {}

std::optional<SpeechSpan> VadSegmenter::push(float probability) {
    const auto chunk_start = m_position;
    m_position += m_config.m_chunk;

    if (!m_in_speech) {
        if (probability >= m_config.m_threshold) {
            m_in_speech = true;
            m_speech_start = chunk_start;
            m_silence_start.reset();
        }
        return std::nullopt;
    }

    if (probability < m_config.m_off_threshold) {
        if (!m_silence_start) { m_silence_start = chunk_start; }
        if (m_position - *m_silence_start >= m_config.m_min_silence) {
            return close(*m_silence_start, true);
        }
    } else {
        m_silence_start.reset();
    }

    if (m_position - m_speech_start >= m_config.m_max_segment) {
        // Split long speech: this part ends here, the next starts right after.
        auto span = close(m_position, false);
        m_in_speech = true;
        m_speech_start = m_position;
        return span;
    }
    return std::nullopt;
}

std::optional<SpeechSpan> VadSegmenter::flush() {
    if (!m_in_speech) { return std::nullopt; }
    return close(m_silence_start.value_or(m_position), false);
}

void VadSegmenter::reset() {
    m_position = 0;
    m_in_speech = false;
    m_speech_start = 0;
    m_silence_start.reset();
    m_last_end = 0;
}

std::int64_t VadSegmenter::keep_from() const noexcept {
    const auto from = m_in_speech ? m_speech_start : m_position;
    return std::max(m_last_end, from - m_config.m_padding);
}

std::optional<SpeechSpan> VadSegmenter::close(std::int64_t speech_end, bool pad_end) {
    m_in_speech = false;
    m_silence_start.reset();
    if (speech_end - m_speech_start < m_config.m_min_speech) { return std::nullopt; }
    SpeechSpan span{
        .m_start = std::max(m_last_end, m_speech_start - m_config.m_padding),
        .m_end = pad_end ? std::min(m_position, speech_end + m_config.m_padding) : speech_end};
    m_last_end = span.m_end;
    return span;
}

}  // namespace tpl::stt
