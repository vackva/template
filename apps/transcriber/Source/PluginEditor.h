#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <optional>

#include "PluginProcessor.h"
#include "SessionSidebar.h"
#include "SettingsPanel.h"
#include "Theme.h"
#include "TranscriptView.h"
#include "tpl/transcript/SegmentCache.h"

/// Three columns like a notes app: sessions on the left, the transcript as a document in
/// the middle (the right-hand editing column is out of scope). A header carries the
/// breadcrumb, the language, Record/Stop and the gear. A timer pulls what the recording
/// worker reported (TranscriberProcessor::take_events) onto the message thread.
class TranscriberEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TranscriberEditor(TranscriberProcessor& owner);
    ~TranscriberEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    /// (Re)creates everything that reads the store; after the storage folder moved.
    void attach_store();
    void open_session(std::optional<tpl::transcript::SessionId> session,
                      std::optional<std::int64_t> seq = std::nullopt,
                      const juce::String& query = {});
    void update_header();
    void toggle_recording();
    void rename_session(tpl::transcript::SessionId session);
    void delete_session(tpl::transcript::SessionId session);
    void export_session(tpl::transcript::SessionId session);
    void copy_session(tpl::transcript::SessionId session);
    void show_settings();
    void show_error(const juce::String& message);
    void update_banner();

    TranscriberProcessor& m_processor;
    theme::LookAndFeel m_look;

    std::unique_ptr<tpl::transcript::SegmentCache> m_cache;
    std::unique_ptr<SessionSidebar> m_sidebar;
    std::unique_ptr<TranscriptView> m_view;

    juce::Label m_logo{{}, "Transcriber"};
    juce::Label m_breadcrumb;
    juce::ComboBox m_language;
    juce::TextButton m_record{"Record"};
    juce::TextButton m_gear{"Settings"};
    juce::Label m_banner;
    SettingsPanel m_settings;

    std::optional<tpl::transcript::SessionId> m_live;
    juce::String m_error;
    std::int64_t m_recording_started_ms = 0;
    juce::ScopedMessageBox m_message_box;
    std::unique_ptr<juce::AlertWindow> m_rename_window;
    std::unique_ptr<juce::FileChooser> m_chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TranscriberEditor)
};
