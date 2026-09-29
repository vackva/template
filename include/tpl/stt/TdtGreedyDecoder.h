#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "tpl/Exports.h"

namespace tpl::stt {

/// A token emitted by the decoder and the encoder frame it was emitted at
/// (Parakeet: 80 ms per frame).
struct Token {
    std::int32_t m_id;
    std::size_t m_frame;

    friend bool operator==(const Token&, const Token&) = default;
};

/// The prediction network + joint of a TDT transducer, evaluated one step at a time.
/// Implemented over ONNX Runtime for Parakeet and by scripted fakes in the tests.
class TPL_API TdtJoint {
public:
    TdtJoint() = default;
    TdtJoint(const TdtJoint&) = delete;
    TdtJoint& operator=(const TdtJoint&) = delete;
    TdtJoint(TdtJoint&&) = delete;
    TdtJoint& operator=(TdtJoint&&) = delete;
    virtual ~TdtJoint();

    /// Logits for encoder frame `frame`, given the previously emitted token (the blank
    /// before the first emission) and the current prediction-network state: token logits
    /// (blank included) followed by one logit per duration. The span stays valid until
    /// the next call.
    virtual std::span<const float> evaluate(std::size_t frame, std::int32_t previous_token) = 0;

    /// Adopts the prediction-network state computed by the last evaluate(). Called only
    /// when that step emitted a token; a blank leaves the state unchanged.
    virtual void accept_state() = 0;
};

/// Shape of the joint output and the limits of the greedy search.
struct TdtConfig {
    std::int32_t m_blank_id = 0;
    std::size_t m_num_tokens = 0;             ///< token logits, blank included
    std::vector<std::size_t> m_durations;     ///< frames advanced per duration logit
    std::size_t m_max_symbols_per_step = 10;  ///< emissions per frame before forcing a step
};

/// Greedy Token-and-Duration Transducer search over `num_frames` encoder frames: per step
/// the argmax token is emitted unless it is the blank, and the argmax duration advances
/// the frame. A zero duration stays on the frame, unless the token was the blank or
/// m_max_symbols_per_step tokens were emitted there; then it advances by one.
/// Throws std::invalid_argument if the joint returns the wrong number of logits.
[[nodiscard]] TPL_API std::vector<Token> tdt_greedy_decode(TdtJoint& joint,
                                                           std::size_t num_frames,
                                                           const TdtConfig& config);

}  // namespace tpl::stt
