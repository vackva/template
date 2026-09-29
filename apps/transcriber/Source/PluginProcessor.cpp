#include "PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "PluginEditor.h"
#include "tpl/stt/StreamingTranscriber.h"
#include "tpl/transcript/ReplaySegmentSource.h"

namespace {

// Past this many pending events without an editor to take them, the editor reloads instead.
constexpr std::size_t k_max_pending_events = 4096;
// A session whose heartbeat is older than this was left open by a crash.
constexpr std::int64_t k_abandoned_after_ms = 5 * 60 * 1000;

std::int64_t now_utc_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// Stand-in for the streaming speech-to-text backend until it lands: scripted sentences
/// released in step with the audio that passes through (8 s per segment, 1.5 s latency).
/// Where the models are: the folder from Settings, else the system-wide install, else (dev
/// builds) the repository's models/ folder. Empty if none holds both models.
tpl::stt::StreamingConfig models_at(const std::filesystem::path& model_dir,
                                    const std::filesystem::path& vad_model) {
    tpl::stt::StreamingConfig config;
    config.m_model_dir = model_dir;
    config.m_vad_model = vad_model;
    return config;
}

std::optional<tpl::stt::StreamingConfig> find_models(const std::filesystem::path& configured) {
    std::vector<tpl::stt::StreamingConfig> candidates;
    if (!configured.empty()) {
        // A configured Parakeet folder expects Silero VAD in its sibling folder.
        const auto vad_folder = tpl::stt::default_vad_model().parent_path().filename();
        candidates.push_back(
            models_at(configured, configured.parent_path() / vad_folder / "silero_vad.onnx"));
    }
    candidates.emplace_back();  // the system-wide install
    candidates.push_back(models_at(TPL_TRANSCRIBER_DEV_MODEL_DIR, TPL_TRANSCRIBER_DEV_VAD_MODEL));
    for (auto& candidate : candidates) {
        if (std::filesystem::exists(candidate.m_model_dir / "vocab.txt") &&
            std::filesystem::exists(candidate.m_vad_model)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::unique_ptr<tpl::stt::SegmentSource> make_demo_source() {
    return std::make_unique<tpl::transcript::ReplaySegmentSource>(
        tpl::transcript::ReplaySegmentSource::synthetic_script(64, 18, 8.0),
        tpl::stt::k_segment_sample_rate * 3 / 2);
}

}  // namespace

juce::AudioProcessor::BusesProperties TranscriberProcessor::buses() {
    // The standalone app only listens: without an output bus JUCE sees no feedback loop
    // (so it does not mute the microphone) and nothing is played back. The plugin passes
    // its audio through.
    if (juce::JUCEApplicationBase::isStandaloneApp()) {
        return BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true);
    }
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

juce::File TranscriberProcessor::settings_file() {
    return juce::File(
        juce::String((tpl::transcript::default_data_dir() / "settings.json").u8string().c_str()));
}

TranscriberProcessor::TranscriberProcessor() : AudioProcessor(buses()) {
    try {
        m_settings = tpl::transcript::load_settings(
            std::filesystem::path(settings_file().getFullPathName().toStdString()));
    } catch (const std::exception& e) {
        m_settings = tpl::transcript::default_settings();
        m_startup_error = e.what();
    }
    create_source();
    open_storage();
}

void TranscriberProcessor::create_source() {
    if (auto models = find_models(m_settings.m_model_dir)) {
        auto streaming = std::make_unique<tpl::stt::StreamingTranscriber>(std::move(*models));
        m_streaming = streaming.get();
        m_source = std::move(streaming);
        m_demo_source = false;
    } else {
        m_streaming = nullptr;
        m_source = make_demo_source();
        m_demo_source = true;
    }
    if (m_sample_rate > 0.0) { m_source->prepare(m_sample_rate, m_max_block); }
}

std::string TranscriberProcessor::source_status() const {
    if (m_demo_source) {
        return "Demo mode: no speech model found, scripted sentences stand in for it.";
    }
    switch (m_streaming->state()) {
        case tpl::stt::StreamingTranscriber::State::Loading: return "Loading the speech model…";
        case tpl::stt::StreamingTranscriber::State::Failed:
            return "The speech model failed to load: " + m_streaming->error();
        case tpl::stt::StreamingTranscriber::State::Ready: return {};
    }
    return {};
}

TranscriberProcessor::~TranscriberProcessor() {
    close_storage();
}

void TranscriberProcessor::open_storage() {
    try {
        m_store = std::make_unique<tpl::transcript::TranscriptStore>(m_settings.m_storage_dir);
        const auto now = now_utc_ms();
        m_store->recover_abandoned_sessions(now - k_abandoned_after_ms);
        if (m_settings.m_retention_days > 0) {
            m_store->purge_finished_before(now -
                                           std::int64_t{m_settings.m_retention_days} * 86'400'000);
        }
        tpl::transcript::ServiceConfig config;
        config.m_max_session_length = std::chrono::hours(m_settings.m_max_session_hours);
        m_service =
            std::make_unique<tpl::transcript::TranscriptionService>(*m_source, *m_store, config);
        m_service->add_listener(this);
        m_audio_service.store(m_service.get(), std::memory_order_release);
    } catch (const std::exception& e) {
        m_service.reset();
        m_store.reset();
        m_startup_error = std::string("Cannot open the transcript storage: ") + e.what();
    }
}

void TranscriberProcessor::close_storage() {
    m_audio_service.store(nullptr, std::memory_order_seq_cst);
    // A processBlock() that loaded the old pointer holds m_audio_busy until it is done;
    // wait for it here on the message thread (the audio thread itself never waits).
    while (m_audio_busy.load(std::memory_order_seq_cst) != 0) { std::this_thread::yield(); }
    if (m_service) { m_service->remove_listener(this); }
    m_service.reset();  // finishes a running session
    m_store.reset();
}

std::string TranscriberProcessor::apply_settings(const tpl::transcript::Settings& settings) {
    const bool storage_moved = settings.m_storage_dir != m_settings.m_storage_dir;
    const bool model_changed = settings.m_model_dir != m_settings.m_model_dir;
    const bool service_changed =
        model_changed || settings.m_max_session_hours != m_settings.m_max_session_hours;
    const auto previous = m_settings;
    try {
        tpl::transcript::save_settings(
            std::filesystem::path(settings_file().getFullPathName().toStdString()),
            settings);
    } catch (const std::exception& e) { return e.what(); }
    m_settings = settings;
    if (storage_moved || service_changed ||
        settings.m_retention_days != previous.m_retention_days) {
        close_storage();
        std::string copy_error;
        if (storage_moved) {
            const auto from =
                previous.m_storage_dir / tpl::transcript::TranscriptStore::k_file_name;
            const auto to = settings.m_storage_dir / tpl::transcript::TranscriptStore::k_file_name;
            std::error_code error;
            std::filesystem::create_directories(settings.m_storage_dir, error);
            if (!error && std::filesystem::exists(from) && !std::filesystem::exists(to)) {
                std::filesystem::copy_file(from, to, error);  // the old file stays as a backup
            }
            if (error) { copy_error = "Copying the transcripts failed: " + error.message(); }
        }
        m_startup_error.clear();
        if (model_changed) { create_source(); }
        open_storage();
        if (!copy_error.empty() && m_startup_error.empty()) { m_startup_error = copy_error; }
        const std::scoped_lock lock(m_events_mutex);
        m_events.m_reload = true;
    }
    return m_startup_error;
}

void TranscriberProcessor::prepareToPlay(double sample_rate, int max_block_size) {
    m_sample_rate = sample_rate;
    m_max_block = max_block_size;
    m_mono.assign(static_cast<std::size_t>(std::max(1, max_block_size)), 0.0f);
    m_source->prepare(sample_rate, max_block_size);
}

bool TranscriberProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) {
        return false;
    }
    // Input only (standalone), or pass-through with matching channels (plugin).
    return out.isDisabled() || out == in;
}

void TranscriberProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midi*/) {
    juce::ScopedNoDenormals no_denormals;
    // Audio passes through unchanged. The busy count keeps close_storage() from destroying
    // the service while this block uses it.
    m_audio_busy.fetch_add(1, std::memory_order_seq_cst);
    const auto* service = m_audio_service.load(std::memory_order_seq_cst);
    if (service != nullptr && service->is_recording() && !m_mono.empty()) { push_mono(buffer); }
    m_audio_busy.fetch_sub(1, std::memory_order_seq_cst);
}

