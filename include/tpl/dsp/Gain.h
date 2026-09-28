#pragma once

#include <span>

#include "tpl/Exports.h"
#include "tpl/dsp/SmoothedValue.h"

namespace tpl::dsp {

/// Gain stage with a click-free, smoothed gain parameter.
///
/// Lifecycle: prepare() (non-real-time) -> process() (audio thread) -> reset().
class TPL_API Gain {
public:
    /// Smoothing time of a gain change.
    static constexpr double k_ramp_seconds = 0.02;

    /// Must be called before process() and whenever the sample rate changes.
    void prepare(double sample_rate) noexcept;

    /// Sets the target gain as a linear factor; ramps there over k_ramp_seconds.
    void set_gain_linear(float gain) noexcept;

    /// Sets the target gain in decibels (-inf dB and below maps to silence).
    void set_gain_db(float gain_db) noexcept;

    /// Applies the gain in place. Real-time safe.
    void process(std::span<float> block) noexcept TPL_NONBLOCKING;

    /// Ends any ramp: the gain jumps to its target.
    void reset() noexcept;

    [[nodiscard]] float gain_linear() const noexcept { return m_gain.target(); }

private:
    SmoothedValue m_gain;
};

/// Converts decibels to a linear factor; values <= k_silence_db give 0.
[[nodiscard]] TPL_API float db_to_linear(float gain_db) noexcept;

/// Anything at or below this level is treated as silence.
inline constexpr float k_silence_db = -100.0f;

}  // namespace tpl::dsp
