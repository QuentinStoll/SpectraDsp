#include "Components/CustomLookAndFeel.h"

#include <cmath>

#include "Stylesheet.h"

namespace {
constexpr float kButtonCorner = ProfilerStyle::Surfaces::controlCorner;
constexpr float kPanelCorner = ProfilerStyle::Surfaces::panelCorner;

bool getPropertyFlag(const juce::Component& component,
                     const juce::Identifier& propertyId,
                     bool defaultValue) {
    return static_cast<bool>(component.getProperties().getWithDefault(propertyId, defaultValue));
}

juce::Colour getStoredColour(const juce::Component& component,
                             const juce::Identifier& propertyId,
                             juce::Colour fallback) {
    if (auto* value = component.getProperties().getVarPointer(propertyId)) {
        return juce::Colour(static_cast<juce::uint32>(static_cast<int>(*value)));
    }

    return fallback;
}
}  // namespace

CustomLookAndFeel::CustomLookAndFeel() {
    loadOrbitronTypefaces();
    applyColourScheme();
}

void CustomLookAndFeel::loadOrbitronTypefaces() {
    _orbitronRegular = ProfilerStyle::Fonts::typeface(ProfilerStyle::Fonts::Weight::Regular);
    _orbitronMedium = ProfilerStyle::Fonts::typeface(ProfilerStyle::Fonts::Weight::Medium);
    _orbitronBold = ProfilerStyle::Fonts::typeface(ProfilerStyle::Fonts::Weight::Bold);
    _orbitronExtraBold = ProfilerStyle::Fonts::typeface(ProfilerStyle::Fonts::Weight::ExtraBold);
    _orbitronBlack = ProfilerStyle::Fonts::typeface(ProfilerStyle::Fonts::Weight::Black);

    if (_orbitronRegular != nullptr) {
        setDefaultSansSerifTypeface(_orbitronRegular);
        setDefaultSansSerifTypefaceName(ProfilerStyle::Fonts::family());
    }
}

juce::Typeface::Ptr CustomLookAndFeel::typefaceForFont(const juce::Font& font) const {
    const auto name = font.getTypefaceName();
    if (name.containsIgnoreCase("Black") && _orbitronBlack != nullptr) {
        return _orbitronBlack;
    }
    if (name.containsIgnoreCase("ExtraBold") && _orbitronExtraBold != nullptr) {
        return _orbitronExtraBold;
    }
    if (name.containsIgnoreCase("Medium") && _orbitronMedium != nullptr) {
        return _orbitronMedium;
    }
    if ((name.containsIgnoreCase("Bold") || font.isBold()) && _orbitronBold != nullptr) {
        return _orbitronBold;
    }
    if (_orbitronRegular != nullptr) {
        return _orbitronRegular;
    }
    return {};
}

void CustomLookAndFeel::applyColourScheme() {
    using namespace ProfilerStyle::Colors;

    setColourScheme({background, container, container, border, text, accent, text, accent, text});

    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::DocumentWindow::backgroundColourId, background);
    setColour(juce::DocumentWindow::textColourId, text);

    setColour(juce::TextButton::buttonColourId, elevated);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, text);

    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, accent);
    setColour(juce::ToggleButton::tickDisabledColourId, textMuted);

    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour(juce::Slider::backgroundColourId, border);
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, border);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour(juce::ComboBox::backgroundColourId, elevated);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, textMuted);
    setColour(juce::ComboBox::focusedOutlineColourId, accent);

    setColour(juce::PopupMenu::backgroundColourId, container);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, text);

    setColour(juce::TextEditor::backgroundColourId, elevated);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, border);
    setColour(juce::TextEditor::focusedOutlineColourId, accent);
    setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.28f));
    setColour(juce::TextEditor::highlightedTextColourId, text);
    setColour(juce::CaretComponent::caretColourId, accent);

    setColour(juce::ScrollBar::thumbColourId, accent);
    setColour(juce::ScrollBar::trackColourId, container);

    setColour(juce::TooltipWindow::backgroundColourId, tooltip);
    setColour(juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour(juce::TooltipWindow::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::BubbleComponent::backgroundColourId, tooltip);
    setColour(juce::BubbleComponent::outlineColourId, juce::Colours::transparentBlack);
}

juce::Font CustomLookAndFeel::getUiFont(float height, juce::Font::FontStyleFlags style) const {
    return ProfilerStyle::Fonts::make(height,
                                      (style & juce::Font::bold) != 0 ? ProfilerStyle::Fonts::Weight::Bold
                                                                      : ProfilerStyle::Fonts::Weight::Regular);
}

juce::Typeface::Ptr CustomLookAndFeel::getTypefaceForFont(const juce::Font& font) {
    if (auto typeface = typefaceForFont(font)) {
        return typeface;
    }

    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label& label) {
    const auto requested = label.getFont();
    if (requested.getTypefacePtr() != nullptr && requested.getTypefaceName().containsIgnoreCase(ProfilerStyle::Fonts::family())) {
        return requested;
    }

    const auto height = requested.getHeight() > 0.0f ? requested.getHeight() : 14.0f;
    return ProfilerStyle::Fonts::make(height,
                                      ProfilerStyle::Fonts::weightFromFont(requested),
                                      requested.getExtraKerningFactor());
}

juce::Font CustomLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight) {
    return ProfilerStyle::Fonts::medium(juce::jlimit(10.0f, 13.0f, static_cast<float>(buttonHeight) * 0.38f));
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox& box) {
    return ProfilerStyle::Fonts::medium(juce::jlimit(10.0f, 13.0f, static_cast<float>(box.getHeight()) * 0.42f));
}

juce::Font CustomLookAndFeel::getPopupMenuFont() {
    return ProfilerStyle::Fonts::regular(12.0f);
}

