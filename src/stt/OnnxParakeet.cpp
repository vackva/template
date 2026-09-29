#include "OnnxParakeet.h"

#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpl::stt {

namespace {

constexpr std::array<const char*, 2> k_frontend_inputs{"waveforms", "waveforms_lens"};
constexpr std::array<const char*, 2> k_frontend_outputs{"features", "features_lens"};
constexpr std::array<const char*, 2> k_encoder_inputs{"audio_signal", "length"};
constexpr std::array<const char*, 2> k_encoder_outputs{"outputs", "encoded_lengths"};
constexpr std::array<const char*, 5> k_joint_inputs{"encoder_outputs",
                                                    "targets",
                                                    "target_length",
                                                    "input_states_1",
                                                    "input_states_2"};
constexpr std::array<const char*, 3> k_joint_outputs{"outputs",
                                                     "output_states_1",
                                                     "output_states_2"};

// Prediction network: 2-layer LSTM, hidden size 640 -> states [2, 1, 640].
constexpr std::array<std::int64_t, 3> k_state_shape{2, 1, 640};
constexpr std::size_t k_state_size = std::size_t{2} * 640;

std::filesystem::path require_model_file(const std::filesystem::path& model_dir,
                                         std::string_view name) {
    const auto path = model_dir / name;
    std::ifstream file(path, std::ios::binary);
    if (!file) { throw std::runtime_error("tpl::stt: model file missing: " + path.string()); }
    constexpr std::string_view k_lfs_pointer = "version https://git-lfs";
    std::string head(k_lfs_pointer.size(), '\0');
    file.read(head.data(), static_cast<std::streamsize>(head.size()));
    if (head == k_lfs_pointer) {
        throw std::runtime_error("tpl::stt: " + path.string() +
                                 " is a Git LFS pointer, run `git lfs pull`");
    }
    return path;
}

Ort::Session open_session(const Ort::Env& env, const std::filesystem::path& path, int num_threads) {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(num_threads);
    options.SetInterOpNumThreads(1);
    options.SetExecutionMode(ORT_SEQUENTIAL);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.AddConfigEntry("session.intra_op.allow_spinning", "0");
    try {
        return {env, path.c_str(), options};
    } catch (const Ort::Exception& e) {
        throw std::runtime_error("tpl::stt: cannot load " + path.string() + ": " + e.what());
    }
}

}  // namespace

OnnxParakeet::OnnxParakeet(const std::filesystem::path& model_dir,
                           std::size_t num_tokens,
                           int num_threads)
    : m_env(ORT_LOGGING_LEVEL_WARNING, "tpl_stt")
    , m_memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , m_joint(*this, num_tokens + k_num_durations) {
    m_frontend = open_session(m_env, require_model_file(model_dir, "nemo128.onnx"), num_threads);
    m_encoder =
        open_session(m_env, require_model_file(model_dir, "encoder-model.int8.onnx"), num_threads);
    m_decoder_joint = open_session(m_env,
                                   require_model_file(model_dir, "decoder_joint-model.int8.onnx"),
                                   num_threads);
}

std::size_t OnnxParakeet::encode(std::span<const float> samples) {
    const std::array<std::int64_t, 2> wave_shape{1, static_cast<std::int64_t>(samples.size())};
    const std::array<std::int64_t, 1> length_shape{1};
    std::int64_t wave_length = wave_shape[1];

    // ORT takes non-const buffers; inputs are never written.
    std::array<Ort::Value, 2> wave{
        Ort::Value::CreateTensor<float>(m_memory_info,
                                        const_cast<float*>(samples.data()),
                                        samples.size(),
                                        wave_shape.data(),
                                        wave_shape.size()),
        Ort::Value::CreateTensor<std::int64_t>(m_memory_info,
                                               &wave_length,
                                               1,
                                               length_shape.data(),
                                               length_shape.size())};
    auto features = m_frontend.Run(Ort::RunOptions{nullptr},
                                   k_frontend_inputs.data(),
                                   wave.data(),
                                   wave.size(),
                                   k_frontend_outputs.data(),
                                   k_frontend_outputs.size());

    auto encoded = m_encoder.Run(Ort::RunOptions{nullptr},
                                 k_encoder_inputs.data(),
                                 features.data(),
                                 features.size(),
                                 k_encoder_outputs.data(),
                                 k_encoder_outputs.size());
    m_encoder_out = std::move(encoded[0]);

    const auto shape = m_encoder_out.GetTensorTypeAndShapeInfo().GetShape();  // [1, hidden, frames]
    const auto hidden_size = static_cast<std::size_t>(shape.at(1));
    const auto stored_frames = static_cast<std::size_t>(shape.at(2));
    const auto valid_frames =
        std::min(stored_frames,
                 static_cast<std::size_t>(
                     std::max<std::int64_t>(0, *encoded[1].GetTensorData<std::int64_t>())));

    m_joint.bind(m_encoder_out.GetTensorData<float>(), hidden_size, stored_frames);
    return valid_frames;
}