void TranscriberProcessor::push_mono(const juce::AudioBuffer<float>& buffer) noexcept {
    const int channels = getTotalNumInputChannels();
    const int total = buffer.getNumSamples();
    const float gain = channels > 0 ? 1.0f / static_cast<float>(channels) : 0.0f;
    for (int start = 0; start < total; start += static_cast<int>(m_mono.size())) {
        const int count = std::min(total - start, static_cast<int>(m_mono.size()));
        std::fill_n(m_mono.begin(), count, 0.0f);
        for (int channel = 0; channel < channels; ++channel) {
            const float* in = buffer.getReadPointer(channel, start);
            for (int i = 0; i < count; ++i) { m_mono[static_cast<std::size_t>(i)] += in[i] * gain; }
        }
        m_source->push_audio(
            std::span<const float>(m_mono.data(), static_cast<std::size_t>(count)));
    }
}

juce::AudioProcessorEditor* TranscriberProcessor::createEditor() {
    return new TranscriberEditor(*this);
}

void TranscriberProcessor::getStateInformation(juce::MemoryBlock& data) {
    juce::XmlElement xml("TranscriberState");
    if (m_viewed_session) {
        xml.setAttribute("viewedSession",
                         juce::String(static_cast<juce::int64>(*m_viewed_session)));
    }
    copyXmlToBinary(xml, data);
}

void TranscriberProcessor::setStateInformation(const void* data, int size) {
    if (const auto xml = getXmlFromBinary(data, size); xml != nullptr) {
        if (xml->hasAttribute("viewedSession")) {
            m_viewed_session = xml->getStringAttribute("viewedSession").getLargeIntValue();
        }
    }
}

void TranscriberProcessor::start_recording() {
    if (m_service) { m_service->start(m_settings.m_language); }
}

void TranscriberProcessor::stop_recording() {
    if (m_service) { m_service->stop(); }
}

bool TranscriberProcessor::is_recording() const noexcept {
    return m_service != nullptr && m_service->is_recording();
}

TranscriptEvents TranscriberProcessor::take_events() {
    const std::scoped_lock lock(m_events_mutex);
    return std::exchange(m_events, {});
}

void TranscriberProcessor::segment_stored(tpl::transcript::SessionId session, std::int64_t seq) {
    const std::scoped_lock lock(m_events_mutex);
    if (m_events.m_stored.size() >= k_max_pending_events) {
        m_events.m_stored.clear();
        m_events.m_reload = true;
    }
    if (!m_events.m_reload) { m_events.m_stored.emplace_back(session, seq); }
}

void TranscriberProcessor::sessions_changed() {
    const std::scoped_lock lock(m_events_mutex);
    m_events.m_sessions_changed = true;
}

void TranscriberProcessor::service_error(const std::string& message) {
    const std::scoped_lock lock(m_events_mutex);
    m_events.m_errors.push_back(message);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new TranscriberProcessor();
}
