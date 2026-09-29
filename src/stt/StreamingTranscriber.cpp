#include "tpl/stt/StreamingTranscriber.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "SileroVad.h"
#include "tpl/Exports.h"
#include "tpl/stt/Segment.h"
#include "tpl/stt/Transcriber.h"
#include "tpl/stt/VadSegmenter.h"
#include "tpl/stt/Vocabulary.h"

namespace tpl::stt {

namespace {

constexpr std::string_view k_word_marker = "\xE2\x96\x81";  // "▁"
constexpr auto k_idle_wait = std::chrono::milliseconds(10);
// 10 ms feature hop x subsampling 8 = 80 ms per encoder frame.
constexpr std::int64_t k_samples_per_frame = 1280;
// Trim the kept audio once this much lies before what the VAD may still need.
constexpr std::int64_t k_trim_slack = k_segment_sample_rate;

/// Groups Parakeet tokens into words: a token starting with "▁" starts a new word.
std::vector<Word> words_of(const Transcript& transcript,
                           const Vocabulary& vocabulary,
                           std::int64_t segment_start,
                           std::int64_t segment_end) {
    std::vector<std::vector<std::int32_t>> groups;
    std::vector<std::int64_t> starts;
    for (const auto& token : transcript.m_tokens) {
        const bool starts_word =
            groups.empty() || vocabulary.token(token.m_id).starts_with(k_word_marker);
        if (starts_word) {
            groups.emplace_back();
            starts.push_back(segment_start +
                             static_cast<std::int64_t>(token.m_frame) * k_samples_per_frame);
        }
        groups.back().push_back(token.m_id);
    }
    std::vector<Word> words;
    words.reserve(groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto end = i + 1 < groups.size() ? starts[i + 1] : segment_end;
        words.push_back({.m_text = vocabulary.decode(groups[i]),
                         .m_start_sample = starts[i],
                         .m_end_sample = std::max(starts[i], std::min(end, segment_end))});
    }
    return words;
}

/// One Parakeet model per process and model folder, shared by every StreamingTranscriber
/// (plugin instances in a DAW would otherwise load 700 MB each). Inference is serialised
/// on m_mutex: it is CPU-bound, so parallel runs would not finish sooner anyway.
struct SharedModel {
    std::mutex m_mutex;
    Transcriber m_transcriber;

    SharedModel(const std::filesystem::path& model_dir, int num_threads)
        : m_transcriber(TranscriberConfig{.m_model_dir = model_dir, .m_num_threads = num_threads}) {
    }
};

std::shared_ptr<SharedModel> shared_model(const std::filesystem::path& model_dir, int num_threads) {
    static std::mutex registry_mutex;
    static std::map<std::filesystem::path, std::weak_ptr<SharedModel>> registry;
    const std::scoped_lock lock(registry_mutex);
    const auto key = std::filesystem::weakly_canonical(model_dir);
    if (auto existing = registry[key].lock()) { return existing; }
    auto model = std::make_shared<SharedModel>(model_dir, num_threads);  // loads: may throw
    registry[key] = model;
    return model;
}

}  // namespace

std::filesystem::path default_vad_model() {
    return {TPL_STT_VAD_INSTALL_PATH};
}

struct StreamingTranscriber::Worker {
    Worker(StreamingTranscriber& owner, StreamingConfig config)
        : m_owner(owner), m_config(std::move(config)), m_segmenter(m_config.m_vad) {
        m_thread = std::thread([this] { run(); });
    }

    ~Worker() {
        {
            const std::scoped_lock lock(m_mutex);
            m_quit = true;
        }
        m_wake.notify_all();
        m_thread.join();
    }
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&&) = delete;
    Worker& operator=(Worker&&) = delete;

    void run() {
        load();
        std::vector<float> incoming(SileroVad::k_chunk * 16);
        for (;;) {
            bool flush = false;
            {
                std::unique_lock lock(m_mutex);
                if (m_owner.m_ring.available() < SileroVad::k_chunk) {
                    m_wake.wait_for(lock, k_idle_wait, [this] {
                        return m_quit || m_flush_requested > m_flush_done;
                    });
                }
                if (m_quit) { return; }
                flush = m_flush_requested > m_flush_done;
            }
            try {
                drain(incoming);
                if (flush) { finish_stream(); }
            } catch (const std::exception& e) {
                // A failed inference loses that segment, not the stream.
                const std::scoped_lock lock(m_mutex);
                m_error = e.what();
            }
            if (flush) {
                {
                    const std::scoped_lock lock(m_mutex);
                    ++m_flush_done;
                }
                m_flushed.notify_all();
            }
        }
    }

    void load() {
        State state = State::Ready;
        std::string error;
        try {
            m_model = shared_model(m_config.m_model_dir, m_config.m_num_threads);
            m_vad = std::make_unique<SileroVad>(m_config.m_vad_model);
        } catch (const std::exception& e) {
            m_model.reset();
            m_vad.reset();
            state = State::Failed;
            error = e.what();
        }
        {
            // Published under the mutex wait_until_loaded() waits on: no lost wake-up.
            const std::scoped_lock lock(m_mutex);
            m_error = std::move(error);
            m_owner.m_state.store(state, std::memory_order_release);
        }
        m_loaded.notify_all();
    }

