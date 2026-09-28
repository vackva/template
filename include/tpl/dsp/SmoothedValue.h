#pragma once

#include <cmath>
#include <cstddef>

namespace tpl::dsp {

/// Linear ramp from the current value to a target over a fixed time, so a
/// parameter change never jumps within one sample (zipper noise).
///
/// Real-time safe: no allocation, no locks. Call prepare() off the audio thread.
class SmoothedValue {
public:
    /// @param sample_rate  sample rate in Hz, > 0
    /// @param ramp_seconds ramp duration; 0 makes set_target() immediate
    void prepare(double sample_rate, double ramp_seconds) noexcept {
        m_ramp_samples = static_cast<std::size_t>(std::lround(sample_rate * ramp_seconds));
        snap_to_target();
    }

    /// Starts a new ramp from the current value towards @p target.
    void set_target(float target) noexcept {
        m_target = target;
        if (m_ramp_samples == 0) {
            snap_to_target();
            return;
        }
        m_remaining = m_ramp_samples;
        m_step = (m_target - m_current) / static_cast<float>(m_ramp_samples);
    }

    /// Jumps to the target; ends any ramp in progress.
    void snap_to_target() noexcept {
        m_current = m_target;
        m_remaining = 0;
        m_step = 0.0f;
    }

    /// Advances one sample and returns the new value.
    float next() noexcept {
        if (m_remaining == 0) { return m_current; }
        --m_remaining;
        m_current = (m_remaining == 0) ? m_target : m_current + m_step;
        return m_current;
    }

    [[nodiscard]] bool is_smoothing() const noexcept { return m_remaining > 0; }
    [[nodiscard]] float current() const noexcept { return m_current; }
    [[nodiscard]] float target() const noexcept { return m_target; }

private:
    float m_current = 0.0f;
    float m_target = 0.0f;
    float m_step = 0.0f;
    std::size_t m_remaining = 0;
    std::size_t m_ramp_samples = 0;
};

}  // namespace tpl::dsp
