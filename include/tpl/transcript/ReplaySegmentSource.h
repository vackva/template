#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/stt/Segment.h"

namespace tpl::transcript {

/// A SegmentSource that plays back a script instead of running a model: a scripted segment
/// is released once the audio pushed since the last reset() reaches its end (plus
/// `latency_samples`), so it follows the real audio clock. The script loops, shifted in
/// time, so it runs for as long as audio keeps coming. Stands in for the streaming
/// backend in the app's demo mode and in tests (a 24 h soak test pushes audio in large
/// blocks, far faster than real time).
class TPL_API ReplaySegmentSource final : public stt::SegmentSource {
public:
    /// `script`: segments in order, positions in 16 kHz samples; ids are reassigned.
    explicit ReplaySegmentSource(std::vector<stt::Segment> script,
                                 std::int64_t latency_samples = 0);

    void prepare(double sample_rate, int max_block_size) override;
    void push_audio(std::span<const float> mono) noexcept override;
    bool pop_segment(stt::Segment& out) override;
    void reset() override;

    /// 16 kHz samples received since the last reset().
    [[nodiscard]] std::int64_t position() const noexcept;

    /// Deterministic sentences of `words_per_segment` words, one segment every
    /// `segment_seconds` (70 % speech, 30 % pause).
    [[nodiscard]] static std::vector<stt::Segment> synthetic_script(
        std::size_t segments,
        std::size_t words_per_segment = 20,
        double segment_seconds = 8.0,
        std::uint32_t seed = 1);

private:
    stt::Segment take_next();

    std::vector<stt::Segment> m_script;
    std::int64_t m_latency_samples;
    std::int64_t m_loop_length = 0;

    std::atomic<double> m_rate_ratio{1.0};          // 16 kHz / host rate
    std::atomic<std::int64_t> m_position_x1000{0};  // milli-samples at 16 kHz

    std::mutex m_mutex;  // non-audio side only
    std::size_t m_next = 0;
    std::int64_t m_loop_offset = 0;
    std::uint64_t m_next_id = 1;
    std::deque<stt::Segment> m_flushed;  // released by reset(), popped first
};

}  // namespace tpl::transcript
