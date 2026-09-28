#include "tpl/dsp/OnePoleLowpass.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "tpl/Exports.h"

namespace tpl::dsp {

void OnePoleLowpass::prepare(double sample_rate) noexcept {
    m_sample_rate = sample_rate;
    update_coefficient();
    reset();
}

void OnePoleLowpass::set_cutoff(float cutoff_hz) noexcept {
    const auto nyquist = static_cast<float>(m_sample_rate / 2.0);
    m_cutoff_hz = std::clamp(cutoff_hz, 1.0e-3f, nyquist);
    update_coefficient();
}

void OnePoleLowpass::process(std::span<float> block) noexcept TPL_NONBLOCKING {
    for (float& sample : block) {
        m_state += m_coefficient * (sample - m_state);
        sample = m_state;
    }
}

void OnePoleLowpass::reset() noexcept {
    m_state = 0.0f;
}

void OnePoleLowpass::update_coefficient() noexcept {
    // Matched impulse response: a = 1 - e^(-2*pi*fc/fs), always in (0, 1).
    const double omega = 2.0 * std::numbers::pi * static_cast<double>(m_cutoff_hz) / m_sample_rate;
    m_coefficient = static_cast<float>(1.0 - std::exp(-omega));
}

}  // namespace tpl::dsp
