#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

#include "tpl/stt/Resampler.h"

namespace {

using tpl::stt::Resampler;

std::vector<float> sine(double frequency, double rate, std::size_t count, float amplitude = 0.5f) {
    std::vector<float> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency *
                                                         static_cast<double>(i) / rate));
    }
    return out;
}

/// Runs `in` through a prepared resampler in blocks of `block` samples.
std::vector<float> run(Resampler& resampler, const std::vector<float>& in, std::size_t block) {
    std::vector<float> out;
    std::vector<float> buffer(resampler.max_output(block));
    for (std::size_t start = 0; start < in.size(); start += block) {
        const auto count = std::min(block, in.size() - start);
        const auto written = resampler.process(std::span(in).subspan(start, count), buffer);
        out.insert(out.end(),
                   buffer.begin(),
                   buffer.begin() + static_cast<std::ptrdiff_t>(written));
    }
    return out;
}

double rms(std::span<const float> x) {
    double sum = 0.0;
    for (const float v : x) { sum += static_cast<double>(v) * v; }
    return x.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(x.size()));
}

TEST(Resampler, SameRateIsACopy) {
    Resampler resampler;
    resampler.prepare(16000.0, 16000.0, 64);
    EXPECT_TRUE(resampler.is_passthrough());
    const auto in = sine(440.0, 16000.0, 64);
    EXPECT_EQ(run(resampler, in, 64), in);
}

TEST(Resampler, OutputCountFollowsTheRatio) {
    for (const double rate : {48000.0, 44100.0, 96000.0, 8000.0}) {
        Resampler resampler;
        resampler.prepare(rate, 16000.0, 512);
        const auto in = sine(300.0, rate, static_cast<std::size_t>(rate * 2.0));
        const auto out = run(resampler, in, 512);
        const double expected = 2.0 * 16000.0;
        EXPECT_NEAR(static_cast<double>(out.size()), expected, 64.0) << rate;  // minus the filter
                                                                               // delay
        EXPECT_LE(out.size(), static_cast<std::size_t>(expected) + 2) << rate;
    }
}

TEST(Resampler, PassbandKeepsLevel) {
    Resampler resampler;
    resampler.prepare(48000.0, 16000.0, 480);
    const auto out = run(resampler, sine(1000.0, 48000.0, 48000), 480);
    const auto steady = std::span(out).subspan(1000, 8000);
    EXPECT_NEAR(rms(steady), 0.5 / std::numbers::sqrt2, 0.005);
}

TEST(Resampler, RejectsAboveTheNewNyquist) {
    Resampler resampler;
    resampler.prepare(48000.0, 16000.0, 480);
    // 12 kHz would alias to 4 kHz at 16 kHz; the filter must remove it (>= 50 dB).
    const auto out = run(resampler, sine(12000.0, 48000.0, 48000), 480);
    const auto level = rms(std::span(out).subspan(1000, 8000)) / (0.5 / std::numbers::sqrt2);
    EXPECT_LT(20.0 * std::log10(level), -50.0);
}

TEST(Resampler, DcGainIsOne) {
    Resampler resampler;
    resampler.prepare(44100.0, 16000.0, 256);
    const auto out = run(resampler, std::vector<float>(44100, 0.25f), 256);
    for (std::size_t i = 200; i < out.size(); ++i) { ASSERT_NEAR(out[i], 0.25f, 1e-3f) << i; }
}

TEST(Resampler, BlockSizeDoesNotChangeTheResult) {
    const auto in = sine(700.0, 48000.0, 20000);
    Resampler whole;
    whole.prepare(48000.0, 16000.0, in.size());
    const auto reference = run(whole, in, in.size());

    Resampler pieces;
    pieces.prepare(48000.0, 16000.0, 1024);
    std::vector<float> out;
    std::vector<float> buffer(pieces.max_output(1024));
    std::size_t start = 0;
    for (std::size_t block = 7; start < in.size(); block = (block * 37) % 1000 + 1) {
        const auto count = std::min(block, in.size() - start);
        const auto written = pieces.process(std::span(in).subspan(start, count), buffer);
        out.insert(out.end(),
                   buffer.begin(),
                   buffer.begin() + static_cast<std::ptrdiff_t>(written));
        start += count;
    }
    ASSERT_EQ(out.size(), reference.size());
    for (std::size_t i = 0; i < out.size(); ++i) { ASSERT_NEAR(out[i], reference[i], 1e-5f) << i; }
}

TEST(Resampler, ResetForgetsHistory) {
    Resampler resampler;
    resampler.prepare(48000.0, 16000.0, 480);
    (void)run(resampler, std::vector<float>(4800, 1.0f), 480);
    resampler.reset();
    const auto out = run(resampler, std::vector<float>(4800, 0.0f), 480);
    for (const float v : out) { ASSERT_EQ(v, 0.0f); }
}

TEST(Resampler, OversizedBlockIsCutNotOverrun) {
    Resampler resampler;
    resampler.prepare(48000.0, 16000.0, 64);
    std::vector<float> buffer(resampler.max_output(64));
    const std::vector<float> in(10'000, 0.1f);
    EXPECT_LE(resampler.process(in, buffer), buffer.size());
}

TEST(Resampler, RejectsInvalidRates) {
    Resampler resampler;
    EXPECT_THROW(resampler.prepare(0.0, 16000.0, 64), std::invalid_argument);
    EXPECT_THROW(resampler.prepare(48000.0, -1.0, 64), std::invalid_argument);
}

}  // namespace
