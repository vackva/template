#include "TranscriptView.h"

#include <algorithm>
#include <cmath>

#include "Theme.h"
#include "tpl/transcript/Export.h"

namespace {

constexpr float k_body_size = 17.0f;
constexpr float k_line_spacing = 1.55f;
constexpr float k_paragraph_gap = 16.0f;
constexpr float k_max_text_width = 680.0f;
constexpr float k_side_padding = 48.0f;
constexpr float k_top_padding = 40.0f;
constexpr std::size_t k_max_cached_layouts = 400;
constexpr double k_wheel_pixels = 240.0;

juce::String utf8(const std::string& text) {
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

/// Letters and digits only, lower-cased, for matching search terms against transcript words.
juce::String fold(const juce::String& word) {
    juce::String out;
    for (auto c = word.getCharPointer(); !c.isEmpty(); ++c) {
        if (juce::CharacterFunctions::isLetterOrDigit(*c)) {
            out += juce::CharacterFunctions::toLowerCase(*c);
        }
    }
    return out;
}

}  // namespace

TranscriptView::TranscriptView(tpl::transcript::SegmentCache& cache)
    : m_cache(cache), m_font(theme::serif_font(k_body_size)) {
    m_line_height = std::round(k_body_size * k_line_spacing);
    m_space_width = juce::GlyphArrangement::getStringWidth(m_font, "a a") -
                    2.0f * juce::GlyphArrangement::getStringWidth(m_font, "a");
    m_index.set_estimated_height(3.0 * m_line_height + k_paragraph_gap);

    m_scrollbar.addListener(this);
    m_scrollbar.setAutoHide(true);
    addAndMakeVisible(m_scrollbar);

    m_jump.onClick = [this] {
        m_following = true;
        update_layout();
        repaint();
    };
    addChildComponent(m_jump);

    setWantsKeyboardFocus(true);
    setTitle("Transcript");
    setDescription("Transcript of the selected session");
}

void TranscriptView::show(std::optional<tpl::transcript::SessionInfo> session, bool live) {
    m_session = std::move(session);
    m_live = live;
    m_following = live;
    m_selection.reset();
    m_highlight_terms.clear();
    m_layouts.clear();
    m_index.resize(0);
    m_index.resize(m_cache.size());
    m_scroll = 0.0;
    update_layout();
    repaint();
}

void TranscriptView::segments_changed(const std::vector<std::int64_t>& seqs) {
    m_index.resize(m_cache.size());
    for (const auto seq : seqs) {
        const auto row = static_cast<std::size_t>(seq);
        m_layouts.erase(row);
        if (m_selection && m_selection->m_seq == row) { m_selection.reset(); }
    }
    update_layout();
    repaint();
}

void TranscriptView::set_live(bool live) {
    if (m_live == live) { return; }
    m_live = live;
    if (!live) { m_following = false; }
    update_layout();
    repaint();
}

void TranscriptView::reveal(std::int64_t seq, const juce::String& query) {
    m_highlight_terms.clear();
    for (const auto& term : juce::StringArray::fromTokens(query, " \t", "")) {
        if (const auto folded = fold(term); folded.isNotEmpty()) { m_highlight_terms.add(folded); }
    }
    if (seq >= 0 && static_cast<std::size_t>(seq) < m_cache.size()) {
        m_following = false;
        (void)layout_of(static_cast<std::size_t>(seq));
        scroll_to(row_top(static_cast<std::size_t>(seq)) - 24.0);
    }
    update_layout();
    repaint();
}

juce::Rectangle<float> TranscriptView::text_column() const {
    const float available = static_cast<float>(getWidth()) - 2.0f * k_side_padding;
    const float width = std::clamp(available, 120.0f, k_max_text_width);
    return {(static_cast<float>(getWidth()) - width) / 2.0f,
            0.0f,
            width,
            static_cast<float>(getHeight())};
}

float TranscriptView::header_height() const {
    return k_top_padding + 40.0f + 22.0f + 28.0f;  // title, date line, space below
}

double TranscriptView::max_scroll() const {
    const double content = header_height() + m_index.total_height() + k_top_padding;
    return std::max(0.0, content - getHeight());
}

double TranscriptView::row_top(std::size_t seq) const {
    return header_height() + m_index.offset_of(seq);
}

const TranscriptView::Laid& TranscriptView::layout_of(std::size_t seq) {
    const float width = text_column().getWidth();
    auto found = m_layouts.find(seq);
    if (found != m_layouts.end() && std::abs(found->second.m_width - width) < 0.5f) {
        return found->second;
    }

    if (m_layouts.size() > k_max_cached_layouts) { m_layouts.clear(); }
    const auto& segment = m_cache.at(seq);
    Laid laid;
    laid.m_width = width;
    laid.m_final = segment.m_is_final;
    laid.m_layout
        .layout(segment.m_text, width, m_space_width, m_line_height, [this](std::string_view word) {
            return juce::GlyphArrangement::getStringWidth(
                m_font,
                juce::String::fromUTF8(word.data(), static_cast<int>(word.size())));
        });
    m_index.set_height(seq, std::max(laid.m_layout.height(), m_line_height) + k_paragraph_gap);
    return m_layouts.insert_or_assign(seq, std::move(laid)).first->second;
}

void TranscriptView::update_layout() {
    const auto rows = m_index.size();
    if (rows > 0 && getHeight() > 0) {
        // Remember what is at the top of the view, lay out what is visible, restore.
        const double content_y = std::max(0.0, m_scroll - header_height());
        const auto anchor = m_index.row_at(content_y);
        const double anchor_offset = content_y - m_index.offset_of(anchor);
        for (auto row = anchor; row < rows && row_top(row) < m_scroll + getHeight() * 2.0; ++row) {
            (void)layout_of(row);
        }
        if (m_scroll > header_height()) {
            m_scroll = header_height() + m_index.offset_of(anchor) + anchor_offset;
        }
        if (m_following) {
            // Lay out the tail so the bottom is exact, then pin to it.
            for (auto row = rows; row > 0 && row_top(row - 1) > max_scroll() - getHeight(); --row) {
                (void)layout_of(row - 1);
            }
            m_scroll = max_scroll();
        }
    }
    m_scroll = std::clamp(m_scroll, 0.0, max_scroll());

    m_scrollbar.setRangeLimits(0.0, max_scroll() + getHeight(), juce::dontSendNotification);
    m_scrollbar.setCurrentRange(m_scroll, getHeight(), juce::dontSendNotification);
    m_jump.setVisible(m_live && !m_following);
}

void TranscriptView::scroll_to(double position) {
    m_scroll = std::clamp(position, 0.0, max_scroll());
    // Reaching the bottom of a live session resumes following.
    if (m_live) { m_following = m_scroll >= max_scroll() - 1.0; }
    update_layout();
    repaint();
}

void TranscriptView::scrollBarMoved(juce::ScrollBar* /*bar*/, double new_start) {
    scroll_to(new_start);
}

void TranscriptView::resized() {
    m_scrollbar.setBounds(getLocalBounds().removeFromRight(10).reduced(0, 8));
    m_jump.setBounds(getLocalBounds().removeFromBottom(56).withSizeKeepingCentre(130, 32));
    update_layout();
}

void TranscriptView::paint(juce::Graphics& g) {
    g.fillAll(theme::k_card);
    const auto column = text_column();
    const auto scroll = static_cast<float>(m_scroll);

    if (!m_session) {
        g.setColour(theme::k_faint);
        g.setFont(theme::ui_font(15.0f));
        g.drawText("Press Record to start a session, or pick one on the left.",
                   getLocalBounds(),
                   juce::Justification::centred);
        return;
    }

    // Header: title and start time.
    const float title_y = k_top_padding - scroll;
    g.setColour(theme::k_text);
    g.setFont(theme::serif_font(28.0f, true));
    g.drawText(utf8(m_session->m_title),
               juce::Rectangle<float>(column.getX(), title_y, column.getWidth(), 40.0f),
               juce::Justification::centredLeft,
               true);
    g.setColour(theme::k_muted);
    g.setFont(theme::ui_font(13.0f));
    const auto subtitle =
        "Started " + utf8(tpl::transcript::format_local_date_time(m_session->m_started_utc_ms)) +
        (m_live ? theme::text("  ·  recording") : juce::String());
    g.drawText(subtitle,
               juce::Rectangle<float>(column.getX(), title_y + 40.0f, column.getWidth(), 22.0f),
               juce::Justification::centredLeft,
               true);

    if (m_index.size() == 0) {
        g.setColour(theme::k_faint);
        g.setFont(m_font);
        g.drawText(m_live ? theme::text("Listening…") : juce::String("No speech in this session."),
                   juce::Rectangle<float>(column.getX(),
                                          header_height() - scroll,
                                          column.getWidth(),
                                          m_line_height),
                   juce::Justification::centredLeft);
        return;
    }

    // Paragraphs in view.
    g.setFont(m_font);
    const float ascent = m_font.getAscent();
    const float text_top = (m_line_height - m_font.getHeight()) / 2.0f;
    for (auto row = m_index.row_at(std::max(0.0, m_scroll - header_height())); row < m_index.size();
         ++row) {
        const float top = static_cast<float>(row_top(row) - m_scroll);
        if (top > static_cast<float>(getHeight())) { break; }
        const auto& laid = layout_of(row);
        const auto& words = laid.m_layout.words();
        const auto& lines = laid.m_layout.lines();

        std::size_t sel_first = 1;
        std::size_t sel_last = 0;
        if (m_selection && m_selection->m_seq == row) {
            sel_first = std::min(m_selection->m_anchor, m_selection->m_end);
            sel_last = std::max(m_selection->m_anchor, m_selection->m_end);
        }
        for (std::size_t w = 0; w < words.size(); ++w) {
            const auto& word = words[w];
            const float x = column.getX() + word.m_x;
            const float y = top + lines[word.m_line].m_y;
            const bool selected = w >= sel_first && w <= sel_last;
            if (selected || is_highlighted(word.m_text)) {
                const bool joins_next =
                    selected && w < sel_last && words[w + 1].m_line == word.m_line;
                const float width = joins_next ? words[w + 1].m_x - word.m_x : word.m_width;
                g.setColour(selected ? theme::k_selection : theme::k_highlight);
                g.fillRect(juce::Rectangle<float>(x, y + 2.0f, width, m_line_height - 4.0f));
            }
            g.setColour(laid.m_final ? theme::k_text : theme::k_muted);
            g.drawSingleLineText(utf8(word.m_text),
                                 static_cast<int>(std::round(x)),
                                 static_cast<int>(std::round(y + text_top + ascent)));
        }
    }
}

std::optional<TranscriptView::Hit> TranscriptView::hit_test(juce::Point<float> point) {
    if (m_index.size() == 0) { return std::nullopt; }
    const double content_y = point.y + m_scroll - header_height();
    if (content_y < 0.0) { return std::nullopt; }
    const auto row = m_index.row_at(content_y);
    const auto& laid = layout_of(row);
    const auto local_y = static_cast<float>(content_y - m_index.offset_of(row));
    const auto word = laid.m_layout.word_at(point.x - text_column().getX(), local_y);
    if (!word) { return std::nullopt; }
    return Hit{.m_seq = row, .m_word = *word};
}

bool TranscriptView::is_highlighted(const std::string& word) const {
    if (m_highlight_terms.isEmpty()) { return false; }
    const auto folded = fold(utf8(word));
    for (const auto& term : m_highlight_terms) {
        if (folded.startsWith(term)) { return true; }
    }
    return false;
}

void TranscriptView::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();
    const auto hit = hit_test(event.position);
    if (event.mods.isPopupMenu()) {
        if (hit && !(m_selection && m_selection->m_seq == hit->m_seq)) {
            m_selection =
                Selection{.m_seq = hit->m_seq, .m_anchor = hit->m_word, .m_end = hit->m_word};
        }
        repaint();
        show_menu(event.getPosition());
        return;
    }
    m_selection.reset();
    if (hit) {
        m_selection = Selection{.m_seq = hit->m_seq, .m_anchor = hit->m_word, .m_end = hit->m_word};
    }
    repaint();
}

