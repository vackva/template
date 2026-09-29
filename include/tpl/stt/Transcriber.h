#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/stt/TdtGreedyDecoder.h"
#include "tpl/stt/Vocabulary.h"

namespace tpl::stt {

/// System-wide directory the installer puts the Parakeet model in
/// (macOS /Library/Application Support/tpl/models/..., Windows C:/ProgramData/tpl/models/...,
/// Linux /usr/local/share/tpl/models/...).
[[nodiscard]] TPL_API std::filesystem::path default_model_dir();

struct TranscriberConfig {
    std::filesystem::path m_model_dir = default_model_dir();
    int m_num_threads = 1;  ///< ONNX Runtime intra-op threads per session
};

struct Transcript {
    std::string m_text;
    std::vector<Token> m_tokens;
};

/// Offline speech-to-text with nvidia/parakeet-tdt-0.6b-v3 (int8 ONNX, exported by
/// scripts/export-parakeet). Transcribes a complete 16 kHz mono buffer in one call; the
/// language (25 European languages) is detected by the model.
///
/// Not real-time safe: loading and transcribe() allocate and may take seconds. Keep it off
/// the audio thread. One instance must not be used from two threads at once.
class TPL_API Transcriber {
public:
    static constexpr std::uint32_t k_sample_rate = 16000;
    /// Encoder frame length: 10 ms feature hop x subsampling factor 8.
    static constexpr double k_seconds_per_frame = 0.08;

    /// Loads the model. Throws std::runtime_error if a file is missing or invalid.
    explicit Transcriber(const TranscriberConfig& config = {});
    ~Transcriber();
    Transcriber(Transcriber&&) noexcept;
    Transcriber& operator=(Transcriber&&) noexcept;
    Transcriber(const Transcriber&) = delete;
    Transcriber& operator=(const Transcriber&) = delete;

    /// `samples`: 16 kHz mono, nominally in [-1, 1]. Empty input gives an empty transcript.
    [[nodiscard]] Transcript transcribe(std::span<const float> samples);

    /// The model's tokens, e.g. to group m_tokens into words.
    [[nodiscard]] const Vocabulary& vocabulary() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace tpl::stt
