#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <string>

#include "Theme.h"
#include "tpl/transcript/Settings.h"

/// The gear panel: storage folder, retention, maximum session length and model folder.
/// Shown as an overlay over the editor; Save hands the edited settings to on_save, which
/// returns an error message or "".
class SettingsPanel final : public juce::Component {
public:
    SettingsPanel();

    void load(const tpl::transcript::Settings& settings);

    std::function<std::string(const tpl::transcript::Settings&)> on_save;
    std::function<void()> on_close;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    [[nodiscard]] juce::Rectangle<int> card() const;
    void choose_folder(juce::TextEditor& target, const juce::String& title);

    tpl::transcript::Settings m_settings;
    juce::Label m_title{{}, "Settings"};
    juce::Label m_storage_label{{}, "Transcript folder"};
    juce::TextEditor m_storage;
    juce::TextButton m_storage_choose{theme::text("Choose…")};
    juce::Label m_retention_label{{}, "Keep sessions"};
    juce::ComboBox m_retention;
    juce::Label m_session_label{{}, "Split recordings after"};
    juce::ComboBox m_session_length;
    juce::Label m_model_label{{}, "Model folder"};
    juce::TextEditor m_model;
    juce::TextButton m_model_choose{theme::text("Choose…")};
    juce::Label m_status;
    juce::TextButton m_cancel{"Cancel"};
    juce::TextButton m_save{"Save"};
    std::unique_ptr<juce::FileChooser> m_chooser;
};