juce::Font CustomLookAndFeel::getSliderPopupFont(juce::Slider&) {
    return ProfilerStyle::Fonts::medium(11.0f);
}

juce::Font CustomLookAndFeel::getAlertWindowFont() {
    return ProfilerStyle::Fonts::regular(13.0f);
}

juce::Font CustomLookAndFeel::getAlertWindowTitleFont() {
    return ProfilerStyle::Fonts::bold(16.0f);
}

juce::Font CustomLookAndFeel::getAlertWindowMessageFont() {
    return ProfilerStyle::Fonts::regular(14.0f);
}

juce::Font CustomLookAndFeel::getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) {
    return ProfilerStyle::Fonts::medium(13.0f);
}

juce::Font CustomLookAndFeel::getTabButtonFont(juce::TabBarButton&, float height) {
    return ProfilerStyle::Fonts::medium(juce::jlimit(11.0f, 14.0f, height * 0.42f));
}

void CustomLookAndFeel::drawAccentGlow(juce::Graphics& g, juce::Rectangle<float> bounds, float intensity,
                                       juce::Colour colour) const {
    if (intensity <= 0.0f) {
        return;
    }

    if (colour.isTransparent()) {
        colour = ProfilerStyle::Colors::accent;
    }

    juce::DropShadow(colour.withAlpha(0.38f * intensity),
                     juce::roundToInt(8.0f + 6.0f * intensity),
                     {})
        .drawForRectangle(g, bounds.toNearestInt());
}

juce::String CustomLookAndFeel::ellipsize(const juce::Font& font, const juce::String& text, float maxWidth) const {
    if (maxWidth <= 0.0f || text.isEmpty() || juce::GlyphArrangement::getStringWidth(font, text) <= maxWidth) {
        return text;
    }

    const auto ellipsis = juce::String("...");
    auto trimmed = text;

    while (trimmed.isNotEmpty() && juce::GlyphArrangement::getStringWidth(font, trimmed + ellipsis) > maxWidth) {
        trimmed = trimmed.dropLastCharacters(1);
    }

    return trimmed.isEmpty() ? ellipsis : trimmed + ellipsis;
}

void CustomLookAndFeel::drawRaisedPanel(juce::Graphics& g,
                                        juce::Rectangle<float> bounds,
                                        bool glow,
                                        float cornerRadius) const {
    juce::ignoreUnused(glow, cornerRadius);
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
}

void CustomLookAndFeel::paintFlatButtonBackground(juce::Graphics& g,
                                                  juce::Rectangle<float> bounds,
                                                  juce::Colour backgroundColour,
                                                  bool isMouseOver,
                                                  bool isButtonDown,
                                                  bool isOn,
                                                  bool outlineVisible,
                                                  juce::Colour outlineColour) const {
    auto fillBounds = outlineVisible ? bounds.reduced(1.5f) : bounds;
    auto fill = backgroundColour;

    if (isButtonDown) {
        fill = fill.darker(0.12f);
    } else if (isMouseOver) {
        fill = fill.brighter(0.08f);
    }

    const auto glowIntensity = isButtonDown ? 0.85f : (isOn ? 0.7f : (isMouseOver ? 0.38f : 0.0f));
    drawAccentGlow(g, fillBounds.reduced(1.0f), glowIntensity);

    g.setColour(fill);
    g.fillRoundedRectangle(fillBounds, kButtonCorner);

    if (glowIntensity > 0.0f) {
        g.setColour(ProfilerStyle::Colors::accent.withAlpha(0.1f * glowIntensity));
        g.fillRoundedRectangle(fillBounds, kButtonCorner);
        g.setColour(ProfilerStyle::Colors::accent.withAlpha(0.55f * glowIntensity));
        g.drawRoundedRectangle(fillBounds.reduced(0.5f), kButtonCorner, 1.0f);
    } else {
        g.setColour(ProfilerStyle::Colors::border);
        g.drawRoundedRectangle(fillBounds.reduced(0.5f), kButtonCorner, 1.0f);
    }

    if (outlineVisible) {
        const auto colour = outlineColour.isTransparent() ? ProfilerStyle::Colors::lightestGrey
                                                          : outlineColour;
        g.setColour(colour.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), kButtonCorner + 1.0f, 1.0f);
    }
}

void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool isMouseOverButton, bool isButtonDown) {
    paintFlatButtonBackground(g,
                              button.getLocalBounds().toFloat(),
                              backgroundColour,
                              isMouseOverButton,
                              isButtonDown,
                              button.getToggleState(),
                              getPropertyFlag(button, ProfilerStyle::Properties::outlineVisible, false),
                              getStoredColour(button, ProfilerStyle::Properties::outlineColour,
                                              ProfilerStyle::Colors::lightestGrey));
}

void CustomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                       bool /*isMouseOverButton*/, bool /*isButtonDown*/) {
    const auto alpha = button.isEnabled() ? 1.0f : 0.45f;
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                          : juce::TextButton::textColourOffId)
                    .withMultipliedAlpha(alpha));
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, const float rotaryStartAngle,
                                         const float rotaryEndAngle, juce::Slider& slider) {
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const auto side = std::min(bounds.getWidth(), bounds.getHeight());
    auto knobArea = bounds.withSizeKeepingCentre(side, side);
    const auto centre = knobArea.getCentre();
    const auto radius = side * 0.5f;
    const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.35f;
    const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = juce::jlimit(1.5f, 2.4f, radius * 0.08f);
    const auto arcRadius = radius - lineW * 1.2f;
    const auto isActive = slider.isEnabled() && (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true));
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(ProfilerStyle::Colors::border.withAlpha(enabledAlpha));
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (slider.isEnabled()) {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, toAngle, true);

        if (isActive) {
            g.setColour(accent.withAlpha(0.22f));
            g.strokePath(valueArc, juce::PathStrokeType(lineW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(accent.withAlpha(enabledAlpha));
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (slider.getTextBoxPosition() == juce::Slider::NoTextBox) {
        const auto valueText = slider.getTextFromValue(slider.getValue()).trim();
        const auto fontHeight = juce::jlimit(9.0f, 10.0f, radius * 0.32f);
        g.setFont(ProfilerStyle::Fonts::medium(fontHeight));
        g.setColour(ProfilerStyle::Colors::text.withAlpha(enabledAlpha));
        g.drawFittedText(valueText,
                         knobArea.reduced(radius * 0.28f).toNearestInt(),
                         juce::Justification::centred,
                         2);
    }
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider) {
    juce::ignoreUnused(minSliderPos, maxSliderPos);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(8.0f);
    if (bounds.isEmpty()) {
        return;
    }

    const auto isVertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.35f;
    const auto trackThickness = 4.0f;
    const auto isActive = slider.isEnabled() && (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true));

    juce::Rectangle<float> trackBounds = isVertical
                                             ? bounds.withSizeKeepingCentre(trackThickness, bounds.getHeight())
                                             : bounds.withSizeKeepingCentre(bounds.getWidth(), trackThickness);

    g.setColour(ProfilerStyle::Colors::border.withAlpha(enabledAlpha));
    g.fillRoundedRectangle(trackBounds, trackThickness * 0.5f);

    const auto clampedSliderPos = isVertical
                                      ? juce::jlimit(trackBounds.getY(), trackBounds.getBottom(), sliderPos)
                                      : juce::jlimit(trackBounds.getX(), trackBounds.getRight(), sliderPos);

    if (slider.isEnabled()) {
        juce::Rectangle<float> valueBounds = isVertical
                                                 ? juce::Rectangle<float>(trackBounds.getX(), clampedSliderPos,
                                                                          trackBounds.getWidth(), trackBounds.getBottom() - clampedSliderPos)
                                                 : juce::Rectangle<float>(trackBounds.getX(), trackBounds.getY(),
                                                                          clampedSliderPos - trackBounds.getX(), trackBounds.getHeight());

        if (!valueBounds.isEmpty()) {
            g.setColour(ProfilerStyle::Colors::accent.withAlpha(isActive ? 0.22f : 0.12f));
            g.fillRoundedRectangle(valueBounds.expanded(isVertical ? 2.0f : 0.0f, isVertical ? 0.0f : 2.0f),
                                   trackThickness * 0.5f);
            g.setColour(ProfilerStyle::Colors::accent);
            g.fillRoundedRectangle(valueBounds, trackThickness * 0.5f);
        }
    }

    const auto thumbSize = 12.0f;
    juce::Rectangle<float> thumbBounds(
        isVertical ? bounds.getCentreX() - thumbSize * 0.5f : clampedSliderPos - thumbSize * 0.5f,
        isVertical ? clampedSliderPos - thumbSize * 0.5f : bounds.getCentreY() - thumbSize * 0.5f,
        thumbSize, thumbSize);

    if (slider.isEnabled() && isActive) {
        juce::DropShadow(ProfilerStyle::Colors::accent.withAlpha(0.4f), 8, {})
            .drawForRectangle(g, thumbBounds.toNearestInt());
    }

    g.setColour(ProfilerStyle::Colors::elevated.withAlpha(enabledAlpha));
    g.fillEllipse(thumbBounds);
    g.setColour(ProfilerStyle::Colors::accent.withAlpha(enabledAlpha));
    g.drawEllipse(thumbBounds.reduced(0.5f), 1.4f);
}

void CustomLookAndFeel::drawSwitch(juce::Graphics& g, juce::Rectangle<float> area,
                                   bool ticked, bool isEnabled, bool isMouseOver, bool isMouseDown) const {
    area = area.reduced(1.0f);
    if (area.getWidth() <= 4.0f || area.getHeight() <= 4.0f) {
        return;
    }

    const auto enabledAlpha = isEnabled ? 1.0f : 0.35f;
    const auto trackH = juce::jmin(area.getHeight() * 0.7f, 22.0f);
    const auto trackW = juce::jmin(area.getWidth() * 0.86f, trackH * 1.9f);
    auto trackRect = area.withSizeKeepingCentre(trackW, trackH);
    const auto cornerSize = trackH * 0.5f;
    auto trackColour = ticked ? ProfilerStyle::Colors::accent : ProfilerStyle::Colors::border;

    if (isMouseOver) {
        trackColour = ticked ? trackColour.brighter(0.08f) : trackColour.brighter(0.06f);
    }
    if (isMouseDown) {
        trackColour = trackColour.darker(0.08f);
    }

    if (ticked && isEnabled) {
        drawAccentGlow(g, trackRect, isMouseOver ? 0.7f : 0.5f);
    }

    g.setColour(trackColour.withAlpha(enabledAlpha * (ticked ? 0.85f : 1.0f)));
    g.fillRoundedRectangle(trackRect, cornerSize);

    g.setColour((ticked ? ProfilerStyle::Colors::accent : ProfilerStyle::Colors::lightestGrey)
                    .withAlpha(enabledAlpha * 0.45f));
    g.drawRoundedRectangle(trackRect.reduced(0.5f), cornerSize, 1.0f);

    const auto thumbPadding = 2.0f;
    const auto thumbSize = trackH - thumbPadding * 2.0f;
    const auto thumbX = ticked ? trackRect.getRight() - thumbSize - thumbPadding
                               : trackRect.getX() + thumbPadding;
    auto thumbRect = juce::Rectangle<float>(thumbX, trackRect.getCentreY() - thumbSize * 0.5f, thumbSize, thumbSize);

    g.setColour(ProfilerStyle::Colors::text.withAlpha(enabledAlpha));
    g.fillEllipse(thumbRect);
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto area = button.getLocalBounds().toFloat();

    if (!getPropertyFlag(button, ProfilerStyle::Properties::toggleLabelVisible, true)) {
        drawSwitch(g, area, button.getToggleState(), button.isEnabled(),
                   shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        return;
    }

    const auto labelHeight = juce::jlimit(8.0f, 20.0f, area.getHeight() * 0.48f);
    auto labelArea = area.removeFromTop(labelHeight);

    g.setColour(button.findColour(juce::ToggleButton::textColourId).withAlpha(button.isEnabled() ? 1.0f : 0.45f));
    g.setFont(ProfilerStyle::Fonts::medium(juce::jlimit(8.0f, 11.0f, labelHeight * 0.8f)));
    g.drawFittedText(button.getButtonText(), labelArea.toNearestInt(), juce::Justification::centred, 1);

    drawSwitch(g, area, button.getToggleState(), button.isEnabled(),
               shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void CustomLookAndFeel::drawTickBox(juce::Graphics& g, juce::Component& button,
                                    float x, float y, float w, float h,
                                    bool ticked, bool isEnabled, bool isMouseOver, bool isMouseDown) {
    juce::ignoreUnused(button);
    drawSwitch(g, juce::Rectangle<float>(x, y, w, h), ticked, isEnabled, isMouseOver, isMouseDown);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                     int, int, int, int, juce::ComboBox& box) {
    auto area = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
    paintFlatButtonBackground(g, area, box.findColour(juce::ComboBox::backgroundColourId),
                              box.isMouseOver(), isButtonDown, false);

    auto arrowZone = juce::Rectangle<float>(static_cast<float>(width) - 24.0f, 0.0f, 18.0f, static_cast<float>(height));
    juce::Path path;
    path.addTriangle(arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                     arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                     arrowZone.getCentreX(), arrowZone.getCentreY() + 4.0f);

    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.85f : 0.4f));
    g.fillPath(path);
}

void CustomLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.fillAll(ProfilerStyle::Colors::container);
    g.setColour(ProfilerStyle::Colors::border);
    g.drawRect(0, 0, width, height, 1);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive,
                                          bool isHighlighted, bool isChecked,
                                          bool /*hasSubMenu*/, const juce::String& text,
                                          const juce::String& /*shortcutKeyText*/,
                                          const juce::Drawable* /*icon*/, const juce::Colour* textColourToUse) {
    if (isSeparator) {
        g.setColour(ProfilerStyle::Colors::border);
        g.fillRect(area.reduced(8, 0).withHeight(1).withY(area.getCentreY()));
        return;
    }

    auto r = area.toFloat().reduced(3.0f, 1.0f);

    if (isHighlighted && isActive) {
        g.setColour(ProfilerStyle::Colors::accent.withAlpha(0.16f));
        g.fillRoundedRectangle(r, 3.0f);
    }

    auto textColour = textColourToUse != nullptr ? *textColourToUse
                                                 : ProfilerStyle::Colors::text.withAlpha(isActive ? 1.0f : 0.45f);
    if (isHighlighted && isActive) {
        textColour = ProfilerStyle::Colors::text;
    }

    g.setColour(textColour);
    g.setFont(getPopupMenuFont());
    g.drawText(text, r.reduced(10.0f, 0), juce::Justification::centredLeft, true);

    if (isChecked) {
        auto indicatorSize = r.getHeight() * 0.28f;
        auto indicatorRect = juce::Rectangle<float>(r.getRight() - 18.0f,
                                                    r.getCentreY() - indicatorSize * 0.5f,
                                                    indicatorSize, indicatorSize);
        g.setColour(ProfilerStyle::Colors::accent);
        g.fillEllipse(indicatorRect);
    }
}

void CustomLookAndFeel::drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g) {
    g.setColour(ProfilerStyle::Colors::container);
    g.fillRoundedRectangle(bar.getLocalBounds().toFloat(), kPanelCorner);
}