void TranscriptView::mouseDrag(const juce::MouseEvent& event) {
    if (!m_selection) { return; }
    // Selection stays within its paragraph: dragging past it clamps to its first/last word.
    const auto top = static_cast<float>(row_top(m_selection->m_seq) - m_scroll);
    const auto& layout = layout_of(m_selection->m_seq).m_layout;
    if (const auto word =
            layout.word_at(event.position.x - text_column().getX(), event.position.y - top)) {
        m_selection->m_end = *word;
    }
    repaint();
}

void TranscriptView::mouseDoubleClick(const juce::MouseEvent& event) {
    if (const auto hit = hit_test(event.position)) {
        const auto& words = layout_of(hit->m_seq).m_layout.words();
        if (!words.empty()) {
            m_selection = Selection{.m_seq = hit->m_seq, .m_anchor = 0, .m_end = words.size() - 1};
        }
        repaint();
    }
}

void TranscriptView::mouseWheelMove(const juce::MouseEvent& /*event*/,
                                    const juce::MouseWheelDetails& wheel) {
    const double delta = -wheel.deltaY * k_wheel_pixels * (wheel.isReversed ? -1.0 : 1.0);
    if (delta < 0.0 && m_live) { m_following = false; }
    scroll_to(m_scroll + delta);
}

