#pragma once

#include <span>

#include "tpl/Exports.h"

namespace tpl::dsp {

/// One-pole low-pass filter, y[n] = y[n-1] + a * (x[n] - y[n-1]).
///
/// Unconditionally stable for every cutoff in (0, sample_rate / 2]; the
/// coefficient is recomputed on prepare() and set_cutoff().
class TPL_API OnePoleLowpass {
public:
    /// Must be called before process() and whenever the sample rate changes.
    void prepare(double sample_rate) noexcept;

    /// Sets the -3 dB cutoff in Hz; clamped to (0, Nyquist].
    void set_cutoff(float cutoff_hz) noexcept;

    /// Filters the block in place. Real-time safe.
    void process(std::span<float> block) noexcept TPL_NONBLOCKING;

    /// Clears the filter state (the next output starts from zero).
    void reset() noexcept;

    [[nodiscard]] float cutoff() const noexcept { return m_cutoff_hz; }
    [[nodiscard]] float coefficient() const noexcept { return m_coefficient; }

private:
    void update_coefficient() noexcept;

    double m_sample_rate = 48000.0;
    float m_cutoff_hz = 1000.0f;
    float m_coefficient = 0.0f;
    float m_state = 0.0f;
};

}  // namespace tpl::dsp
