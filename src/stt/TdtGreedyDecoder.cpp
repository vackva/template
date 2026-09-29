#include "tpl/stt/TdtGreedyDecoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace tpl::stt {

TdtJoint::~TdtJoint() = default;

namespace {

std::size_t argmax(std::span<const float> values) {
    return static_cast<std::size_t>(
        std::distance(values.begin(), std::ranges::max_element(values)));
}

}  // namespace

std::vector<Token> tdt_greedy_decode(TdtJoint& joint,
                                     std::size_t num_frames,
                                     const TdtConfig& config) {
    if (config.m_num_tokens == 0 || config.m_durations.empty() ||
        config.m_max_symbols_per_step == 0) {
        throw std::invalid_argument("tdt_greedy_decode: empty token or duration set");
    }
    const std::size_t expected_logits = config.m_num_tokens + config.m_durations.size();

    std::vector<Token> tokens;
    std::int32_t previous_token = config.m_blank_id;
    std::size_t frame = 0;
    std::size_t emitted_on_frame = 0;

    while (frame < num_frames) {
        const auto logits = joint.evaluate(frame, previous_token);
        if (logits.size() != expected_logits) {
            throw std::invalid_argument("tdt_greedy_decode: joint returned " +
                                        std::to_string(logits.size()) + " logits, expected " +
                                        std::to_string(expected_logits));
        }
        const auto token = static_cast<std::int32_t>(argmax(logits.first(config.m_num_tokens)));
        const auto duration = config.m_durations[argmax(logits.subspan(config.m_num_tokens))];

        if (token != config.m_blank_id) {
            joint.accept_state();
            tokens.push_back({.m_id = token, .m_frame = frame});
            previous_token = token;
            ++emitted_on_frame;
        }

        if (duration > 0) {
            frame += duration;
            emitted_on_frame = 0;
        } else if (token == config.m_blank_id ||
                   emitted_on_frame == config.m_max_symbols_per_step) {
            ++frame;
            emitted_on_frame = 0;
        }
    }
    return tokens;
}

}  // namespace tpl::stt
