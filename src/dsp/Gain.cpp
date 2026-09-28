#include "tpl/dsp/Gain.h"

#include <cmath>
#include <span>

#include "tpl/Exports.h"

namespace tpl::dsp {

float db_to_linear(float gain_db) noexcept {
    if (gain_db <= k_silence_db) { return 0.0f; }
    return std::pow(10.0f, gain_db / 20.0f);
}

void Gain::prepare(double sample_rate) noexcept {
    m_gain.prepare(sample_rate, k_ramp_seconds);
}

void Gain::set_gain_linear(float gain) noexcept {
    m_gain.set_target(gain);
}

void Gain::set_gain_db(float gain_db) noexcept {
    m_gain.set_target(db_to_linear(gain_db));
}

void Gain::process(std::span<float> block) noexcept TPL_NONBLOCKING {
    if (!m_gain.is_smoothing()) {
        const float gain = m_gain.current();
        for (float& sample : block) { sample *= gain; }
        return;
    }
    for (float& sample : block) { sample *= m_gain.next(); }
}

void Gain::reset() noexcept {
    m_gain.snap_to_target();
}

}  // namespace tpl::dsp
