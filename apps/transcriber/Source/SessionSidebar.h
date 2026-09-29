#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

/// Left column: a search field over a list that shows either all sessions (newest first,
/// the live one marked) or the search hits across sessions. Right-click a session for
/// Rename / Export / Delete.
class SessionSidebar final : public juce::Component,
                             private juce::ListBoxModel,
                             private juce::Timer {
public:
    explicit SessionSidebar(tpl::transcript::TranscriptStore& store);

    /// Reloads the session list from the store (keeps the selection if it still exists).
    void refresh();
    void set_live_session(std::optional<tpl::transcript::SessionId> session);
    void select_session(std::optional<tpl::transcript::SessionId> session);

    /// A session (and, from a search hit, the segment and the query) was chosen.
    std::function<
        void(tpl::transcript::SessionId, std::optional<std::int64_t> seq, juce::String query)>
        on_open;
    std::function<void(tpl::transcript::SessionId)> on_rename;
    std::function<void(tpl::transcript::SessionId)> on_delete;
    std::function<void(tpl::transcript::SessionId)> on_export;

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    [[nodiscard]] bool showing_hits() const { return m_search.getText().trim().isNotEmpty(); }
    void run_search();
    void show_menu(tpl::transcript::SessionId session);

    tpl::transcript::TranscriptStore& m_store;
    juce::TextEditor m_search;
    juce::ListBox m_list;
    std::vector<tpl::transcript::SessionInfo> m_sessions;
    std::vector<tpl::transcript::SearchHit> m_hits;
    std::optional<tpl::transcript::SessionId> m_live;
    std::optional<tpl::transcript::SessionId> m_selected;
};
