#include "PluginEditor.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include "tpl/transcript/Export.h"

namespace {

constexpr int k_poll_hz = 20;
constexpr std::int64_t k_export_page = 1024;

juce::String utf8(const std::string& text) {
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

juce::String elapsed_text(std::int64_t ms) {
    const auto seconds = std::max<std::int64_t>(0, ms / 1000);
    return juce::String::formatted("%02d:%02d:%02d",
                                   static_cast<int>(seconds / 3600),
                                   static_cast<int>((seconds / 60) % 60),
                                   static_cast<int>(seconds % 60));
}

/// All segments of a session, loaded in pages (exports are the only full reads).
std::vector<tpl::transcript::StoredSegment> all_segments(
    const tpl::transcript::TranscriptStore& store,
    tpl::transcript::SessionId session) {
    std::vector<tpl::transcript::StoredSegment> segments;
    for (std::int64_t first = 0;; first += k_export_page) {
        auto page = store.load_segments(session, first, k_export_page);
        const bool last = static_cast<std::int64_t>(page.size()) < k_export_page;
        segments.insert(segments.end(),
                        std::make_move_iterator(page.begin()),
                        std::make_move_iterator(page.end()));
        if (last) { break; }
    }
    return segments;
}

}  // namespace

TranscriberEditor::TranscriberEditor(TranscriberProcessor& owner)
    : AudioProcessorEditor(owner), m_processor(owner) {
    setLookAndFeel(&m_look);

    m_logo.setFont(theme::ui_font(18.0f, true));
    addAndMakeVisible(m_logo);
    m_breadcrumb.setFont(theme::ui_font(14.0f));
    m_breadcrumb.setColour(juce::Label::textColourId, theme::k_muted);
    addAndMakeVisible(m_breadcrumb);

    m_language.addItem("English", 1);
    m_language.addItem("Deutsch", 2);
    m_language.setSelectedId(owner.settings().m_language == "de" ? 2 : 1,
                             juce::dontSendNotification);
    m_language.setTooltip("Language of new sessions");
    m_language.onChange = [this] {
        auto settings = m_processor.settings();
        settings.m_language = m_language.getSelectedId() == 2 ? "de" : "en";
        if (const auto error = m_processor.apply_settings(settings); !error.empty()) {
            show_error(utf8(error));
        }
    };
    addAndMakeVisible(m_language);

    m_record.onClick = [this] { toggle_recording(); };
    m_record.setColour(juce::TextButton::textColourOffId, theme::k_record);
    addAndMakeVisible(m_record);
    m_gear.onClick = [this] { show_settings(); };
    addAndMakeVisible(m_gear);

    m_banner.setFont(theme::ui_font(13.0f));
    m_banner.setJustificationType(juce::Justification::centred);
    addChildComponent(m_banner);

    m_settings.on_save = [this](const tpl::transcript::Settings& settings) {
        const auto* before = m_processor.store();
        auto error = m_processor.apply_settings(settings);
        // The processor reopened its store: every view holding the old one must go.
        if (m_processor.store() != before || m_cache == nullptr) { attach_store(); }
        return error;
    };
    m_settings.on_close = [this] { m_settings.setVisible(false); };
    addChildComponent(m_settings);

    attach_store();
    if (!m_processor.startup_error().empty()) { show_error(utf8(m_processor.startup_error())); }

    setResizable(true, true);
    setResizeLimits(760, 480, 2400, 1600);
    setSize(1100, 720);
    startTimerHz(k_poll_hz);
}

TranscriberEditor::~TranscriberEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void TranscriberEditor::attach_store() {
    auto* store = m_processor.store();
    if (m_view != nullptr) { removeChildComponent(m_view.get()); }
    if (m_sidebar != nullptr) { removeChildComponent(m_sidebar.get()); }
    m_view.reset();
    m_sidebar.reset();
    m_cache.reset();
    if (store == nullptr) {
        update_header();
        return;
    }

    m_cache = std::make_unique<tpl::transcript::SegmentCache>(*store);
    m_sidebar = std::make_unique<SessionSidebar>(*store);
    m_view = std::make_unique<TranscriptView>(*m_cache);

    m_sidebar->on_open = [this](auto session, auto seq, const juce::String& query) {
        open_session(session, seq, query);
    };
    m_sidebar->on_rename = [this](auto session) { rename_session(session); };
    m_sidebar->on_delete = [this](auto session) { delete_session(session); };
    m_sidebar->on_export = [this](auto session) { export_session(session); };
    m_view->on_copy_session = [this] {
        if (m_cache && m_cache->session()) { copy_session(*m_cache->session()); }
    };
    m_view->on_export = [this] {
        if (m_cache && m_cache->session()) { export_session(*m_cache->session()); }
    };
    addAndMakeVisible(*m_sidebar);
    addAndMakeVisible(*m_view);
    m_settings.toFront(false);

    const auto* service = m_processor.service();
    m_live = service != nullptr ? service->active_session() : std::nullopt;
    m_sidebar->set_live_session(m_live);
    auto initial = m_processor.m_viewed_session ? m_processor.m_viewed_session : m_live;
    if (!initial) {
        // Nothing remembered or recording: the newest session rather than an empty page.
        if (const auto sessions = store->list_sessions(); !sessions.empty()) {
            initial = sessions.front().m_id;
        }
    }
    open_session(initial);
    resized();
}

void TranscriberEditor::open_session(std::optional<tpl::transcript::SessionId> session,
                                     std::optional<std::int64_t> seq,
                                     const juce::String& query) {
    if (!m_cache || !m_view || !m_sidebar) { return; }
    std::optional<tpl::transcript::SessionInfo> info;
    if (session) { info = m_processor.store()->session(*session); }
    if (!info) { session.reset(); }

    m_processor.m_viewed_session = session;
    m_cache->open(session);
    m_view->show(info, session.has_value() && session == m_live);
    m_sidebar->select_session(session);
    if (seq) { m_view->reveal(*seq, query); }
    update_header();
}

void TranscriberEditor::update_header() {
    juce::String crumb = "Sessions";
    if (m_cache && m_cache->session()) {
        if (const auto info = m_processor.store()->session(*m_cache->session())) {
            crumb += "  /  " + utf8(info->m_title);
        }
    }
    m_breadcrumb.setText(crumb, juce::dontSendNotification);

    const bool recording = m_processor.is_recording();
    m_record.setButtonText(recording ? "Stop  " + elapsed_text(now_ms() - m_recording_started_ms)
                                     : juce::String("Record"));
    m_record.setEnabled(m_processor.service() != nullptr);
    m_language.setEnabled(!recording);
}

void TranscriberEditor::toggle_recording() {
    if (m_processor.is_recording()) {
        m_processor.stop_recording();
    } else {
        m_recording_started_ms = now_ms();
        m_processor.start_recording();
    }
    update_header();
}

void TranscriberEditor::timerCallback() {
    auto events = m_processor.take_events();
    for (const auto& error : events.m_errors) { show_error("Recording stopped: " + utf8(error)); }

    if (m_sidebar && m_cache && m_view) {
        const auto* service = m_processor.service();
        const auto live = service != nullptr ? service->active_session() : std::nullopt;
        const bool live_changed = live != m_live;
        if (live_changed) {
            const bool started = live.has_value() && (!m_live || m_cache->session() == m_live);
            m_live = live;
            m_sidebar->set_live_session(live);
            // Follow the recording into its new session (Record pressed, or a rollover).
            if (started) {
                open_session(live);
            } else {
                m_view->set_live(m_cache->session().has_value() && m_cache->session() == live);
            }
        }

        if (events.m_reload) {
            m_sidebar->refresh();
            open_session(m_cache->session());
        } else {
            std::vector<std::int64_t> changed;
            for (const auto& [session, seq] : events.m_stored) {
                if (m_cache->session() == session) {
                    m_cache->segment_stored(session, seq);
                    changed.push_back(seq);
                }
            }
            if (!changed.empty()) { m_view->segments_changed(changed); }
            if (events.m_sessions_changed || !events.m_stored.empty()) { m_sidebar->refresh(); }
        }
    }
    update_header();
    update_banner();
}

void TranscriberEditor::rename_session(tpl::transcript::SessionId session) {
    const auto info = m_processor.store()->session(session);
    if (!info) { return; }
    m_rename_window = std::make_unique<juce::AlertWindow>("Rename session",
                                                          juce::String(),
                                                          juce::MessageBoxIconType::NoIcon,
                                                          this);
    m_rename_window->addTextEditor("title", utf8(info->m_title));
    m_rename_window->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    m_rename_window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    m_rename_window->enterModalState(
        true,
        juce::ModalCallbackFunction::create([this, session](int result) {
            if (result == 1 && m_rename_window != nullptr) {
                const auto title = m_rename_window->getTextEditorContents("title").trim();
                if (title.isNotEmpty() && m_processor.store() != nullptr) {
                    m_processor.store()->rename_session(session, title.toStdString());
                    if (m_sidebar) { m_sidebar->refresh(); }
                    if (m_cache && m_cache->session() == session) { open_session(session); }
                }
            }
            m_rename_window.reset();
        }));
}

void TranscriberEditor::delete_session(tpl::transcript::SessionId session) {
    const auto info = m_processor.store()->session(session);
    if (!info) { return; }
    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
        juce::MessageBoxIconType::WarningIcon,
        "Delete session",
        "\"" + utf8(info->m_title) +
            "\" and its transcript will be deleted. This cannot be undone.",
        "Delete",
        "Cancel",
        this);
    m_message_box = juce::AlertWindow::showScopedAsync(options, [this, session](int result) {
        if (result != 1 || m_processor.store() == nullptr) { return; }
        m_processor.store()->remove_session(session);
        if (m_cache && m_cache->session() == session) { open_session(std::nullopt); }
        if (m_sidebar) { m_sidebar->refresh(); }
    });
}