void CustomLookAndFeel::drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                                      bool isMouseOver, bool isMouseDown) {
    auto area = button.getLocalBounds().toFloat().reduced(2.0f);
    const auto isFront = button.isFrontTab();
    auto fill = isFront ? ProfilerStyle::Colors::elevated : juce::Colours::transparentBlack;

    if (isMouseOver && !isFront) {
        fill = ProfilerStyle::Colors::elevated.withAlpha(0.55f);
    }

    if (isFront) {
        drawAccentGlow(g, area.reduced(2.0f), isMouseDown ? 0.55f : 0.35f);
    }

    if (!fill.isTransparent()) {
        g.setColour(fill);
        g.fillRoundedRectangle(area, kButtonCorner);
    }

    if (isFront) {
        auto indicator = area.removeFromBottom(2.0f).reduced(8.0f, 0.0f);
        g.setColour(ProfilerStyle::Colors::accent.withAlpha(0.7f));
        g.fillRoundedRectangle(indicator, 1.0f);
    }

    g.setColour(isFront ? ProfilerStyle::Colors::text : ProfilerStyle::Colors::textMuted);
    g.setFont(ProfilerStyle::Fonts::medium(12.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds().toFloat().reduced(2.0f),
               juce::Justification::centred);
}

void CustomLookAndFeel::drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int, int) {
}

int CustomLookAndFeel::getTabButtonBestWidth(juce::TabBarButton&, int tabDepth) {
    return tabDepth;
}

void CustomLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label) {
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        const auto font = getLabelFont(label);
        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.45f));
        g.setFont(font);
        g.drawText(label.getText(), label.getLocalBounds(), label.getJustificationType(), false);
    }
}

juce::Font CustomLookAndFeel::getTooltipFont() {
    return ProfilerStyle::Fonts::medium(11.0f);
}

