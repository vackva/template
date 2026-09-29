#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Colours, fonts and metrics of the Transcriber UI (after the "Scribe" notes-app reference:
/// light grey chrome, white document card, serif body text, soft lavender selection).
namespace theme {

inline const juce::Colour k_background{0xfff4f4f5};  // window and sidebar
inline const juce::Colour k_card{0xffffffff};        // transcript document
inline const juce::Colour k_border{0xffe4e4e7};
inline const juce::Colour k_text{0xff18181b};
inline const juce::Colour k_muted{0xff71717a};
inline const juce::Colour k_faint{0xffa1a1aa};
inline const juce::Colour k_selected_row{0xffe9e9ec};
inline const juce::Colour k_selection{0xffede4fb};  // selected words
inline const juce::Colour k_highlight{0xfffff1b8};  // search matches
inline const juce::Colour k_record{0xffe5484d};
inline const juce::Colour k_accent{0xff6d4aff};

inline constexpr int k_header_height = 56;
inline constexpr int k_sidebar_width = 240;
inline constexpr int k_gutter = 16;
inline constexpr float k_corner = 14.0f;

/// UI text from a UTF-8 literal. juce::String(const char*) takes ASCII only (and asserts on
/// "…", "·" or umlauts); every non-ASCII literal goes through here.
inline juce::String text(const char* utf8) {
    return juce::String::fromUTF8(utf8);
}

/// UI chrome: the platform sans-serif.
juce::Font ui_font(float height, bool bold = false);
/// Transcript body and title: a serif (Georgia / Times New Roman / DejaVu Serif fallback).
juce::Font serif_font(float height, bool bold = false);

/// Flat, borderless controls in the palette above.
class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();

    void drawButtonBackground(juce::Graphics&,
                              juce::Button&,
                              const juce::Colour& background,
                              bool highlighted,
                              bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int button_height) override;
    void fillTextEditorBackground(juce::Graphics&,
                                  int width,
                                  int height,
                                  juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;
};

}  // namespace theme
