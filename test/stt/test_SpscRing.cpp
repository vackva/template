#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <thread>
#include <vector>

#include "tpl/stt/SpscRing.h"

namespace {

using tpl::stt::SpscRing;

TEST(SpscRing, CapacityRoundsUpToAPowerOfTwo) {
    EXPECT_EQ(SpscRing<float>(1000).capacity(), 1024u);
    EXPECT_EQ(SpscRing<float>(1).capacity(), 1u);
}

TEST(SpscRing, WriteThenReadInOrderAcrossTheWrap) {
    SpscRing<int> ring(8);
    std::array<int, 6> out{};
    for (int round = 0; round < 5; ++round) {
        const std::array<int, 6> in{round, round + 1, round + 2, round + 3, round + 4, round + 5};
        ASSERT_EQ(ring.write(in), 6u);
        EXPECT_EQ(ring.available(), 6u);
        ASSERT_EQ(ring.read(out), 6u);
        EXPECT_EQ(out, in);
    }
    EXPECT_EQ(ring.read(out), 0u);
}

TEST(SpscRing, DropsWhatDoesNotFit) {
    SpscRing<int> ring(4);
    const std::array<int, 6> in{1, 2, 3, 4, 5, 6};
    EXPECT_EQ(ring.write(in), 4u);
    EXPECT_EQ(ring.dropped(), 2u);
    std::array<int, 2> out{};
    EXPECT_EQ(ring.read(out), 2u);
    EXPECT_EQ(out, (std::array<int, 2>{1, 2}));
    EXPECT_EQ(ring.write(std::span(in).first(1)), 1u);
    EXPECT_EQ(ring.available(), 3u);
}

// One producer, one consumer thread: every value arrives once, in order (TSan checks races).
TEST(SpscRing, ConcurrentProducerAndConsumer) {
    constexpr int k_total = 200'000;
    SpscRing<int> ring(1024);
    std::thread producer([&] {
        std::array<int, 64> block{};
        for (int next = 0; next < k_total;) {
            std::iota(block.begin(), block.end(), next);
            // Only what fits: write() drops (and counts) the rest, as the audio thread wants.
            const auto free = ring.capacity() - ring.available();
            const auto count =
                std::min({block.size(), free, static_cast<std::size_t>(k_total - next)});
            const auto written = ring.write(std::span(block).first(count));
            next += static_cast<int>(written);
            if (written == 0) { std::this_thread::yield(); }
        }
    });
    std::vector<int> received;
    received.reserve(k_total);
    std::array<int, 100> out{};
    while (received.size() < static_cast<std::size_t>(k_total)) {
        const auto count = ring.read(out);
        received.insert(received.end(),
                        out.begin(),
                        out.begin() + static_cast<std::ptrdiff_t>(count));
        if (count == 0) { std::this_thread::yield(); }
    }
    producer.join();
    for (int i = 0; i < k_total; ++i) { ASSERT_EQ(received[static_cast<std::size_t>(i)], i); }
    EXPECT_EQ(ring.dropped(), 0u);
}

}  // namespace