void TranscriberEditor::copy_session(tpl::transcript::SessionId session) {
    if (auto* store = m_processor.store()) {
        juce::SystemClipboard::copyTextToClipboard(
            utf8(tpl::transcript::export_text(all_segments(*store, session))));
    }
}

void TranscriberEditor::export_session(tpl::transcript::SessionId session) {
    auto* store = m_processor.store();
    if (store == nullptr) { return; }
    const auto info = store->session(session);
    if (!info) { return; }
    const auto name = juce::File::createLegalFileName(utf8(info->m_title));
    m_chooser = std::make_unique<juce::FileChooser>(
        "Export transcript",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(name + ".txt"),
        "*.txt;*.md;*.srt");
    m_chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, session](const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            auto* current = m_processor.store();
            if (file == juce::File() || current == nullptr) { return; }
            const auto segments = all_segments(*current, session);
            const auto info_now = current->session(session);
            if (!info_now) { return; }
            std::string text;
            const auto extension = file.getFileExtension().toLowerCase();
            if (extension == ".srt") {
                text = tpl::transcript::export_srt(segments);
            } else if (extension == ".md") {
                text = tpl::transcript::export_markdown(*info_now, segments);
            } else {
                text = tpl::transcript::export_text(segments);
            }
            if (!file.replaceWithText(utf8(text), false, false, "\n")) {
                show_error("Could not write " + file.getFullPathName());
            }
        });
}

