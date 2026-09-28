#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "tpl/dsp/OnePoleLowpass.h"

namespace {

using tpl::dsp::OnePoleLowpass;

// RMS gain of the filter for a sine at @p frequency, after the transient settles.
float measure_gain(OnePoleLowpass& filter, double sample_rate, double frequency) {
    constexpr std::size_t k_length = 48000;
    std::vector<float> signal(k_length);
    for (std::size_t n = 0; n < k_length; ++n) {
        signal[n] = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(n) / sample_rate));
    }
    filter.reset();
    filter.process(signal);
    double in_power = 0.0;
    double out_power = 0.0;
    for (std::size_t n = k_length / 2; n < k_length; ++n) {
        const double x =
            std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(n) / sample_rate);
        in_power += x * x;
        out_power += static_cast<double>(signal[n]) * static_cast<double>(signal[n]);
    }
    return static_cast<float>(std::sqrt(out_power / in_power));
}

TEST(OnePoleLowpass, PassesDc) {
    OnePoleLowpass filter;
    filter.prepare(48000.0);
    filter.set_cutoff(100.0f);
    std::vector<float> block(48000, 1.0f);
    filter.process(block);
    EXPECT_NEAR(block.back(), 1.0f, 1e-4f);
}

TEST(OnePoleLowpass, AttenuatesAboveCutoff) {
    OnePoleLowpass filter;
    filter.prepare(48000.0);
    filter.set_cutoff(200.0f);
    const float low = measure_gain(filter, 48000.0, 20.0);
    const float high = measure_gain(filter, 48000.0, 5000.0);
    EXPECT_GT(low, 0.95f);
    EXPECT_LT(high, 0.1f);
}

TEST(OnePoleLowpass, CoefficientStaysStable) {
    OnePoleLowpass filter;
    filter.prepare(44100.0);
    for (const float cutoff : {-10.0f, 0.0f, 1.0f, 1000.0f, 22050.0f, 1.0e9f}) {
        filter.set_cutoff(cutoff);
        EXPECT_GT(filter.coefficient(), 0.0f) << "cutoff " << cutoff;
        EXPECT_LT(filter.coefficient(), 1.0f) << "cutoff " << cutoff;
        EXPECT_LE(filter.cutoff(), 22050.0f);
    }
}

TEST(OnePoleLowpass, ResetClearsState) {
    OnePoleLowpass filter;
    filter.prepare(48000.0);
    std::vector<float> block(64, 1.0f);
    filter.process(block);
    filter.reset();
    std::vector<float> silence(1, 0.0f);
    filter.process(silence);
    EXPECT_EQ(silence[0], 0.0f);
}

}  // namespace