juce::Rectangle<int> CustomLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                         juce::Point<int> screenPos,
                                                         juce::Rectangle<int> parentArea) {
    const auto font = getTooltipFont();
    const auto textWidth = juce::GlyphArrangement::getStringWidth(font, tipText);
    const auto width = juce::jlimit(40, juce::jmax(80, parentArea.getWidth() - 16), juce::roundToInt(textWidth + 16.0f));
    const auto height = juce::roundToInt(font.getHeight() + 10.0f);
    auto bounds = juce::Rectangle<int>(screenPos.x, screenPos.y + 14, width, height);
    return bounds.constrainedWithin(parentArea.reduced(4));
}

void CustomLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(ProfilerStyle::Colors::tooltip);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colours::white);
    g.setFont(getTooltipFont());
    g.drawFittedText(text, bounds.reduced(8.0f, 3.0f).toNearestInt(), juce::Justification::centredLeft, 3);
}

void CustomLookAndFeel::drawDocumentWindowTitleBar(juce::DocumentWindow&, juce::Graphics& g,
                                                   int w, int h, int, int,
                                                   const juce::Image*, bool) {
    g.setColour(juce::Colours::black);
    g.fillRect(0, 0, w, h);
}

void CustomLookAndFeel::fillResizableWindowBackground(juce::Graphics& g, int w, int h,
                                                      const juce::BorderSize<int>&,
                                                      juce::ResizableWindow&) {
    g.setColour(juce::Colours::black);
    g.fillRect(0, 0, w, h);
}

void CustomLookAndFeel::drawResizableWindowBorder(juce::Graphics&, int, int,
                                                  const juce::BorderSize<int>&,
                                                  juce::ResizableWindow&) {
}

void CustomLookAndFeel::drawSignalChainBlock(juce::Graphics& g,
                                             juce::Rectangle<float> bounds,
                                             juce::Colour categoryColour,
                                             RigIcon icon,
                                             bool isActive,
                                             bool isMouseOver,
                                             bool ledOn) const {
    const auto borderAlpha = isActive ? 1.0f : (isMouseOver ? 0.92f : 0.72f);
    const auto borderWidth = isActive ? 2.0f : 1.0f;

    g.setColour(ProfilerStyle::Colors::blockFill);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(categoryColour.withAlpha(borderAlpha));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, borderWidth);

    const auto led = getSignalChainLedBounds(bounds);
    if (ledOn) {
        juce::DropShadow(categoryColour.withAlpha(0.35f), 3, {}).drawForRectangle(g, led.toNearestInt());
        g.setColour(categoryColour);
    } else {
        g.setColour(ProfilerStyle::Colors::border.brighter(0.12f));
    }
    g.fillEllipse(led);

    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto iconSize = side * 0.50f;
    drawRigIcon(g, bounds.withSizeKeepingCentre(iconSize, iconSize), icon,
                categoryColour.withAlpha(ledOn ? 1.0f : 0.38f));
}

juce::Rectangle<float> CustomLookAndFeel::getSignalChainLedBounds(juce::Rectangle<float> bounds) {
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto ledSize = juce::jlimit(6.0f, 8.0f, side * 0.07f);
    return juce::Rectangle<float>(ledSize, ledSize)
        .withX(bounds.getRight() - ledSize - side * 0.08f)
        .withY(bounds.getY() + side * 0.08f);
}

juce::Rectangle<float> CustomLookAndFeel::getSignalIoLedBounds(juce::Rectangle<float> bounds) {
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto ledSize = juce::jlimit(5.0f, 6.5f, side * 0.14f);
    return juce::Rectangle<float>(ledSize, ledSize)
        .withX(bounds.getRight() - ledSize)
        .withY(bounds.getY());
}

void CustomLookAndFeel::drawSignalIoNode(juce::Graphics& g,
                                         juce::Rectangle<float> bounds,
                                         bool isActive,
                                         bool isMouseOver,
                                         bool ledOn,
                                         bool showLed,
                                         float signalLevel) const {
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto ring = bounds.withSizeKeepingCentre(side * 0.62f, side * 0.62f);
    const auto colour = juce::Colour(0xffF4F4F5);
    const auto alpha = isActive ? 1.0f : (isMouseOver ? 0.96f : 0.92f);
    const auto level = juce::jlimit(0.0f, 1.0f, signalLevel);

    if (isActive || isMouseOver) {
        g.setColour(colour.withAlpha(isActive ? 0.22f : 0.12f));
        g.drawEllipse(ring.expanded(2.2f), 3.0f);
    }

    g.setColour(juce::Colours::black);
    g.fillEllipse(ring);

    if (level > 0.004f) {
        juce::Graphics::ScopedSaveState clip(g);
        juce::Path disc;
        disc.addEllipse(ring.reduced(0.6f));
        g.reduceClipRegion(disc);

        const auto centre = ring.getCentre();
        const auto maxRadius = ring.getWidth() * 0.5f;
        const auto glowRadius = maxRadius * std::pow(level, 0.72f);
        const auto cyan = juce::Colour(0xff20F2FF);

        juce::ColourGradient wash(cyan.withAlpha(0.10f + 0.16f * level), centre,
                                  cyan.withAlpha(0.0f), {centre.x + glowRadius, centre.y},
                                  true);
        g.setGradientFill(wash);
        g.fillEllipse(centre.x - glowRadius, centre.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);

        const auto coreRadius = juce::jmax(1.2f, glowRadius * 0.42f);
        juce::ColourGradient core(cyan.withAlpha(0.28f + 0.62f * level), centre,
                                  cyan.withAlpha(0.0f), {centre.x + coreRadius, centre.y},
                                  true);
        g.setGradientFill(core);
        g.fillEllipse(centre.x - coreRadius, centre.y - coreRadius, coreRadius * 2.0f, coreRadius * 2.0f);
    }

    g.setColour(colour.withAlpha(ledOn ? alpha : 0.32f));
    g.drawEllipse(ring, kSignalBusCoreWidth);

    if (!showLed) {
        return;
    }

    const auto led = getSignalIoLedBounds(bounds);
    if (ledOn) {
        g.setColour(colour);
    } else {
        g.setColour(ProfilerStyle::Colors::border.brighter(0.12f));
    }
    g.fillEllipse(led);
}

