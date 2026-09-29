#include "tpl/stt/Transcriber.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

#include "OnnxParakeet.h"
#include "tpl/stt/TdtGreedyDecoder.h"
#include "tpl/stt/Vocabulary.h"

namespace tpl::stt {

std::filesystem::path default_model_dir() {
    return {TPL_STT_MODEL_INSTALL_DIR};
}

struct Transcriber::Impl {
    explicit Impl(const TranscriberConfig& config)
        : m_vocabulary(Vocabulary::load(config.m_model_dir / "vocab.txt"))
        , m_model(config.m_model_dir, m_vocabulary.size(), config.m_num_threads)
        , m_decoder_config{.m_blank_id = m_vocabulary.blank_id(),
                           .m_num_tokens = m_vocabulary.size(),
                           .m_durations = {0, 1, 2, 3, 4},
                           .m_max_symbols_per_step = 10} {}

    Vocabulary m_vocabulary;
    OnnxParakeet m_model;
    TdtConfig m_decoder_config;
};

Transcriber::Transcriber(const TranscriberConfig& config)
    : m_impl(std::make_unique<Impl>(config)) {}
Transcriber::~Transcriber() = default;
Transcriber::Transcriber(Transcriber&&) noexcept = default;
Transcriber& Transcriber::operator=(Transcriber&&) noexcept = default;

Transcript Transcriber::transcribe(std::span<const float> samples) {
    // Digital silence: the per-feature normalisation of a constant signal yields all-zero
    // features, on which the model hallucinates ("Ha ha ha."). NeMo transcribes it as "".
    if (std::ranges::all_of(samples, [](float sample) { return sample == 0.0f; })) { return {}; }
    const auto num_frames = m_impl->m_model.encode(samples);
    Transcript transcript;
    transcript.m_tokens =
        tdt_greedy_decode(m_impl->m_model.joint(), num_frames, m_impl->m_decoder_config);

    std::vector<std::int32_t> ids;
    ids.reserve(transcript.m_tokens.size());
    for (const auto& token : transcript.m_tokens) { ids.push_back(token.m_id); }
    transcript.m_text = m_impl->m_vocabulary.decode(ids);
    return transcript;
}

}  // namespace tpl::stt
