#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "tpl/transcript/ParagraphLayout.h"
#include "tpl/transcript/RowIndex.h"
#include "tpl/transcript/SegmentCache.h"
#include "tpl/transcript/Types.h"

/// The transcript document: one paragraph per segment, virtualised for sessions of a day or
/// more. Only paragraphs near the viewport are loaded (SegmentCache) and laid out
/// (ParagraphLayout); RowIndex keeps the scroll geometry of everything else estimated.
///
/// While the view is at the bottom of a live session it follows new text; scrolling up
/// stops that and shows "Jump to live". Selection works on words within one paragraph;
/// the context menu copies the selection, the paragraph or the whole session.
class TranscriptView final : public juce::Component, private juce::ScrollBar::Listener {
public:
    explicit TranscriptView(tpl::transcript::SegmentCache& cache);

    /// Shows the session the cache was opened on (nullopt: an empty state).
    void show(std::optional<tpl::transcript::SessionInfo> session, bool live);
    /// The cache learned about new or revised segments.
    void segments_changed(const std::vector<std::int64_t>& seqs);
    void set_live(bool live);
    /// Scrolls to segment `seq` and highlights `query`'s words everywhere.
    void reveal(std::int64_t seq, const juce::String& query);

    std::function<void()> on_copy_session;
    std::function<void()> on_export;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    struct Laid {
        tpl::transcript::ParagraphLayout m_layout;
        float m_width = 0.0f;
        bool m_final = true;
    };
    struct Hit {
        std::size_t m_seq;
        std::size_t m_word;
    };
    struct Selection {
        std::size_t m_seq;
        std::size_t m_anchor;
        std::size_t m_end;
    };

    void scrollBarMoved(juce::ScrollBar*, double new_start) override;

    [[nodiscard]] juce::Rectangle<float> text_column() const;
    [[nodiscard]] float header_height() const;
    [[nodiscard]] double max_scroll() const;
    [[nodiscard]] double row_top(std::size_t seq) const;
    const Laid& layout_of(std::size_t seq);
    /// Lays out the rows in view, keeping the paragraph at the top of the view in place
    /// (or the bottom, when following), and updates the scroll bar.
    void update_layout();
    void scroll_to(double position);
    [[nodiscard]] std::optional<Hit> hit_test(juce::Point<float> point);
    [[nodiscard]] bool is_highlighted(const std::string& word) const;
    void copy_selection() const;
    void show_menu(juce::Point<int> position);

    tpl::transcript::SegmentCache& m_cache;
    tpl::transcript::RowIndex m_index;
    std::map<std::size_t, Laid> m_layouts;
    std::optional<tpl::transcript::SessionInfo> m_session;
    bool m_live = false;
    bool m_following = true;
    double m_scroll = 0.0;
    std::optional<Selection> m_selection;
    juce::StringArray m_highlight_terms;

    juce::ScrollBar m_scrollbar{true};
    juce::TextButton m_jump{"Jump to live"};

    juce::Font m_font;
    float m_line_height = 0.0f;
    float m_space_width = 0.0f;
};
