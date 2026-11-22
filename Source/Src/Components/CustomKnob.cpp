#include "Components/CustomKnob.h"

#include "Stylesheet.h"

namespace {
constexpr int kNormalDragSensitivity = 220;
constexpr int kFineDragSensitivity = 900;
}  // namespace

void CustomKnob::FineSlider::applyDragSensitivity(const juce::MouseEvent& event) {
    setMouseDragSensitivity(event.mods.isShiftDown() ? kFineDragSensitivity : kNormalDragSensitivity);
}

void CustomKnob::FineSlider::mouseDown(const juce::MouseEvent& event) {
    applyDragSensitivity(event);
    juce::Slider::mouseDown(event);
}

void CustomKnob::FineSlider::mouseDrag(const juce::MouseEvent& event) {
    applyDragSensitivity(event);
    juce::Slider::mouseDrag(event);
}

void CustomKnob::FineSlider::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    auto details = wheel;
    if (event.mods.isShiftDown()) {
        details.deltaX *= 0.18f;
        details.deltaY *= 0.18f;
    }

    juce::Slider::mouseWheelMove(event, details);
}

//==============================================================================
// CustomKnob Implementation
//==============================================================================

CustomKnob::CustomKnob(const juce::String& name, float min, float max, float defaultValue, const juce::String& suffix, float step) {
    _slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    _slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    _slider.setRange(min, max, step);
    _slider.setValue(defaultValue);
    _slider.setTextValueSuffix(" " + suffix);
    _slider.setWantsKeyboardFocus(false);
    _slider.setDoubleClickReturnValue(true, defaultValue);
    _slider.setMouseDragSensitivity(kNormalDragSensitivity);

    _label.setText(juce::String(name).toUpperCase(), juce::dontSendNotification);
    _label.setJustificationType(juce::Justification::centred);
    _label.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    _label.setMinimumHorizontalScale(0.75f);
    _label.setFont(ProfilerStyle::Fonts::control());

    addAndMakeVisible(_slider);
    addAndMakeVisible(_label);
}

void CustomKnob::paint(juce::Graphics& /*g*/) {
}

void CustomKnob::resized() {
    auto area = getLocalBounds();

    constexpr auto labelHeight = 16;
    constexpr auto labelToKnobGap = 2;
    constexpr auto maxKnobSide = 84;

    const auto availableKnobHeight = juce::jmax(0, area.getHeight() - labelHeight - labelToKnobGap);
    const auto knobSide = juce::jmax(24, juce::jmin(maxKnobSide, juce::jmin(area.getWidth(), availableKnobHeight)));
    const auto contentHeight = juce::jmin(area.getHeight(), labelHeight + labelToKnobGap + knobSide);
    _label.setFont(ProfilerStyle::Fonts::medium(juce::jlimit(10.0f, 12.0f, static_cast<float>(knobSide) * 0.15f)));

    auto contentArea = area.withHeight(contentHeight).withCentre(area.getCentre());
    _slider.setBounds(contentArea.removeFromTop(knobSide).withSizeKeepingCentre(knobSide, knobSide));
    contentArea.removeFromTop(labelToKnobGap);
    _label.setBounds(contentArea.removeFromTop(labelHeight));
}
