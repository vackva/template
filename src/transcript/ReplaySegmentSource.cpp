#include "tpl/transcript/ReplaySegmentSource.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "tpl/stt/Segment.h"

namespace tpl::transcript {

ReplaySegmentSource::ReplaySegmentSource(std::vector<stt::Segment> script,
                                         std::int64_t latency_samples)
    : m_script(std::move(script)), m_latency_samples(latency_samples) {
    if (!m_script.empty()) {
        // One second of pause before the script repeats.
        m_loop_length = m_script.back().m_end_sample + stt::k_segment_sample_rate;
    }
}

void ReplaySegmentSource::prepare(double sample_rate, int /*max_block_size*/) {
    m_rate_ratio.store(
        sample_rate > 0.0 ? static_cast<double>(stt::k_segment_sample_rate) / sample_rate : 1.0);
}

void ReplaySegmentSource::push_audio(std::span<const float> mono) noexcept {
    const double samples_16k =
        static_cast<double>(mono.size()) * m_rate_ratio.load(std::memory_order_relaxed);
    m_position_x1000.fetch_add(static_cast<std::int64_t>(samples_16k * 1000.0),
                               std::memory_order_relaxed);
}

std::int64_t ReplaySegmentSource::position() const noexcept {
    return m_position_x1000.load(std::memory_order_relaxed) / 1000;
}

stt::Segment ReplaySegmentSource::take_next() {
    stt::Segment out = m_script[m_next];
    out.m_id = m_next_id++;
    out.m_start_sample += m_loop_offset;
    out.m_end_sample += m_loop_offset;
    for (auto& word : out.m_words) {
        word.m_start_sample += m_loop_offset;
        word.m_end_sample += m_loop_offset;
    }
    if (++m_next == m_script.size()) {
        m_next = 0;
        m_loop_offset += m_loop_length;
    }
    return out;
}

bool ReplaySegmentSource::pop_segment(stt::Segment& out) {
    const std::scoped_lock lock(m_mutex);
    if (!m_flushed.empty()) {
        out = std::move(m_flushed.front());
        m_flushed.pop_front();
        return true;
    }
    if (m_script.empty()) { return false; }
    const auto& scripted = m_script[m_next];
    if (scripted.m_end_sample + m_loop_offset + m_latency_samples > position()) { return false; }
    out = take_next();
    return true;
}

void ReplaySegmentSource::reset() {
    const std::scoped_lock lock(m_mutex);
    // Like a real backend's flush: everything heard so far becomes available, latency or
    // not, and a segment cut off mid-speech ends at the current position.
    const auto heard = position();
    while (!m_script.empty() && m_script[m_next].m_start_sample + m_loop_offset < heard) {
        auto segment = take_next();
        if (segment.m_end_sample > heard) {
            segment.m_end_sample = heard;
            std::erase_if(segment.m_words,
                          [heard](const stt::Word& w) { return w.m_start_sample >= heard; });
            segment.m_text.clear();
            for (const auto& word : segment.m_words) {
                if (!segment.m_text.empty()) { segment.m_text += ' '; }
                segment.m_text += word.m_text;
            }
        }
        m_flushed.push_back(std::move(segment));
    }
    m_position_x1000.store(0, std::memory_order_relaxed);
    m_next = 0;
    m_loop_offset = 0;
}

std::vector<stt::Segment> ReplaySegmentSource::synthetic_script(std::size_t segments,
                                                                std::size_t words_per_segment,
                                                                double segment_seconds,
                                                                std::uint32_t seed) {
    static constexpr std::array<const char*, 24> k_words{
        "the",     "signal",  "passes", "through", "a",      "filter",  "and",     "every",
        "sample",  "arrives", "on",     "time",    "while",  "the",     "model",   "listens",
        "quietly", "to",      "each",   "word",    "spoken", "clearly", "tonight", "again"};
    const auto segment_length =
        static_cast<std::int64_t>(segment_seconds * stt::k_segment_sample_rate);
    const auto speech_length = segment_length * 7 / 10;
    const auto word_length =
        words_per_segment > 0 ? speech_length / static_cast<std::int64_t>(words_per_segment) : 0;

    std::uint32_t state = seed == 0 ? 1 : seed;
    const auto next_random = [&state] {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return state;
    };

    std::vector<stt::Segment> script;
    script.reserve(segments);
    for (std::size_t s = 0; s < segments; ++s) {
        stt::Segment segment;
        segment.m_start_sample = static_cast<std::int64_t>(s) * segment_length;
        segment.m_end_sample = segment.m_start_sample + speech_length;
        for (std::size_t w = 0; w < words_per_segment; ++w) {
            std::string text = k_words[next_random() % k_words.size()];
            if (w == 0) { text[0] = static_cast<char>(text[0] - 'a' + 'A'); }
            if (w + 1 == words_per_segment) { text += '.'; }
            const auto start = segment.m_start_sample + static_cast<std::int64_t>(w) * word_length;
            if (!segment.m_text.empty()) { segment.m_text += ' '; }
            segment.m_text += text;
            segment.m_words.push_back({.m_text = std::move(text),
                                       .m_start_sample = start,
                                       .m_end_sample = start + word_length});
        }
        script.push_back(std::move(segment));
    }
    return script;
}

}  // namespace tpl::transcript
