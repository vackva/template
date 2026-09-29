#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "tpl/stt/Segment.h"
#include "tpl/stt/StreamingTranscriber.h"
#include "tpl/transcript/Settings.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/TranscriptionService.h"

/// What the recording worker reported since the editor last looked.
struct TranscriptEvents {
    std::vector<std::pair<tpl::transcript::SessionId, std::int64_t>> m_stored;
    bool m_sessions_changed = false;
    bool m_reload = false;  ///< too much happened while nobody looked: reload everything
    std::vector<std::string> m_errors;
};

/// Owns the recording: speech-to-text source, transcript store and service. The audio
/// passes through unchanged; while a session records, processBlock() mixes it to mono and
/// hands it to the source (which resamples on this thread). The editor is a view and may
/// come and go; it drains take_events() on a timer.
class TranscriberProcessor final : public juce::AudioProcessor,
                                   private tpl::transcript::TranscriptionService::Listener {
public:
    TranscriberProcessor();
    ~TranscriberProcessor() override;

    // --- juce::AudioProcessor --------------------------------------------------------------
    void prepareToPlay(double sample_rate, int max_block_size) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Transcriber"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int /*index*/) override {}
    const juce::String getProgramName(int /*index*/) override { return {}; }
    void changeProgramName(int /*index*/, const juce::String& /*name*/) override {}
    void getStateInformation(juce::MemoryBlock& data) override;
    void setStateInformation(const void* data, int size) override;

    // --- for the editor (message thread) ---------------------------------------------------
    /// Null if the store could not be opened; startup_error() says why.
    [[nodiscard]] tpl::transcript::TranscriptStore* store() noexcept { return m_store.get(); }
    [[nodiscard]] tpl::transcript::TranscriptionService* service() noexcept {
        return m_service.get();
    }
    [[nodiscard]] const std::string& startup_error() const noexcept { return m_startup_error; }
    [[nodiscard]] const tpl::transcript::Settings& settings() const noexcept { return m_settings; }
    /// Saves the settings; a new storage directory stops any recording, copies the
    /// database there (if none exists yet) and reopens it. Returns an error message or "".
    std::string apply_settings(const tpl::transcript::Settings& settings);
    /// True while the backend is the scripted demo source, not a speech model.
    [[nodiscard]] bool is_demo_source() const noexcept { return m_demo_source; }
    /// What the speech backend is doing, for a banner: "" when it is ready.
    [[nodiscard]] std::string source_status() const;

    void start_recording();
    void stop_recording();
    [[nodiscard]] bool is_recording() const noexcept;

    [[nodiscard]] TranscriptEvents take_events();

    /// The session the editor shows; saved with the plugin state.
    std::optional<tpl::transcript::SessionId> m_viewed_session;

    [[nodiscard]] static juce::File settings_file();

private:
    void segment_stored(tpl::transcript::SessionId session, std::int64_t seq) override;
    void sessions_changed() override;
    void service_error(const std::string& message) override;

    void push_mono(const juce::AudioBuffer<float>& buffer) noexcept;
    void create_source();
    void open_storage();
    void close_storage();

    tpl::transcript::Settings m_settings;
    bool m_demo_source = true;
    std::unique_ptr<tpl::stt::SegmentSource> m_source;
    tpl::stt::StreamingTranscriber* m_streaming = nullptr;  // m_source, if it is the real backend
    double m_sample_rate = 0.0;
    int m_max_block = 0;
    std::unique_ptr<tpl::transcript::TranscriptStore> m_store;
    std::unique_ptr<tpl::transcript::TranscriptionService> m_service;
    std::string m_startup_error;

    // Audio thread: pre-allocated in prepareToPlay().
    std::vector<float> m_mono;
    std::atomic<tpl::transcript::TranscriptionService*> m_audio_service{nullptr};
    std::atomic<int> m_audio_busy{0};

    std::mutex m_events_mutex;
    TranscriptEvents m_events;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TranscriberProcessor)
};