void CustomLookAndFeel::drawSignalBus(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to) const {
    const auto colour = juce::Colour(0xffC5C5CE);
    g.setColour(colour.withAlpha(0.16f));
    g.drawLine(from.x, from.y, to.x, to.y, kSignalBusGlowWidth);
    g.setColour(colour.withAlpha(0.88f));
    g.drawLine(from.x, from.y, to.x, to.y, kSignalBusCoreWidth);
}

void CustomLookAndFeel::drawEmptySignalSlot(juce::Graphics& g, juce::Rectangle<float> bounds) const {
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.5f;
    auto outline = bounds.withSizeKeepingCentre(side, side);
    g.setColour(ProfilerStyle::Colors::slotBorder);
    g.drawRoundedRectangle(outline, 6.0f, 1.0f);
}

void CustomLookAndFeel::drawRigIcon(juce::Graphics& g, juce::Rectangle<float> bounds, RigIcon icon, juce::Colour colour) const {
    g.setColour(colour);
    const auto strokeW = juce::jlimit(1.1f, 1.8f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.05f);
    const auto stroke = juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    switch (icon) {
        case RigIcon::InputJack: {
            auto plate = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.72f, bounds.getHeight() * 0.72f);
            g.drawEllipse(plate, strokeW);
            auto inner = plate.reduced(plate.getWidth() * 0.22f);
            g.drawEllipse(inner, strokeW * 0.85f);
            auto slot = inner.withSizeKeepingCentre(inner.getWidth() * 0.22f, inner.getHeight() * 0.55f);
            g.fillRoundedRectangle(slot, 1.0f);
            break;
        }
        case RigIcon::AmpHead: {
            auto head = bounds.reduced(1.5f);
            g.drawRoundedRectangle(head, 2.2f, strokeW);
            auto controlRow = head.removeFromTop(head.getHeight() * 0.34f).reduced(3.0f, 2.2f);
            const auto knobSize = juce::jmax(3.0f, juce::jmin(controlRow.getHeight(), controlRow.getWidth() * 0.16f));
            for (int i = 0; i < 4; ++i) {
                const auto x = controlRow.getX() + (controlRow.getWidth() - knobSize) * (i / 3.0f);
                g.drawEllipse(x, controlRow.getCentreY() - knobSize * 0.5f, knobSize, knobSize, juce::jmax(1.0f, strokeW * 0.7f));
            }
            auto grille = head.reduced(3.2f, 2.4f);
            const auto dot = juce::jmax(1.4f, grille.getWidth() * 0.07f);
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 5; ++col) {
                    const auto x = grille.getX() + (grille.getWidth() - dot) * (col / 4.0f);
                    const auto y = grille.getY() + (grille.getHeight() - dot) * (row / 2.0f);
                    g.fillEllipse(x, y, dot, dot);
                }
            }
            break;
        }
        case RigIcon::Cabinet: {
            juce::Path cone;
            const auto top = bounds.getY() + bounds.getHeight() * 0.18f;
            const auto bottom = bounds.getBottom() - bounds.getHeight() * 0.12f;
            cone.addQuadrilateral(bounds.getX() + bounds.getWidth() * 0.18f, top,
                                  bounds.getRight() - bounds.getWidth() * 0.18f, top,
                                  bounds.getRight() - bounds.getWidth() * 0.32f, bottom,
                                  bounds.getX() + bounds.getWidth() * 0.32f, bottom);
            g.strokePath(cone, stroke);
            const auto centre = juce::Point<float>(bounds.getCentreX(), top + bounds.getHeight() * 0.22f);
            const auto radius = bounds.getWidth() * 0.16f;
            g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, strokeW);
            g.fillEllipse(centre.x - radius * 0.35f, centre.y - radius * 0.35f, radius * 0.7f, radius * 0.7f);
            break;
        }
        case RigIcon::Pedal: {
            auto box = bounds.reduced(bounds.getWidth() * 0.12f, bounds.getHeight() * 0.08f);
            g.drawRoundedRectangle(box, 3.0f, strokeW);
            auto footswitch = juce::Rectangle<float>(0, 0, box.getWidth() * 0.28f, box.getWidth() * 0.28f)
                                  .withCentre({box.getCentreX(), box.getBottom() - box.getHeight() * 0.28f});
            g.drawEllipse(footswitch, strokeW);
            auto jack = juce::Rectangle<float>(0, 0, box.getWidth() * 0.18f, strokeW * 1.6f)
                            .withCentre({box.getCentreX(), box.getY() + box.getHeight() * 0.22f});
            g.fillRoundedRectangle(jack, 1.0f);
            break;
        }
        case RigIcon::EqFaders: {
            const float caps[] = {0.28f, 0.58f, 0.4f};
            const auto trackW = juce::jmax(2.0f, bounds.getWidth() * 0.055f);
            const auto capW = juce::jmax(7.0f, bounds.getWidth() * 0.18f);
            const auto capH = juce::jmax(3.2f, bounds.getHeight() * 0.085f);
            for (int i = 0; i < 3; ++i) {
                const auto x = bounds.getX() + bounds.getWidth() * (0.22f + 0.28f * static_cast<float>(i));
                auto track = juce::Rectangle<float>(x - trackW * 0.5f, bounds.getY() + 2.0f, trackW, bounds.getHeight() - 4.0f);
                g.fillRoundedRectangle(track, 1.0f);
                auto cap = juce::Rectangle<float>(x - capW * 0.5f,
                                                  bounds.getY() + bounds.getHeight() * caps[i] - capH * 0.5f,
                                                  capW, capH);
                g.fillRoundedRectangle(cap, 1.0f);
            }
            break;
        }
        case RigIcon::Speaker: {
            auto ring = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.58f, bounds.getHeight() * 0.58f);
            g.setColour(juce::Colours::black);
            g.fillEllipse(ring);
            g.setColour(colour);
            g.drawEllipse(ring, juce::jmax(1.8f, strokeW * 1.6f));
            break;
        }
        case RigIcon::Pitch: {
            auto left = bounds.reduced(bounds.getWidth() * 0.18f, 1.0f);
            auto stem = left.removeFromLeft(left.getWidth() * 0.42f);
            g.drawLine(stem.getCentreX(), stem.getBottom(), stem.getCentreX(), stem.getY() + stem.getHeight() * 0.2f, strokeW);
            g.drawLine(stem.getCentreX(), stem.getY() + stem.getHeight() * 0.2f,
                       bounds.getRight() - bounds.getWidth() * 0.22f, bounds.getY() + bounds.getHeight() * 0.08f, strokeW);
            g.drawEllipse(stem.getCentreX() - stem.getWidth() * 0.28f, stem.getBottom() - stem.getHeight() * 0.28f,
                          stem.getWidth() * 0.56f, stem.getHeight() * 0.22f, strokeW);
            break;
        }
        case RigIcon::Reverb: {
            auto centre = bounds.getCentre();
            for (int arc = 0; arc < 3; ++arc) {
                const auto radius = bounds.getWidth() * (0.16f + 0.16f * static_cast<float>(arc));
                juce::Path path;
                path.addCentredArc(centre.x, centre.y + bounds.getHeight() * 0.12f, radius, radius,
                                   0.0f, -2.4f, -0.75f, true);
                g.strokePath(path, stroke);
            }
            break;
        }
        case RigIcon::Delay: {
            auto box = bounds.reduced(bounds.getWidth() * 0.12f, bounds.getHeight() * 0.22f);
            juce::Path path;
            path.startNewSubPath(box.getX(), box.getCentreY());
            path.lineTo(box.getCentreX() - box.getWidth() * 0.08f, box.getY());
            path.lineTo(box.getCentreX() - box.getWidth() * 0.08f, box.getBottom());
            path.closeSubPath();
            path.startNewSubPath(box.getRight(), box.getCentreY());
            path.lineTo(box.getCentreX() + box.getWidth() * 0.08f, box.getY());
            path.lineTo(box.getCentreX() + box.getWidth() * 0.08f, box.getBottom());
            path.closeSubPath();
            g.strokePath(path, stroke);
            break;
        }
        case RigIcon::Mod: {
            juce::Path path;
            const auto left = bounds.getX() + bounds.getWidth() * 0.08f;
            const auto right = bounds.getRight() - bounds.getWidth() * 0.08f;
            const auto mid = bounds.getCentreY();
            const auto amp = bounds.getHeight() * 0.32f;
            path.startNewSubPath(left, mid);
            path.cubicTo(left + (right - left) * 0.25f, mid - amp,
                         left + (right - left) * 0.25f, mid - amp,
                         bounds.getCentreX(), mid);
            path.cubicTo(left + (right - left) * 0.75f, mid + amp,
                         left + (right - left) * 0.75f, mid + amp,
                         right, mid);
            g.strokePath(path, stroke);
            break;
        }
        case RigIcon::Dynamics: {
            juce::Path path;
            path.startNewSubPath(bounds.getX() + bounds.getWidth() * 0.16f, bounds.getBottom() - bounds.getHeight() * 0.18f);
            path.lineTo(bounds.getCentreX(), bounds.getCentreY());
            path.lineTo(bounds.getRight() - bounds.getWidth() * 0.12f, bounds.getY() + bounds.getHeight() * 0.28f);
            g.strokePath(path, stroke);
            break;
        }
        case RigIcon::Tuner: {
            auto dial = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.72f, bounds.getHeight() * 0.72f);
            g.drawEllipse(dial, strokeW);
            g.drawLine(dial.getCentreX(), dial.getCentreY(),
                       dial.getCentreX() + dial.getWidth() * 0.08f, dial.getY() + dial.getHeight() * 0.22f, strokeW);
            break;
        }
    }
}

