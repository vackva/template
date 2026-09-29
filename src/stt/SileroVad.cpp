#include "SileroVad.h"

#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>

namespace tpl::stt {

namespace {

constexpr std::array<const char*, 3> k_inputs{"input", "state", "sr"};
constexpr std::array<const char*, 2> k_outputs{"output", "stateN"};

}  // namespace

SileroVad::SileroVad(const std::filesystem::path& model)
    : m_env(ORT_LOGGING_LEVEL_WARNING, "tpl_stt_vad")
    , m_memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    if (!std::filesystem::exists(model)) {
        throw std::runtime_error("tpl::stt: VAD model missing: " + model.string());
    }
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(1);
    options.SetInterOpNumThreads(1);
    options.SetExecutionMode(ORT_SEQUENTIAL);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.AddConfigEntry("session.intra_op.allow_spinning", "0");
    try {
        m_session = Ort::Session(m_env, model.c_str(), options);
    } catch (const Ort::Exception& e) {
        throw std::runtime_error("tpl::stt: cannot load " + model.string() + ": " + e.what());
    }
}

void SileroVad::reset() {
    m_input.fill(0.0f);
    m_state.fill(0.0f);
}

float SileroVad::probability(std::span<const float> chunk) {
    if (chunk.size() != k_chunk) {
        throw std::invalid_argument("SileroVad: chunk must be 512 samples");
    }
    std::ranges::copy(chunk, m_input.begin() + k_context);

    const std::array<std::int64_t, 2> input_shape{1, static_cast<std::int64_t>(m_input.size())};
    const std::array<std::int64_t, 3> state_shape{2, 1, 128};
    const std::array<std::int64_t, 2> output_shape{1, 1};
    std::array<Ort::Value, 3> inputs{
        Ort::Value::CreateTensor<float>(m_memory_info,
                                        m_input.data(),
                                        m_input.size(),
                                        input_shape.data(),
                                        2),
        Ort::Value::CreateTensor<float>(m_memory_info,
                                        m_state.data(),
                                        m_state.size(),
                                        state_shape.data(),
                                        3),
        Ort::Value::CreateTensor<std::int64_t>(m_memory_info, &m_sample_rate, 1, nullptr, 0)};
    std::array<Ort::Value, 2> outputs{
        Ort::Value::CreateTensor<float>(m_memory_info, &m_probability, 1, output_shape.data(), 2),
        Ort::Value::CreateTensor<float>(m_memory_info,
                                        m_state_out.data(),
                                        m_state_out.size(),
                                        state_shape.data(),
                                        3)};
    m_session.Run(Ort::RunOptions{nullptr},
                  k_inputs.data(),
                  inputs.data(),
                  inputs.size(),
                  k_outputs.data(),
                  outputs.data(),
                  outputs.size());

    m_state = m_state_out;
    // The last 64 samples are the next call's context.
    std::copy(m_input.end() - k_context, m_input.end(), m_input.begin());
    return m_probability;
}

}  // namespace tpl::stt