OnnxParakeet::Joint::Joint(OnnxParakeet& owner, std::size_t num_logits)
    : m_owner(owner)
    , m_state_in{std::vector<float>(k_state_size), std::vector<float>(k_state_size)}
    , m_state_out{std::vector<float>(k_state_size), std::vector<float>(k_state_size)}
    , m_logits(num_logits) {}

void OnnxParakeet::Joint::bind(const float* encoder_out,
                               std::size_t hidden_size,
                               std::size_t num_frames) {
    m_encoder_out = encoder_out;
    m_hidden_size = hidden_size;
    m_num_frames = num_frames;
    m_frame.assign(hidden_size, 0.0f);
    for (auto& state : m_state_in) { std::ranges::fill(state, 0.0f); }

    const auto& memory = m_owner.m_memory_info;
    const std::array<std::int64_t, 3> frame_shape{1, static_cast<std::int64_t>(hidden_size), 1};
    const std::array<std::int64_t, 2> target_shape{1, 1};
    const std::array<std::int64_t, 1> target_length_shape{1};
    const std::array<std::int64_t, 4> logits_shape{1,
                                                   1,
                                                   1,
                                                   static_cast<std::int64_t>(m_logits.size())};

    m_inputs[0] = Ort::Value::CreateTensor<float>(memory,
                                                  m_frame.data(),
                                                  m_frame.size(),
                                                  frame_shape.data(),
                                                  frame_shape.size());
    m_inputs[1] = Ort::Value::CreateTensor<std::int32_t>(memory,
                                                         &m_target,
                                                         1,
                                                         target_shape.data(),
                                                         target_shape.size());
    m_inputs[2] = Ort::Value::CreateTensor<std::int32_t>(memory,
                                                         &m_target_length,
                                                         1,
                                                         target_length_shape.data(),
                                                         target_length_shape.size());
    for (std::size_t i = 0; i < 2; ++i) {
        m_inputs[3 + i] = Ort::Value::CreateTensor<float>(memory,
                                                          m_state_in[i].data(),
                                                          m_state_in[i].size(),
                                                          k_state_shape.data(),
                                                          k_state_shape.size());
        m_outputs[1 + i] = Ort::Value::CreateTensor<float>(memory,
                                                           m_state_out[i].data(),
                                                           m_state_out[i].size(),
                                                           k_state_shape.data(),
                                                           k_state_shape.size());
    }
    m_outputs[0] = Ort::Value::CreateTensor<float>(memory,
                                                   m_logits.data(),
                                                   m_logits.size(),
                                                   logits_shape.data(),
                                                   logits_shape.size());
}

std::span<const float> OnnxParakeet::Joint::evaluate(std::size_t frame,
                                                     std::int32_t previous_token) {
    if (frame >= m_num_frames) {
        throw std::out_of_range("tpl::stt: encoder frame " + std::to_string(frame) +
                                " out of range");
    }
    for (std::size_t d = 0; d < m_hidden_size; ++d) {
        m_frame[d] = m_encoder_out[(d * m_num_frames) + frame];
    }
    m_target = previous_token;
    m_owner.m_decoder_joint.Run(Ort::RunOptions{nullptr},
                                k_joint_inputs.data(),
                                m_inputs.data(),
                                m_inputs.size(),
                                k_joint_outputs.data(),
                                m_outputs.data(),
                                m_outputs.size());
    return m_logits;
}

void OnnxParakeet::Joint::accept_state() {
    m_state_in = m_state_out;
}

}  // namespace tpl::stt