void CustomLookAndFeel::drawSignalCable(juce::Graphics& g,
                                        juce::Point<float> from,
                                        juce::Point<float> to,
                                        bool isEnergized) const {
    const auto colour = isEnergized ? juce::Colour(0xffC5C5CE) : ProfilerStyle::Colors::border.brighter(0.2f);
    const auto span = juce::jmax(8.0f, from.getDistanceFrom(to));
    const auto glowW = juce::jlimit(2.2f, 3.4f, span * 0.22f);
    const auto coreW = juce::jlimit(1.0f, 1.4f, span * 0.09f);
    const auto node = juce::jlimit(2.6f, 3.6f, span * 0.22f);

    g.setColour(colour.withAlpha(isEnergized ? 0.18f : 0.3f));
    g.drawLine(from.x, from.y, to.x, to.y, glowW);
    g.setColour(colour.withAlpha(isEnergized ? 0.9f : 0.65f));
    g.drawLine(from.x, from.y, to.x, to.y, coreW);

    g.setColour(colour);
    g.fillEllipse(from.x - node * 0.5f, from.y - node * 0.5f, node, node);
    g.fillEllipse(to.x - node * 0.5f, to.y - node * 0.5f, node, node);
}

void CustomLookAndFeel::drawDropZone(juce::Graphics& g,
                                     juce::Rectangle<float> bounds,
                                     const juce::String& title,
                                     const juce::String& detail,
                                     bool isLoaded,
                                     bool isDragOver,
                                     RigIcon icon,
                                     juce::Colour categoryColour) const {
    if (isDragOver) {
        drawAccentGlow(g, bounds.reduced(2.0f), 0.8f, categoryColour);
    }

    g.setColour(ProfilerStyle::Colors::blockFill);
    g.fillRoundedRectangle(bounds, kPanelCorner);

    g.setColour(isDragOver ? categoryColour
                           : (isLoaded ? categoryColour.withAlpha(0.55f) : ProfilerStyle::Colors::border));
    g.drawRoundedRectangle(bounds.reduced(0.5f), kPanelCorner, 1.0f);

    if (isDragOver) {
        auto dashBounds = bounds.reduced(10.0f);
        g.setColour(categoryColour.withAlpha(0.55f));
        const float dash[] = {5.0f, 4.0f};
        g.drawDashedLine({dashBounds.getX(), dashBounds.getY(), dashBounds.getRight(), dashBounds.getY()}, dash, 2, 1.0f);
        g.drawDashedLine({dashBounds.getRight(), dashBounds.getY(), dashBounds.getRight(), dashBounds.getBottom()}, dash, 2, 1.0f);
        g.drawDashedLine({dashBounds.getRight(), dashBounds.getBottom(), dashBounds.getX(), dashBounds.getBottom()}, dash, 2, 1.0f);
        g.drawDashedLine({dashBounds.getX(), dashBounds.getBottom(), dashBounds.getX(), dashBounds.getY()}, dash, 2, 1.0f);
    }

    auto content = bounds.reduced(14.0f, 8.0f);
    const auto hintSize = juce::jlimit(11.0f, 13.0f, bounds.getHeight() * 0.12f);
    auto hintBounds = content.removeFromBottom(hintSize + 4.0f);
    content.removeFromBottom(4.0f);

    const auto iconSize = juce::jmin(42.0f, content.getHeight() * 0.42f);
    auto iconBounds = content.removeFromTop(iconSize).withSizeKeepingCentre(iconSize, iconSize);
    drawRigIcon(g, iconBounds, icon, isDragOver || isLoaded ? categoryColour : ProfilerStyle::Colors::caption);

    const auto titleSize = juce::jlimit(12.0f, 14.0f, bounds.getHeight() * 0.14f);
    g.setColour(ProfilerStyle::Colors::caption);
    g.setFont(ProfilerStyle::Fonts::moduleTitle());
    auto titleBounds = content.removeFromTop(titleSize + 4.0f);
    g.drawFittedText(title, titleBounds.toNearestInt(), juce::Justification::centred, 1);

    const auto detailSize = juce::jlimit(isLoaded ? 14.0f : 12.0f, 16.0f, bounds.getHeight() * 0.16f);
    const auto detailFont = isLoaded ? ProfilerStyle::Fonts::bold(detailSize) : ProfilerStyle::Fonts::medium(detailSize);
    g.setFont(detailFont);
    g.setColour(isLoaded ? ProfilerStyle::Colors::text : ProfilerStyle::Colors::caption);
    g.drawText(ellipsize(detailFont, detail, content.getWidth()),
               content.toNearestInt(),
               juce::Justification::centred,
               false);

    g.setFont(ProfilerStyle::Fonts::medium(hintSize));
    g.setColour(ProfilerStyle::Colors::caption.withAlpha(0.85f));
    g.drawFittedText("Left click / Drag & Drop to load  •  Right click to unload",
                     hintBounds.toNearestInt(),
                     juce::Justification::centred,
                     1);
}

