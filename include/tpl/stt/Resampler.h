#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "tpl/Exports.h"

namespace tpl::stt {

/// Streaming sample-rate converter for mono audio, any ratio: windowed-sinc (Blackman,
/// 24 zero crossings) low-passed below 0.475 x the lower rate, evaluated from a polyphase
/// table with linear interpolation between phases.
///
/// prepare() allocates; process() is real-time safe and runs on the audio thread. The
/// output lags the input by the filter's half length (about 1 ms at 48 -> 16 kHz).
class TPL_API Resampler {
public:
    /// Non-real-time. `max_block` is the largest input block process() will get.
    void prepare(double input_rate, double output_rate, std::size_t max_block);

    /// Most output samples one process() call of `input` samples can produce.
    [[nodiscard]] std::size_t max_output(std::size_t input) const noexcept;

    /// Converts `in` (at most max_block samples) into `out` (at least max_output() long);
    /// returns the number of samples written. No allocation, no locks.
    std::size_t process(std::span<const float> in, std::span<float> out) noexcept TPL_NONBLOCKING;

    /// Forgets the signal history (the next block starts from silence).
    void reset() noexcept;

    [[nodiscard]] bool is_passthrough() const noexcept { return m_passthrough; }

private:
    [[nodiscard]] float sample_at(double position) const noexcept;

    bool m_passthrough = true;
    double m_step = 1.0;        // input samples per output sample
    double m_half_width = 0.0;  // kernel half length in input samples
    std::size_t m_taps = 0;     // taps per phase
    std::size_t m_phases = 0;
    std::vector<float> m_table;    // (m_phases + 1) x m_taps
    std::vector<float> m_history;  // input kept for the kernel, capacity fixed in prepare()
    std::size_t m_history_size = 0;
    double m_position = 0.0;  // next output position in m_history coordinates
    double m_ratio = 1.0;
};

}  // namespace tpl::stt
