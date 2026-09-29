#include "SettingsPanel.h"

#include <array>
#include <utility>

#include "Theme.h"
#include "tpl/stt/Transcriber.h"

namespace {

// ComboBox item id -> value.
constexpr std::array<std::pair<int, int>, 6> k_retention_days{
    {{1, 0}, {2, 7}, {3, 30}, {4, 90}, {5, 365}, {6, 3650}}};
constexpr std::array<std::pair<int, int>, 5> k_session_hours{
    {{1, 1}, {2, 4}, {3, 8}, {4, 12}, {5, 24}}};

juce::String path_text(const std::filesystem::path& path) {
    return juce::String(reinterpret_cast<const char*>(path.u8string().c_str()));
}

std::filesystem::path to_path(const juce::String& text) {
    return std::filesystem::path(text.trim().toStdString());
}

template <std::size_t N>
int id_for(const std::array<std::pair<int, int>, N>& table, int value) {
    for (const auto& [id, v] : table) {
        if (v == value) { return id; }
    }
    return table.front().first;
}

template <std::size_t N>
int value_for(const std::array<std::pair<int, int>, N>& table, int id) {
    for (const auto& [i, value] : table) {
        if (i == id) { return value; }
    }
    return table.front().second;
}

}  // namespace

SettingsPanel::SettingsPanel() {
    m_title.setFont(theme::ui_font(20.0f, true));
    for (auto* label : {&m_storage_label, &m_retention_label, &m_session_label, &m_model_label}) {
        label->setFont(theme::ui_font(13.0f));
        label->setColour(juce::Label::textColourId, theme::k_muted);
        addAndMakeVisible(label);
    }
    addAndMakeVisible(m_title);

    for (auto* editor : {&m_storage, &m_model}) {
        editor->setFont(theme::ui_font(14.0f));
        editor->setIndents(10, 7);
        addAndMakeVisible(editor);
    }
    m_model.setTextToShowWhenEmpty(path_text(tpl::stt::default_model_dir()), theme::k_faint);

    m_retention.addItem("Until I delete them", 1);
    m_retention.addItem("7 days", 2);
    m_retention.addItem("30 days", 3);
    m_retention.addItem("90 days", 4);
    m_retention.addItem("1 year", 5);
    m_retention.addItem("10 years", 6);
    m_session_length.addItem("1 hour", 1);
    m_session_length.addItem("4 hours", 2);
    m_session_length.addItem("8 hours", 3);
    m_session_length.addItem("12 hours", 4);
    m_session_length.addItem("24 hours", 5);
    addAndMakeVisible(m_retention);
    addAndMakeVisible(m_session_length);

    m_storage_choose.onClick = [this] { choose_folder(m_storage, "Transcript folder"); };
    m_model_choose.onClick = [this] { choose_folder(m_model, "Model folder"); };
    addAndMakeVisible(m_storage_choose);
    addAndMakeVisible(m_model_choose);

    m_status.setFont(theme::ui_font(13.0f));
    m_status.setColour(juce::Label::textColourId, theme::k_record);
    addAndMakeVisible(m_status);

    m_cancel.onClick = [this] {
        if (on_close) { on_close(); }
    };
    m_save.onClick = [this] {
        auto edited = m_settings;
        edited.m_storage_dir = to_path(m_storage.getText());
        edited.m_model_dir = to_path(m_model.getText());
        edited.m_retention_days = value_for(k_retention_days, m_retention.getSelectedId());
        edited.m_max_session_hours = value_for(k_session_hours, m_session_length.getSelectedId());
        if (edited.m_storage_dir.empty()) {
            m_status.setText("Choose a transcript folder.", juce::dontSendNotification);
            return;
        }
        const auto error = on_save ? on_save(edited) : std::string{};
        if (!error.empty()) {
            m_status.setText(juce::String::fromUTF8(error.c_str()), juce::dontSendNotification);
            return;
        }
        if (on_close) { on_close(); }
    };
    addAndMakeVisible(m_cancel);
    addAndMakeVisible(m_save);
    setTitle("Settings");
}

void SettingsPanel::load(const tpl::transcript::Settings& settings) {
    m_settings = settings;
    m_storage.setText(path_text(settings.m_storage_dir), false);
    m_model.setText(path_text(settings.m_model_dir), false);
    m_retention.setSelectedId(id_for(k_retention_days, settings.m_retention_days),
                              juce::dontSendNotification);
    m_session_length.setSelectedId(id_for(k_session_hours, settings.m_max_session_hours),
                                   juce::dontSendNotification);
    m_status.setText({}, juce::dontSendNotification);
}

void SettingsPanel::choose_folder(juce::TextEditor& target, const juce::String& title) {
    m_chooser = std::make_unique<juce::FileChooser>(title, juce::File(target.getText()));
    m_chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [&target](const juce::FileChooser& chooser) {
            if (const auto folder = chooser.getResult(); folder != juce::File()) {
                target.setText(folder.getFullPathName(), false);
            }
        });
}

juce::Rectangle<int> SettingsPanel::card() const {
    return getLocalBounds().withSizeKeepingCentre(std::min(520, getWidth() - 32), 430);
}

void SettingsPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.25f));
    const auto area = card().toFloat();
    g.setColour(theme::k_card);
    g.fillRoundedRectangle(area, theme::k_corner);
    g.setColour(theme::k_border);
    g.drawRoundedRectangle(area, theme::k_corner, 1.0f);
}

void SettingsPanel::resized() {
    auto area = card().reduced(24);
    m_title.setBounds(area.removeFromTop(32));
    area.removeFromTop(12);

    const auto folder_row =
        [&](juce::Label& label, juce::TextEditor& editor, juce::TextButton& button) {
            label.setBounds(area.removeFromTop(20));
            auto row = area.removeFromTop(34);
            button.setBounds(row.removeFromRight(96));
            row.removeFromRight(8);
            editor.setBounds(row);
            area.removeFromTop(12);
        };
    const auto combo_row = [&](juce::Label& label, juce::ComboBox& combo) {
        label.setBounds(area.removeFromTop(20));
        combo.setBounds(area.removeFromTop(32).withWidth(220));
        area.removeFromTop(12);
    };
    folder_row(m_storage_label, m_storage, m_storage_choose);
    combo_row(m_retention_label, m_retention);
    combo_row(m_session_label, m_session_length);
    folder_row(m_model_label, m_model, m_model_choose);

    auto buttons = area.removeFromBottom(36);
    m_save.setBounds(buttons.removeFromRight(96));
    buttons.removeFromRight(8);
    m_cancel.setBounds(buttons.removeFromRight(96));
    m_status.setBounds(buttons);
}

void SettingsPanel::mouseDown(const juce::MouseEvent& event) {
    // A click outside the card closes the panel without saving.
    if (!card().contains(event.getPosition()) && on_close) { on_close(); }
}
