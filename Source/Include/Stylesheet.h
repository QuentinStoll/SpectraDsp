#pragma once
#include <JuceHeader.h>

#include "BinaryData.h"

namespace ProfilerStyle {
enum class Theme {
    Darker,
    Dark,
    Light,
    Orange
};

namespace Colors {
inline const juce::Colour background = juce::Colours::black;
inline const juce::Colour header = juce::Colours::black;
inline const juce::Colour container = juce::Colours::black;
inline const juce::Colour elevated = juce::Colours::black;
inline const juce::Colour border = juce::Colour(0xff22232A);
inline const juce::Colour slotBorder = juce::Colour(0xff22232A);
inline const juce::Colour blockFill = juce::Colours::black;
inline const juce::Colour tooltip = juce::Colour(0xff1A1A1A);
inline const juce::Colour text = juce::Colour(0xffE8E8ED);
inline const juce::Colour textMuted = juce::Colour(0xff8A8A96);
inline const juce::Colour caption = juce::Colour(0xff8A8A93);
inline const juce::Colour accent = juce::Colour(0xffFF5500);
inline const juce::Colour accentCyan = juce::Colour(0xff00E5FF);
inline const juce::Colour rigInput = juce::Colour(0xff10B981);
inline const juce::Colour rigAmp = juce::Colour(0xffFF5500);
inline const juce::Colour rigCab = juce::Colour(0xff3B82F6);
inline const juce::Colour rigEq = juce::Colour(0xffA855F7);
inline const juce::Colour rigPedal = juce::Colour(0xffE11D48);
inline const juce::Colour rigMaster = juce::Colour(0xffF4F4F5);
inline const juce::Colour rigPitch = juce::Colour(0xff22D3EE);
inline const juce::Colour rigReverb = juce::Colour(0xff818CF8);
inline const juce::Colour rigDelay = juce::Colour(0xffF59E0B);
inline const juce::Colour rigChorus = juce::Colour(0xff34D399);
inline const juce::Colour rigPhaser = juce::Colour(0xffA3E635);
inline const juce::Colour rigFlanger = juce::Colour(0xff2DD4BF);
inline const juce::Colour rigDynamics = juce::Colour(0xffFB7185);
inline const juce::Colour rigTuner = juce::Colour(0xffE4E4E7);

inline const juce::Colour white = text;
inline const juce::Colour lightestGrey = juce::Colour(0xff5C5C64);
inline const juce::Colour lighterGrey = juce::Colour(0xff3A3A40);
inline const juce::Colour lightGrey = border;
inline const juce::Colour grey = juce::Colour(0xff323238);
inline const juce::Colour darkGrey = border;
inline const juce::Colour darkerGrey = container;
inline const juce::Colour darkestGrey = background;
inline const juce::Colour orange = accent;

inline juce::Colour appBackground = background;
}  // namespace Colors

// A UI-wide preference, shared by open plugin editors. Resolve colours when
// painting so components created before a theme change update immediately.
namespace Contrast {
inline bool enabled = false;

inline juce::ChangeBroadcaster& changes() {
    static juce::ChangeBroadcaster broadcaster;
    return broadcaster;
}

inline void setEnabled(bool value) {
    if (enabled != value) {
        enabled = value;
        changes().sendChangeMessage();
    }
}

inline juce::Colour foreground(juce::Colour normal = Colors::text) {
    return enabled ? juce::Colours::white : normal;
}

inline juce::Colour outline(juce::Colour normal = Colors::border) {
    return enabled ? juce::Colours::white : normal;
}

inline juce::Colour accent(juce::Colour normal = Colors::accent) {
    return enabled ? juce::Colour(0xffffff00) : normal;
}

inline juce::Colour surface(juce::Colour normal = Colors::background) {
    return enabled ? juce::Colours::black : normal;
}

inline float disabledAlpha(float normal = 0.45f) {
    return enabled ? 0.65f : normal;
}
}  // namespace Contrast

namespace Properties {
inline const juce::Identifier outlineVisible{"outlineVisible"};
inline const juce::Identifier outlineColour{"outlineColour"};
inline const juce::Identifier toggleLabelVisible{"toggleLabelVisible"};
}  // namespace Properties

namespace Fonts {
enum class Weight {
    Regular,
    Medium,
    Bold,
    ExtraBold,
    Black
};

inline constexpr const char* family() noexcept {
    return "Orbitron";
}

inline juce::String typefaceName(Weight weight) {
    switch (weight) {
        case Weight::Medium:
            return "Orbitron Medium";
        case Weight::Bold:
            return "Orbitron Bold";
        case Weight::ExtraBold:
            return "Orbitron ExtraBold";
        case Weight::Black:
            return "Orbitron Black";
        case Weight::Regular:
        default:
            return "Orbitron";
    }
}

inline Weight weightFromFont(const juce::Font& font) {
    const auto name = font.getTypefaceName();
    if (name.containsIgnoreCase("Black")) {
        return Weight::Black;
    }
    if (name.containsIgnoreCase("ExtraBold")) {
        return Weight::ExtraBold;
    }
    if (name.containsIgnoreCase("Medium")) {
        return Weight::Medium;
    }
    if (name.containsIgnoreCase("Bold") || font.isBold()) {
        return Weight::Bold;
    }
    return Weight::Regular;
}

inline juce::Typeface::Ptr typeface(Weight weight) {
    static const auto regular = juce::Typeface::createSystemTypefaceFor(BinaryData::OrbitronRegular_ttf,
                                                                        BinaryData::OrbitronRegular_ttfSize);
    static const auto medium = juce::Typeface::createSystemTypefaceFor(BinaryData::OrbitronMedium_ttf,
                                                                       BinaryData::OrbitronMedium_ttfSize);
    static const auto bold = juce::Typeface::createSystemTypefaceFor(BinaryData::OrbitronBold_ttf,
                                                                     BinaryData::OrbitronBold_ttfSize);
    static const auto extraBold = juce::Typeface::createSystemTypefaceFor(BinaryData::OrbitronExtraBold_ttf,
                                                                          BinaryData::OrbitronExtraBold_ttfSize);
    static const auto black = juce::Typeface::createSystemTypefaceFor(BinaryData::OrbitronBlack_ttf,
                                                                      BinaryData::OrbitronBlack_ttfSize);
    switch (weight) {
        case Weight::Medium:
            return medium != nullptr ? medium : regular;
        case Weight::Bold:
            return bold != nullptr ? bold : regular;
        case Weight::ExtraBold:
            return extraBold != nullptr ? extraBold : (bold != nullptr ? bold : regular);
        case Weight::Black:
            return black != nullptr ? black : (bold != nullptr ? bold : regular);
        case Weight::Regular:
        default:
            return regular;
    }
}

inline juce::Font make(float height, Weight weight, float tracking = 0.0f) {
    const auto px = static_cast<float>(juce::jmax(1, juce::roundToInt(height)));
    auto* face = typeface(weight).get();
    juce::Font font = face != nullptr
                          ? juce::Font(juce::FontOptions(typeface(weight)).withHeight(px))
                          : juce::Font(juce::FontOptions(typefaceName(weight), px, juce::Font::plain));
    if (tracking != 0.0f) {
        font.setExtraKerningFactor(tracking);
    }
    return font;
}

inline juce::String defaultFamily() {
    return family();
}

inline juce::Font regular(float height) {
    return make(height, Weight::Regular);
}

inline juce::Font medium(float height) {
    return make(height, Weight::Medium);
}

inline juce::Font bold(float height) {
    return make(height, Weight::Bold);
}

inline juce::Font extraBold(float height) {
    return make(height, Weight::ExtraBold);
}

inline juce::Font black(float height) {
    return make(height, Weight::Black);
}

inline juce::Font logo(float height) {
    return make(height, Weight::Black, 0.22f);
}

inline juce::Font moduleTitle() {
    return make(11.0f, Weight::Bold);
}

inline juce::Font control() {
    return make(10.0f, Weight::Medium);
}

inline juce::Font micro() {
    return make(9.0f, Weight::Medium);
}
}  // namespace Fonts

namespace Surfaces {
inline constexpr float panelCorner = 6.0f;
inline constexpr float controlCorner = 6.0f;

inline void fillWindow(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(Contrast::surface(Colors::appBackground));
    g.fillRect(area);
}

inline void fillPanel(juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius = panelCorner) {
    g.setColour(Colors::elevated);
    g.fillRoundedRectangle(area, cornerRadius);
    g.setColour(Contrast::outline());
    g.drawRoundedRectangle(area.reduced(0.5f), cornerRadius, 1.0f);
}

inline void fillBar(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(Colors::background);
    g.fillRect(area);
}

inline void drawHairline(juce::Graphics& g, juce::Rectangle<float> line) {
    g.setColour(Contrast::outline());
    g.fillRect(line);
}
}  // namespace Surfaces

namespace Gradients {
inline static juce::ColourGradient vertical(
    juce::Rectangle<float> area,
    juce::Colour top,
    juce::Colour bottom,
    float midPoint = 0.5f) {
    juce::ColourGradient gradient(
        top, area.getX(), area.getY(),
        bottom, area.getX(), area.getBottom(),
        false);
    gradient.addColour(midPoint, bottom);

    return gradient;
}
}  // namespace Gradients
}  // namespace ProfilerStyle