    /// Everything the ring holds: VAD per full chunk, transcribe closed segments.
    void drain(std::vector<float>& incoming) {
        for (;;) {
            const auto count = m_owner.m_ring.read(incoming);
            if (count == 0) { return; }
            if (!m_vad) { continue; }  // failed to load: discard
            m_pending.insert(m_pending.end(),
                             incoming.begin(),
                             incoming.begin() + static_cast<std::ptrdiff_t>(count));
            std::size_t used = 0;
            for (; used + SileroVad::k_chunk <= m_pending.size(); used += SileroVad::k_chunk) {
                const std::span<const float> chunk(m_pending.data() + used, SileroVad::k_chunk);
                m_audio.insert(m_audio.end(), chunk.begin(), chunk.end());
                if (const auto span = m_segmenter.push(m_vad->probability(chunk))) { emit(*span); }
                trim();
            }
            m_pending.erase(m_pending.begin(),
                            m_pending.begin() + static_cast<std::ptrdiff_t>(used));
        }
    }

    void trim() {
        const auto keep_from = m_segmenter.keep_from();
        if (keep_from - m_audio_start < k_trim_slack) { return; }
        const auto drop = static_cast<std::size_t>(keep_from - m_audio_start);
        m_audio.erase(
            m_audio.begin(),
            m_audio.begin() + static_cast<std::ptrdiff_t>(std::min(drop, m_audio.size())));
        m_audio_start = keep_from;
    }

    /// Session boundary: close the open segment and start positions over.
    void finish_stream() {
        if (m_vad) {
            if (const auto span = m_segmenter.flush()) { emit(*span); }
            m_vad->reset();
        }
        m_segmenter.reset();
        m_pending.clear();
        m_audio.clear();
        m_audio_start = 0;
    }

    void emit(const SpeechSpan& span) {
        const auto begin = std::clamp<std::int64_t>(span.m_start - m_audio_start,
                                                    0,
                                                    static_cast<std::int64_t>(m_audio.size()));
        const auto end = std::clamp<std::int64_t>(span.m_end - m_audio_start,
                                                  begin,
                                                  static_cast<std::int64_t>(m_audio.size()));
        const std::span<const float> samples(m_audio.data() + begin,
                                             static_cast<std::size_t>(end - begin));
        Transcript transcript;
        {
            const std::scoped_lock lock(m_model->m_mutex);
            transcript = m_model->m_transcriber.transcribe(samples);
        }
        if (transcript.m_text.empty()) { return; }  // noise the VAD let through

        const auto start = m_audio_start + begin;
        const auto stop = m_audio_start + end;
        Segment segment{
            .m_id = ++m_next_id,
            .m_start_sample = start,
            .m_end_sample = stop,
            .m_text = transcript.m_text,
            .m_words = words_of(transcript, m_model->m_transcriber.vocabulary(), start, stop),
            .m_is_final = true};
        const std::scoped_lock lock(m_output_mutex);
        m_output.push_back(std::move(segment));
    }

    StreamingTranscriber& m_owner;
    StreamingConfig m_config;

    // Worker-thread state.
    std::shared_ptr<SharedModel> m_model;
    std::unique_ptr<SileroVad> m_vad;
    VadSegmenter m_segmenter;
    std::vector<float> m_pending;  // < one chunk not yet through the VAD
    std::vector<float> m_audio;    // 16 kHz audio from m_audio_start on
    std::int64_t m_audio_start = 0;
    std::uint64_t m_next_id = 0;

    std::mutex m_mutex;  // flush bookkeeping, quit, error
    std::condition_variable m_wake;
    std::condition_variable m_flushed;
    std::condition_variable m_loaded;
    std::uint64_t m_flush_requested = 0;
    std::uint64_t m_flush_done = 0;
    bool m_quit = false;
    std::string m_error;

    std::mutex m_output_mutex;
    std::deque<Segment> m_output;

    std::thread m_thread;  // last: starts after everything above exists
};

StreamingTranscriber::StreamingTranscriber(StreamingConfig config)
    : m_ring(
          static_cast<std::size_t>(std::max(1.0, config.m_ring_seconds) * k_segment_sample_rate)) {
    m_resampler.prepare(static_cast<double>(k_segment_sample_rate),
                        static_cast<double>(k_segment_sample_rate),
                        0);
    m_worker = std::make_unique<Worker>(*this, std::move(config));
}

StreamingTranscriber::~StreamingTranscriber() = default;

void StreamingTranscriber::prepare(double sample_rate, int max_block_size) {
    const auto block = static_cast<std::size_t>(std::max(1, max_block_size));
    m_resampler.prepare(sample_rate, static_cast<double>(k_segment_sample_rate), block);
    m_resampled.assign(m_resampler.max_output(block), 0.0f);
}

void StreamingTranscriber::push_audio(std::span<const float> mono) noexcept TPL_NONBLOCKING {
    // Blocks larger than prepared are cut to size rather than allocating here.
    const auto written = m_resampler.process(mono, m_resampled);
    m_ring.write(std::span<const float>(m_resampled.data(), written));
}

bool StreamingTranscriber::pop_segment(Segment& out) {
    const std::scoped_lock lock(m_worker->m_output_mutex);
    if (m_worker->m_output.empty()) { return false; }
    out = std::move(m_worker->m_output.front());
    m_worker->m_output.pop_front();
    return true;
}

void StreamingTranscriber::reset() {
    std::unique_lock lock(m_worker->m_mutex);
    const auto ticket = ++m_worker->m_flush_requested;
    m_worker->m_wake.notify_all();
    m_worker->m_flushed.wait(lock, [&] { return m_worker->m_flush_done >= ticket; });
}

std::string StreamingTranscriber::error() const {
    const std::scoped_lock lock(m_worker->m_mutex);
    return m_worker->m_error;
}

bool StreamingTranscriber::wait_until_loaded() {
    std::unique_lock lock(m_worker->m_mutex);
    m_worker->m_loaded.wait(lock, [this] { return state() != State::Loading; });
    return state() == State::Ready;
}

}  // namespace tpl::stt