bool TranscriptView::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)) {
        copy_selection();
        return true;
    }
    if (key == juce::KeyPress::pageDownKey || key == juce::KeyPress::pageUpKey) {
        const double page = getHeight() * 0.9;
        if (key == juce::KeyPress::pageUpKey && m_live) { m_following = false; }
        scroll_to(m_scroll + (key == juce::KeyPress::pageDownKey ? page : -page));
        return true;
    }
    if (key == juce::KeyPress::endKey) {
        m_following = m_live;
        scroll_to(max_scroll());
        return true;
    }
    if (key == juce::KeyPress::homeKey) {
        m_following = false;
        scroll_to(0.0);
        return true;
    }
    return false;
}

void TranscriptView::copy_selection() const {
    if (!m_selection) { return; }
    const auto found = m_layouts.find(m_selection->m_seq);
    if (found == m_layouts.end()) { return; }
    juce::SystemClipboard::copyTextToClipboard(
        utf8(found->second.m_layout.text_between(m_selection->m_anchor, m_selection->m_end)));
}

void TranscriptView::show_menu(juce::Point<int> position) {
    juce::PopupMenu menu;
    menu.addItem(juce::PopupMenu::Item("Copy selection")
                     .setEnabled(m_selection.has_value())
                     .setAction([this] { copy_selection(); }));
    if (m_selection) {
        const auto seq = m_selection->m_seq;
        menu.addItem("Copy paragraph", [this, seq] {
            juce::SystemClipboard::copyTextToClipboard(utf8(m_cache.at(seq).m_text));
        });
    }
    menu.addItem("Copy session", [this] {
        if (on_copy_session) { on_copy_session(); }
    });
    menu.addSeparator();
    menu.addItem(theme::text("Export…"), [this] {
        if (on_export) { on_export(); }
    });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        localAreaToGlobal(juce::Rectangle<int>(position.x, position.y, 1, 1))));
}
