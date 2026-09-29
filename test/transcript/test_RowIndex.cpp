#include <gtest/gtest.h>

#include <cstddef>
#include <random>
#include <stdexcept>
#include <vector>

#include "tpl/transcript/RowIndex.h"

namespace {

using tpl::transcript::RowIndex;

TEST(RowIndex, EmptyIndex) {
    const RowIndex index;
    EXPECT_EQ(index.size(), 0u);
    EXPECT_DOUBLE_EQ(index.total_height(), 0.0);
    EXPECT_EQ(index.row_at(100.0), 0u);
}

TEST(RowIndex, UnmeasuredRowsUseTheEstimate) {
    RowIndex index(10.0);
    index.resize(5);
    EXPECT_DOUBLE_EQ(index.total_height(), 50.0);
    EXPECT_DOUBLE_EQ(index.offset_of(3), 30.0);
    EXPECT_FALSE(index.is_measured(2));
}

TEST(RowIndex, MeasuredHeightsShiftLaterOffsets) {
    RowIndex index(10.0);
    index.resize(4);
    index.set_height(1, 25.0);
    EXPECT_TRUE(index.is_measured(1));
    EXPECT_DOUBLE_EQ(index.height(1), 25.0);
    EXPECT_DOUBLE_EQ(index.offset_of(1), 10.0);
    EXPECT_DOUBLE_EQ(index.offset_of(2), 35.0);
    EXPECT_DOUBLE_EQ(index.total_height(), 55.0);
}

TEST(RowIndex, RowAtEdges) {
    RowIndex index(10.0);
    index.resize(3);
    index.set_height(1, 20.0);  // rows: [0,10) [10,30) [30,40)
    EXPECT_EQ(index.row_at(-5.0), 0u);
    EXPECT_EQ(index.row_at(0.0), 0u);
    EXPECT_EQ(index.row_at(9.99), 0u);
    EXPECT_EQ(index.row_at(10.0), 1u);
    EXPECT_EQ(index.row_at(29.99), 1u);
    EXPECT_EQ(index.row_at(30.0), 2u);
    EXPECT_EQ(index.row_at(1e9), 2u);
}

TEST(RowIndex, GrowShrinkAndInvalidate) {
    RowIndex index(10.0);
    index.resize(2);
    index.set_height(0, 30.0);
    index.resize(6);  // appended rows keep the measured prefix
    EXPECT_DOUBLE_EQ(index.total_height(), 30.0 + 5 * 10.0);
    index.resize(1);
    EXPECT_DOUBLE_EQ(index.total_height(), 30.0);
    index.invalidate_all();
    EXPECT_DOUBLE_EQ(index.total_height(), 10.0);
    EXPECT_FALSE(index.is_measured(0));
}

TEST(RowIndex, NewEstimateAppliesOnlyToUnmeasuredRows) {
    RowIndex index(10.0);
    index.resize(3);
    index.set_height(0, 50.0);
    index.set_estimated_height(20.0);
    EXPECT_DOUBLE_EQ(index.total_height(), 90.0);
}

TEST(RowIndex, OutOfRangeThrows) {
    RowIndex index;
    index.resize(2);
    EXPECT_THROW(index.set_height(2, 1.0), std::out_of_range);
    EXPECT_THROW((void)index.height(5), std::out_of_range);
    EXPECT_FALSE(index.is_measured(5));
    EXPECT_DOUBLE_EQ(index.offset_of(99), index.total_height());
}

// Against a plain prefix sum, with appends in odd batch sizes (the live-transcript case).
TEST(RowIndex, MatchesNaivePrefixSums) {
    std::mt19937 random(42);  // NOLINT(bugprone-random-generator-seed): deterministic test
    std::uniform_real_distribution<double> heights(5.0, 200.0);
    RowIndex index(40.0);
    std::vector<double> naive;
    for (int batch = 0; batch < 40; ++batch) {
        const std::size_t grow = 1 + static_cast<std::size_t>(random() % 37);
        index.resize(index.size() + grow);
        naive.resize(naive.size() + grow, 40.0);
        for (int k = 0; k < 10; ++k) {
            const std::size_t row = random() % naive.size();
            naive[row] = heights(random);
            index.set_height(row, naive[row]);
        }
    }
    double top = 0.0;
    for (std::size_t i = 0; i < naive.size(); ++i) {
        ASSERT_NEAR(index.offset_of(i), top, 1e-6) << i;
        ASSERT_EQ(index.row_at(top + naive[i] / 2.0), i);
        top += naive[i];
    }
    EXPECT_NEAR(index.total_height(), top, 1e-6);
}

}  // namespace
