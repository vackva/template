#pragma once

#include <onnxruntime_cxx_api.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "tpl/stt/TdtGreedyDecoder.h"

namespace tpl::stt {

/// The three ONNX Runtime sessions of the exported Parakeet model: log-mel frontend
/// (nemo128.onnx), FastConformer encoder and the TDT prediction net + joint. Internal to
/// tpl_stt; Transcriber is the public face.
class OnnxParakeet {
public:
    static constexpr std::size_t k_num_durations = 5;  // TDT durations 0..4

    /// Throws std::runtime_error naming the file if one is missing, still a Git LFS
    /// pointer, or rejected by ONNX Runtime.
    OnnxParakeet(const std::filesystem::path& model_dir, std::size_t num_tokens, int num_threads);

    /// Frontend + encoder over one 16 kHz mono buffer; returns the number of valid encoder
    /// frames. The encoder output stays inside and is what joint() decodes.
    std::size_t encode(std::span<const float> samples);

    /// The decoder_joint session over the frames of the last encode(); its prediction-net
    /// state starts from zero on every encode().
    [[nodiscard]] TdtJoint& joint() noexcept { return m_joint; }

private:
    class Joint final : public TdtJoint {
    public:
        Joint(OnnxParakeet& owner, std::size_t num_logits);

        void bind(const float* encoder_out, std::size_t hidden_size, std::size_t num_frames);
        std::span<const float> evaluate(std::size_t frame, std::int32_t previous_token) override;
        void accept_state() override;

    private:
        OnnxParakeet& m_owner;
        const float* m_encoder_out = nullptr;  // [hidden, frames], channels first
        std::size_t m_hidden_size = 0;
        std::size_t m_num_frames = 0;

        std::vector<float> m_frame;
        std::int32_t m_target = 0;
        std::int32_t m_target_length = 1;
        std::array<std::vector<float>, 2> m_state_in;
        std::array<std::vector<float>, 2> m_state_out;
        std::vector<float> m_logits;

        std::array<Ort::Value, 5> m_inputs{Ort::Value{nullptr},
                                           Ort::Value{nullptr},
                                           Ort::Value{nullptr},
                                           Ort::Value{nullptr},
                                           Ort::Value{nullptr}};
        std::array<Ort::Value, 3> m_outputs{Ort::Value{nullptr},
                                            Ort::Value{nullptr},
                                            Ort::Value{nullptr}};
    };

    Ort::Env m_env;
    Ort::MemoryInfo m_memory_info;
    Ort::Session m_frontend{nullptr};
    Ort::Session m_encoder{nullptr};
    Ort::Session m_decoder_joint{nullptr};
    Ort::Value m_encoder_out{nullptr};
    Joint m_joint;
};

}  // namespace tpl::stt
