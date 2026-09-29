#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "tpl/stt/TdtGreedyDecoder.h"

namespace {

using tpl::stt::TdtConfig;
using tpl::stt::Token;

// Vocabulary of 3 tokens + blank (id 3); durations 0..4 like Parakeet.
constexpr std::int32_t k_blank = 3;

TdtConfig config() {
    return {.m_blank_id = k_blank,
            .m_num_tokens = 4,
            .m_durations = {0, 1, 2, 3, 4},
            .m_max_symbols_per_step = 10};
}

struct Step {
    std::int32_t m_token;
    std::size_t m_duration_index;
};

/// The joint's decision for one evaluate(): emit `token`, advance by durations[`duration_index`].
Step step(std::int32_t token, std::size_t duration_index) {
    return {.m_token = token, .m_duration_index = duration_index};
}

/// Plays back scripted (token, duration) decisions and records what the decoder asked for.
class ScriptedJoint final : public tpl::stt::TdtJoint {
public:
    explicit ScriptedJoint(std::vector<Step> script, Step fallback = step(k_blank, 1))
        : m_script(std::move(script)), m_fallback(fallback) {}

    std::span<const float> evaluate(std::size_t frame, std::int32_t previous_token) override {
        m_calls.emplace_back(frame, previous_token);
        const Step step = m_next < m_script.size() ? m_script[m_next++] : m_fallback;
        m_logits.assign(m_logits_size, 0.0f);
        m_logits[static_cast<std::size_t>(step.m_token)] = 1.0f;
        m_logits[4 + step.m_duration_index] = 1.0f;
        return m_logits;
    }

    void accept_state() override { ++m_accepted; }

    std::vector<std::pair<std::size_t, std::int32_t>> m_calls;
    std::size_t m_accepted = 0;
    std::size_t m_logits_size = 9;

private:
    std::vector<Step> m_script;
    Step m_fallback;
    std::size_t m_next = 0;
    std::vector<float> m_logits;
};

TEST(TdtGreedyDecoder, NoFramesMakesNoCalls) {
    ScriptedJoint joint({});
    EXPECT_TRUE(tpl::stt::tdt_greedy_decode(joint, 0, config()).empty());
    EXPECT_TRUE(joint.m_calls.empty());
}

TEST(TdtGreedyDecoder, AllBlankVisitsEveryFrameAndEmitsNothing) {
    ScriptedJoint joint({});
    EXPECT_TRUE(tpl::stt::tdt_greedy_decode(joint, 4, config()).empty());
    ASSERT_EQ(joint.m_calls.size(), 4u);
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(joint.m_calls[i], std::make_pair(i, k_blank)); }
    EXPECT_EQ(joint.m_accepted, 0u);
}

TEST(TdtGreedyDecoder, ZeroDurationStaysOnFrameAndFeedsBackTheToken) {
    ScriptedJoint joint({step(1, 0), step(2, 0), step(k_blank, 1)});
    const auto tokens = tpl::stt::tdt_greedy_decode(joint, 2, config());
    EXPECT_EQ(tokens, (std::vector<Token>{{1, 0}, {2, 0}}));
    ASSERT_GE(joint.m_calls.size(), 3u);
    EXPECT_EQ(joint.m_calls[0], std::make_pair(std::size_t{0}, k_blank));
    EXPECT_EQ(joint.m_calls[1], std::make_pair(std::size_t{0}, 1));
    EXPECT_EQ(joint.m_calls[2], std::make_pair(std::size_t{0}, 2));
    EXPECT_EQ(joint.m_accepted, 2u);
}

TEST(TdtGreedyDecoder, DurationsSkipFrames) {
    ScriptedJoint joint({step(0, 2), step(k_blank, 3), step(1, 4)});
    const auto tokens = tpl::stt::tdt_greedy_decode(joint, 9, config());
    EXPECT_EQ(tokens, (std::vector<Token>{{0, 0}, {1, 5}}));
    ASSERT_EQ(joint.m_calls.size(), 3u);
    EXPECT_EQ(joint.m_calls[1].first, 2u);
    EXPECT_EQ(joint.m_calls[2].first, 5u);
}

TEST(TdtGreedyDecoder, BlankWithZeroDurationStillAdvances) {
    ScriptedJoint joint({}, step(k_blank, 0));
    EXPECT_TRUE(tpl::stt::tdt_greedy_decode(joint, 3, config()).empty());
    EXPECT_EQ(joint.m_calls.size(), 3u);
}

TEST(TdtGreedyDecoder, MaxSymbolsPerStepForcesAStep) {
    ScriptedJoint joint({}, step(2, 0));
    auto cfg = config();
    cfg.m_max_symbols_per_step = 3;
    const auto tokens = tpl::stt::tdt_greedy_decode(joint, 2, cfg);
    EXPECT_EQ(tokens, (std::vector<Token>{{2, 0}, {2, 0}, {2, 0}, {2, 1}, {2, 1}, {2, 1}}));
}

TEST(TdtGreedyDecoder, DurationPastTheEndTerminates) {
    ScriptedJoint joint({step(1, 4)});
    const auto tokens = tpl::stt::tdt_greedy_decode(joint, 2, config());
    EXPECT_EQ(tokens, (std::vector<Token>{{1, 0}}));
    EXPECT_EQ(joint.m_calls.size(), 1u);
}

TEST(TdtGreedyDecoder, WrongLogitCountThrows) {
    ScriptedJoint joint({});
    joint.m_logits_size = 10;
    EXPECT_THROW((void)tpl::stt::tdt_greedy_decode(joint, 1, config()), std::invalid_argument);
}

TEST(TdtGreedyDecoder, EmptyConfigThrows) {
    ScriptedJoint joint({});
    auto cfg = config();
    cfg.m_durations.clear();
    EXPECT_THROW((void)tpl::stt::tdt_greedy_decode(joint, 1, cfg), std::invalid_argument);
}

}  // namespace
