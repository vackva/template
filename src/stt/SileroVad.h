#pragma once

#include <onnxruntime_cxx_api.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace tpl::stt {

/// Silero VAD v6 over ONNX Runtime: speech probability of 512-sample chunks at 16 kHz,
/// keeping the model's recurrent state and 64 samples of context between calls. Internal to
/// tpl_stt (StreamingTranscriber's worker thread).
class SileroVad {
public:
    static constexpr std::size_t k_chunk = 512;
    static constexpr std::size_t k_context = 64;
    static constexpr std::size_t k_state_size = std::size_t{2} * 128;

    /// Throws std::runtime_error if the model cannot be loaded.
    explicit SileroVad(const std::filesystem::path& model);

    /// Probability that `chunk` (exactly k_chunk samples) contains speech.
    float probability(std::span<const float> chunk);
    /// Clears state and context (a new stream).
    void reset();

private:
    Ort::Env m_env;
    Ort::MemoryInfo m_memory_info;
    Ort::Session m_session{nullptr};
    std::array<float, k_context + k_chunk> m_input{};
    std::array<float, k_state_size> m_state{};
    std::array<float, k_state_size> m_state_out{};
    std::int64_t m_sample_rate = 16000;
    float m_probability = 0.0f;
};

}  // namespace tpl::stt
