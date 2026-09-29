#include "SessionSidebar.h"

#include <algorithm>

#include "Theme.h"
#include "tpl/transcript/Export.h"

namespace {

constexpr int k_row_height = 58;
constexpr int k_search_delay_ms = 200;
constexpr int k_max_hits = 200;

juce::String utf8(const std::string& text) {
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

juce::String duration_text(const tpl::transcript::SessionInfo& session) {
    if (!session.m_ended_utc_ms) { return "recording"; }
    const auto minutes = (*session.m_ended_utc_ms - session.m_started_utc_ms) / 60'000;
    if (minutes < 60) { return juce::String(minutes) + " min"; }
    return juce::String(minutes / 60) + " h " + juce::String(minutes % 60) + " min";
}

}  // namespace

SessionSidebar::SessionSidebar(tpl::transcript::TranscriptStore& store) : m_store(store) {
    m_search.setTextToShowWhenEmpty("Search transcripts", theme::k_faint);
    m_search.setFont(theme::ui_font(14.0f));
    m_search.setIndents(12, 8);
    m_search.onTextChange = [this] { startTimer(k_search_delay_ms); };
    m_search.onEscapeKey = [this] { m_search.clear(); };
    m_search.setTitle("Search transcripts");
    addAndMakeVisible(m_search);

    m_list.setModel(this);
    m_list.setRowHeight(k_row_height);
    m_list.setOutlineThickness(0);
    m_list.setTitle("Sessions");
    addAndMakeVisible(m_list);
    refresh();
}

void SessionSidebar::refresh() {
    m_sessions = m_store.list_sessions();
    if (showing_hits()) { run_search(); }
    m_list.updateContent();
    m_list.repaint();
}

void SessionSidebar::set_live_session(std::optional<tpl::transcript::SessionId> session) {
    m_live = session;
    m_list.repaint();
}

void SessionSidebar::select_session(std::optional<tpl::transcript::SessionId> session) {
    m_selected = session;
    m_list.repaint();
}

void SessionSidebar::resized() {
    auto bounds = getLocalBounds().reduced(theme::k_gutter, 0);
    bounds.removeFromTop(theme::k_gutter);
    m_search.setBounds(bounds.removeFromTop(36));
    bounds.removeFromTop(10);
    m_list.setBounds(bounds.withTrimmedBottom(theme::k_gutter));
}

void SessionSidebar::paint(juce::Graphics& g) {
    g.fillAll(theme::k_background);
    g.setColour(theme::k_border);
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
}

int SessionSidebar::getNumRows() {
    return static_cast<int>(showing_hits() ? m_hits.size() : m_sessions.size());
}

void SessionSidebar::paintListBoxItem(int row,
                                      juce::Graphics& g,
                                      int width,
                                      int height,
                                      bool /*selected*/) {
    const auto area = juce::Rectangle<int>(width, height).reduced(2, 3);
    auto text_area = area.reduced(10, 8);

    if (showing_hits()) {
        if (row < 0 || row >= static_cast<int>(m_hits.size())) { return; }
        const auto& hit = m_hits[static_cast<std::size_t>(row)];
        g.setColour(theme::k_text);
        g.setFont(theme::ui_font(13.0f, true));
        g.drawText(utf8(hit.m_session_title),
                   text_area.removeFromTop(18),
                   juce::Justification::centredLeft,
                   true);

        // Snippet with its matches highlighted.
        juce::AttributedString snippet;
        snippet.setWordWrap(juce::AttributedString::none);
        std::size_t pos = 0;
        const auto append = [&](std::size_t begin, std::size_t end, bool match) {
            if (end <= begin) { return; }
            snippet.append(utf8(hit.m_snippet.substr(begin, end - begin)),
                           theme::ui_font(12.5f, match),
                           match ? theme::k_text : theme::k_muted);
        };
        for (const auto& [begin, end] : hit.m_highlights) {
            append(pos, begin, false);
            append(begin, end, true);
            pos = end;
        }
        append(pos, hit.m_snippet.size(), false);
        snippet.draw(g, text_area.toFloat());
        return;
    }

    if (row < 0 || row >= static_cast<int>(m_sessions.size())) { return; }
    const auto& session = m_sessions[static_cast<std::size_t>(row)];
    if (m_selected == session.m_id) {
        g.setColour(theme::k_selected_row);
        g.fillRoundedRectangle(area.toFloat(), 10.0f);
    }
    const bool live = m_live == session.m_id;
    auto title_row = text_area.removeFromTop(20);
    if (live) {
        g.setColour(theme::k_record);
        g.fillEllipse(title_row.removeFromLeft(8).withSizeKeepingCentre(8, 8).toFloat());
        title_row.removeFromLeft(6);
    }
    g.setColour(theme::k_text);
    g.setFont(theme::ui_font(14.0f));
    g.drawText(utf8(session.m_title), title_row, juce::Justification::centredLeft, true);

    g.setColour(theme::k_muted);
    g.setFont(theme::ui_font(12.0f));
    const auto details = juce::String(session.m_language).toUpperCase() + theme::text("  ·  ") +
                         duration_text(session) + theme::text("  ·  ") +
                         juce::String(session.m_segment_count) + " paragraphs";
    g.drawText(details, text_area.removeFromTop(18), juce::Justification::centredLeft, true);
}

void SessionSidebar::listBoxItemClicked(int row, const juce::MouseEvent& event) {
    if (showing_hits()) {
        if (row < 0 || row >= static_cast<int>(m_hits.size())) { return; }
        const auto& hit = m_hits[static_cast<std::size_t>(row)];
        m_selected = hit.m_session_id;
        if (on_open) { on_open(hit.m_session_id, hit.m_seq, m_search.getText().trim()); }
        return;
    }
    if (row < 0 || row >= static_cast<int>(m_sessions.size())) { return; }
    const auto id = m_sessions[static_cast<std::size_t>(row)].m_id;
    if (event.mods.isPopupMenu()) {
        show_menu(id);
        return;
    }
    m_selected = id;
    m_list.repaint();
    if (on_open) { on_open(id, std::nullopt, {}); }
}

void SessionSidebar::show_menu(tpl::transcript::SessionId session) {
    juce::PopupMenu menu;
    menu.addItem(theme::text("Rename…"), [this, session] {
        if (on_rename) { on_rename(session); }
    });
    menu.addItem(theme::text("Export…"), [this, session] {
        if (on_export) { on_export(session); }
    });
    menu.addSeparator();
    menu.addItem(juce::PopupMenu::Item(theme::text("Delete…"))
                     .setEnabled(m_live != session)
                     .setAction([this, session] {
                         if (on_delete) { on_delete(session); }
                     }));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&m_list).withMousePosition());
}

void SessionSidebar::timerCallback() {
    stopTimer();
    run_search();
    m_list.updateContent();
    m_list.repaint();
}

void SessionSidebar::run_search() {
    const auto query = m_search.getText().trim();
    m_hits = query.isEmpty() ? std::vector<tpl::transcript::SearchHit>{}
                             : m_store.search(query.toStdString(), k_max_hits);
}