void CustomLookAndFeel::drawGearIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) const {
    const auto centre = bounds.getCentre();
    const auto outer = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.38f;
    const auto inner = outer * 0.62f;
    const auto hole = outer * 0.28f;
    constexpr int teeth = 8;

    juce::Path gear;
    for (int i = 0; i < teeth; ++i) {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(teeth) - juce::MathConstants<float>::halfPi;
        const auto next = angle + juce::MathConstants<float>::twoPi / static_cast<float>(teeth);
        const auto toothWidth = (next - angle) * 0.28f;

        const juce::Point<float> p0(centre.x + inner * std::cos(angle + toothWidth),
                                    centre.y + inner * std::sin(angle + toothWidth));
        const juce::Point<float> p1(centre.x + outer * std::cos(angle + toothWidth * 1.35f),
                                    centre.y + outer * std::sin(angle + toothWidth * 1.35f));
        const juce::Point<float> p2(centre.x + outer * std::cos(next - toothWidth * 1.35f),
                                    centre.y + outer * std::sin(next - toothWidth * 1.35f));
        const juce::Point<float> p3(centre.x + inner * std::cos(next - toothWidth),
                                    centre.y + inner * std::sin(next - toothWidth));

        if (i == 0) {
            gear.startNewSubPath(p0);
        } else {
            gear.lineTo(p0);
        }
        gear.lineTo(p1);
        gear.lineTo(p2);
        gear.lineTo(p3);
    }
    gear.closeSubPath();
    gear.addEllipse(centre.x - hole, centre.y - hole, hole * 2.0f, hole * 2.0f);
    gear.setUsingNonZeroWinding(false);

    g.setColour(colour);
    g.fillPath(gear);
}

void CustomLookAndFeel::drawLibraryIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) const {
    auto area = bounds.reduced(bounds.getWidth() * 0.22f, bounds.getHeight() * 0.22f);
    const auto gap = 3.0f;
    const auto cellW = (area.getWidth() - gap) * 0.5f;
    const auto cellH = (area.getHeight() - gap) * 0.5f;

    g.setColour(colour);
    g.fillRoundedRectangle(area.getX(), area.getY(), cellW, cellH, 1.5f);
    g.fillRoundedRectangle(area.getX() + cellW + gap, area.getY(), cellW, cellH, 1.5f);
    g.fillRoundedRectangle(area.getX(), area.getY() + cellH + gap, cellW, cellH, 1.5f);
    g.fillRoundedRectangle(area.getX() + cellW + gap, area.getY() + cellH + gap, cellW, cellH, 1.5f);
}
