#include "Theme.h"

namespace theme {

namespace {

juce::String serif_name() {
    static const juce::String name = [] {
        const auto available = juce::Font::findAllTypefaceNames();
        for (const auto* candidate :
             {"Georgia", "Times New Roman", "DejaVu Serif", "Liberation Serif"}) {
            if (available.contains(candidate)) { return juce::String(candidate); }
        }
        return juce::Font::getDefaultSerifFontName();
    }();
    return name;
}

}  // namespace

juce::Font ui_font(float height, bool bold) {
    return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font serif_font(float height, bool bold) {
    return juce::Font(
        juce::FontOptions(serif_name(), height, bold ? juce::Font::bold : juce::Font::plain));
}

LookAndFeel::LookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, k_background);
    setColour(juce::TextButton::buttonColourId, k_card);
    setColour(juce::TextButton::buttonOnColourId, k_selected_row);
    setColour(juce::TextButton::textColourOffId, k_text);
    setColour(juce::TextButton::textColourOnId, k_text);
    setColour(juce::TextEditor::backgroundColourId, k_card);
    setColour(juce::TextEditor::textColourId, k_text);
    setColour(juce::TextEditor::highlightColourId, k_selection);
    setColour(juce::TextEditor::highlightedTextColourId, k_text);
    setColour(juce::TextEditor::outlineColourId, k_border);
    setColour(juce::TextEditor::focusedOutlineColourId, k_accent.withAlpha(0.5f));
    setColour(juce::CaretComponent::caretColourId, k_text);
    setColour(juce::ListBox::backgroundColourId, k_background);
    setColour(juce::ComboBox::backgroundColourId, k_card);
    setColour(juce::ComboBox::outlineColourId, k_border);
    setColour(juce::ComboBox::textColourId, k_text);
    setColour(juce::ComboBox::arrowColourId, k_muted);
    setColour(juce::PopupMenu::backgroundColourId, k_card);
    setColour(juce::PopupMenu::textColourId, k_text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, k_selected_row);
    setColour(juce::PopupMenu::highlightedTextColourId, k_text);
    setColour(juce::Label::textColourId, k_text);
    setColour(juce::ScrollBar::thumbColourId, k_faint.withAlpha(0.6f));
    setColour(juce::AlertWindow::backgroundColourId, k_card);
    setColour(juce::AlertWindow::textColourId, k_text);
    setColour(juce::ToggleButton::textColourId, k_text);
    setColour(juce::ToggleButton::tickColourId, k_accent);
    setColour(juce::Slider::textBoxTextColourId, k_text);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g,
                                       juce::Button& button,
                                       const juce::Colour& background,
                                       bool highlighted,
                                       bool down) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill =
        button.getToggleState() ? findColour(juce::TextButton::buttonOnColourId) : background;
    if (down) {
        fill = fill.darker(0.08f);
    } else if (highlighted) {
        fill = fill.darker(0.03f);
    }
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(k_border);
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton& /*button*/, int button_height) {
    return ui_font(std::min(15.0f, static_cast<float>(button_height) * 0.5f));
}

void LookAndFeel::fillTextEditorBackground(juce::Graphics& g,
                                           int width,
                                           int height,
                                           juce::TextEditor& editor) {
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(juce::Rectangle<int>(width, height).toFloat(), 10.0f);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics& g,
                                        int width,
                                        int height,
                                        juce::TextEditor& editor) {
    const auto colour = editor.hasKeyboardFocus(true)
                            ? findColour(juce::TextEditor::focusedOutlineColourId)
                            : findColour(juce::TextEditor::outlineColourId);
    g.setColour(colour);
    g.drawRoundedRectangle(juce::Rectangle<int>(width, height).toFloat().reduced(0.5f),
                           10.0f,
                           1.0f);
}

}  // namespace theme