void TranscriberEditor::show_settings() {
    m_settings.load(m_processor.settings());
    m_settings.setVisible(true);
    m_settings.toFront(true);
}

void TranscriberEditor::show_error(const juce::String& message) {
    m_error = message;
    update_banner();
}

void TranscriberEditor::update_banner() {
    // An error stays until dismissed by a new one; otherwise the backend's status.
    const auto status = m_error.isNotEmpty() ? m_error : utf8(m_processor.source_status());
    const bool visible = status.isNotEmpty();
    if (status == m_banner.getText() && visible == m_banner.isVisible()) { return; }
    m_banner.setText(status, juce::dontSendNotification);
    m_banner.setColour(juce::Label::textColourId,
                       m_error.isNotEmpty() ? theme::k_record : theme::k_muted);
    m_banner.setVisible(visible);
    resized();
}

void TranscriberEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::k_background);
    g.setColour(theme::k_border);
    g.drawHorizontalLine(theme::k_header_height - 1, 0.0f, static_cast<float>(getWidth()));

    if (m_view != nullptr) {
        // The document card behind the transcript.
        const auto card = m_view->getBounds().toFloat().expanded(1.0f);
        g.setColour(theme::k_border);
        g.drawRoundedRectangle(card, theme::k_corner, 1.0f);
    } else {
        g.setColour(theme::k_muted);
        g.setFont(theme::ui_font(15.0f));
        g.drawText("The transcript storage is not available. Check the folder in Settings.",
                   getLocalBounds().withTrimmedTop(theme::k_header_height),
                   juce::Justification::centred);
    }
}

void TranscriberEditor::resized() {
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(theme::k_header_height).reduced(theme::k_gutter, 10);
    m_logo.setBounds(header.removeFromLeft(theme::k_sidebar_width - theme::k_gutter));
    m_gear.setBounds(header.removeFromRight(90));
    header.removeFromRight(8);
    m_record.setBounds(header.removeFromRight(140));
    header.removeFromRight(8);
    m_language.setBounds(header.removeFromRight(110));
    header.removeFromRight(8);
    m_breadcrumb.setBounds(header);

    if (m_banner.isVisible()) { m_banner.setBounds(bounds.removeFromBottom(28)); }
    if (m_sidebar != nullptr) {
        m_sidebar->setBounds(bounds.removeFromLeft(theme::k_sidebar_width));
    }
    if (m_view != nullptr) {
        m_view->setBounds(bounds.reduced(theme::k_gutter + 8, theme::k_gutter));
    }
    m_settings.setBounds(getLocalBounds());
}
