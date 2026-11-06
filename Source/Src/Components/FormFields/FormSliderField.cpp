#include "Components/FormFields/FormSliderField.h"

FormSliderField::FormSliderField(const juce::Identifier& fieldId,
                                 const juce::String& labelText,
                                 double min,
                                 double max,
                                 double defaultValue,
                                 const juce::String& suffix,
                                 double step)
    : FormField(fieldId, labelText) {
    addAndMakeVisible(_slider);

    _slider.setSliderStyle(juce::Slider::LinearHorizontal);
    _slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
    _slider.setRange(min, max, step);
    _slider.setValue(defaultValue, juce::dontSendNotification);
    setTextValueSuffix(suffix);

    _slider.onValueChange = [this]() {
        notifyValueChanged(getLabelText() + ": " + _slider.getTextFromValue(_slider.getValue()));
    };
}

double FormSliderField::getSliderValue() const {
    return _slider.getValue();
}

void FormSliderField::setSliderValue(double value, juce::NotificationType notification) {
    _slider.setValue(value, notification);
}

void FormSliderField::setRange(double min, double max, double step) {
    _slider.setRange(min, max, step);
}

void FormSliderField::setTextValueSuffix(const juce::String& suffix) {
    _slider.setTextValueSuffix(suffix.isEmpty() ? juce::String{} : " " + suffix);
}

void FormSliderField::setTextBoxVisible(bool shouldBeVisible) {
    _slider.setTextBoxStyle(shouldBeVisible ? juce::Slider::TextBoxRight : juce::Slider::NoTextBox, false, 58, 20);
}

juce::Slider& FormSliderField::getSlider() noexcept {
    return _slider;
}

const juce::Slider& FormSliderField::getSlider() const noexcept {
    return _slider;
}

int FormSliderField::getPreferredHeight() const {
    return 36;
}

juce::var FormSliderField::getValue() const {
    return getSliderValue();
}

void FormSliderField::setValue(const juce::var& value) {
    setSliderValue(static_cast<double>(value), juce::dontSendNotification);
}

void FormSliderField::resizedControl(juce::Rectangle<int> controlArea) {
    _slider.setBounds(controlArea);
}
