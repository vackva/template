#include <gtest/gtest.h>

#include "tpl/dsp/SmoothedValue.h"

namespace {

using tpl::dsp::SmoothedValue;

TEST(SmoothedValue, ReachesTargetExactlyAfterRamp) {
    SmoothedValue value;
    value.prepare(1000.0, 0.01);  // 10-sample ramp
    value.set_target(1.0f);
    for (int i = 0; i < 9; ++i) { value.next(); }
    EXPECT_TRUE(value.is_smoothing());
    EXPECT_FLOAT_EQ(value.next(), 1.0f);
    EXPECT_FALSE(value.is_smoothing());
    EXPECT_FLOAT_EQ(value.next(), 1.0f);
}

TEST(SmoothedValue, RampIsMonotonic) {
    SmoothedValue value;
    value.prepare(1000.0, 0.01);
    value.set_target(1.0f);
    float previous = value.current();
    while (value.is_smoothing()) {
        const float current = value.next();
        EXPECT_GT(current, previous);
        previous = current;
    }
}

TEST(SmoothedValue, ZeroRampIsImmediate) {
    SmoothedValue value;
    value.prepare(48000.0, 0.0);
    value.set_target(0.5f);
    EXPECT_FALSE(value.is_smoothing());
    EXPECT_FLOAT_EQ(value.current(), 0.5f);
}

TEST(SmoothedValue, SnapEndsRamp) {
    SmoothedValue value;
    value.prepare(1000.0, 0.01);
    value.set_target(-1.0f);
    value.next();
    value.snap_to_target();
    EXPECT_FALSE(value.is_smoothing());
    EXPECT_FLOAT_EQ(value.current(), -1.0f);
}

}  // namespace
