#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "tpl/dsp/Gain.h"

namespace {

using tpl::dsp::Gain;

TEST(DbToLinear, KnownValues) {
    EXPECT_FLOAT_EQ(tpl::dsp::db_to_linear(0.0f), 1.0f);
    EXPECT_NEAR(tpl::dsp::db_to_linear(-6.0206f), 0.5f, 1e-4f);
    EXPECT_NEAR(tpl::dsp::db_to_linear(20.0f), 10.0f, 1e-4f);
}

TEST(DbToLinear, SilenceFloor) {
    EXPECT_EQ(tpl::dsp::db_to_linear(tpl::dsp::k_silence_db), 0.0f);
    EXPECT_EQ(tpl::dsp::db_to_linear(-INFINITY), 0.0f);
}

TEST(Gain, UnityAfterResetLeavesSignalUntouchedButStartsAtZero) {
    Gain gain;
    gain.prepare(48000.0);
    std::array<float, 4> block{1.0f, 1.0f, 1.0f, 1.0f};
    gain.process(block);
    // Default gain is 0 (silence) until a target is set.
    for (const float sample : block) { EXPECT_EQ(sample, 0.0f); }
}

TEST(Gain, RampsWithoutJump) {
    Gain gain;
    const double sample_rate = 1000.0;
    gain.prepare(sample_rate);
    gain.set_gain_linear(1.0f);

    const auto ramp = static_cast<std::size_t>(sample_rate * Gain::k_ramp_seconds);
    std::array<float, 64> block{};
    block.fill(1.0f);
    gain.process(block);

    for (std::size_t i = 1; i < ramp; ++i) { EXPECT_GT(block[i], block[i - 1]); }
    for (std::size_t i = ramp; i < block.size(); ++i) { EXPECT_FLOAT_EQ(block[i], 1.0f); }
}

TEST(Gain, ResetJumpsToTarget) {
    Gain gain;
    gain.prepare(48000.0);
    gain.set_gain_db(-6.0206f);
    gain.reset();
    std::array<float, 2> block{1.0f, -1.0f};
    gain.process(block);
    EXPECT_NEAR(block[0], 0.5f, 1e-4f);
    EXPECT_NEAR(block[1], -0.5f, 1e-4f);
    EXPECT_NEAR(gain.gain_linear(), 0.5f, 1e-4f);
}

}  // namespace
