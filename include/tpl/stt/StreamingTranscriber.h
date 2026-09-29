#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/stt/Resampler.h"
#include "tpl/stt/Segment.h"
#include "tpl/stt/SpscRing.h"
#include "tpl/stt/Transcriber.h"
#include "tpl/stt/VadSegmenter.h"

namespace tpl::stt {

/// System-wide location of the Silero VAD model, next to the Parakeet model.
[[nodiscard]] TPL_API std::filesystem::path default_vad_model();

struct StreamingConfig {
    std::filesystem::path m_model_dir = default_model_dir();
    std::filesystem::path m_vad_model = default_vad_model();
    int m_num_threads = 1;  ///< ONNX Runtime threads for Parakeet
    VadConfig m_vad;
    double m_ring_seconds = 60.0;  ///< 16 kHz audio buffered while the worker transcribes
    /// Optional, worker thread: the exact 16 kHz samples each segment is transcribed from and
    /// the position of the first one. For tests (audio integrity) and for keeping segment audio.
    std::function<void(std::int64_t start_sample, std::span<const float> samples)> m_segment_audio;
};

/// The streaming speech-to-text backend (accurate tier): a SegmentSource that cuts the
/// audio at pauses (Silero VAD, at most 30 s per segment) and transcribes each segment with
/// Parakeet TDT.
///
/// Audio thread: push_audio() resamples to 16 kHz (Resampler) and writes a lock-free ring;
/// nothing else. Worker thread (owned): loads both models, runs the VAD over the ring,
/// transcribes closed segments and groups tokens into words. A segment appears in
/// pop_segment() about one inference after its speech ended.
class TPL_API StreamingTranscriber final : public SegmentSource {
public:
    enum class State { Loading, Ready, Failed };

    /// Returns at once; the worker loads the models (a second or two) and reports through
    /// state() / error(). Audio pushed while loading is buffered (up to the ring size).
    explicit StreamingTranscriber(StreamingConfig config = {});
    ~StreamingTranscriber() override;
    StreamingTranscriber(const StreamingTranscriber&) = delete;
    StreamingTranscriber& operator=(const StreamingTranscriber&) = delete;
    StreamingTranscriber(StreamingTranscriber&&) = delete;
    StreamingTranscriber& operator=(StreamingTranscriber&&) = delete;

    void prepare(double sample_rate, int max_block_size) override;
    void push_audio(std::span<const float> mono) noexcept TPL_NONBLOCKING override;
    bool pop_segment(Segment& out) override;
    void reset() override;

    [[nodiscard]] State state() const noexcept { return m_state.load(std::memory_order_acquire); }
    /// Why loading failed ("" otherwise).
    [[nodiscard]] std::string error() const;
    /// Blocks until loading finished; true if the models are ready.
    bool wait_until_loaded();
    /// 16 kHz samples lost because the worker fell behind by more than the ring.
    [[nodiscard]] std::size_t dropped_samples() const noexcept { return m_ring.dropped(); }

private:
    struct Worker;

    // Audio-thread side.
    Resampler m_resampler;
    std::vector<float> m_resampled;
    SpscRing<float> m_ring;
    std::atomic<State> m_state{State::Loading};

    std::unique_ptr<Worker> m_worker;
};

}  // namespace tpl::stt
