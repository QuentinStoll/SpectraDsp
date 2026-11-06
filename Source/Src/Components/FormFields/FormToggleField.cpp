#include "Components/FormFields/FormToggleField.h"

FormToggleField::FormToggleField(const juce::Identifier& fieldId, const juce::String& labelText)
    : FormField(fieldId, labelText) {
    addAndMakeVisible(_toggleButton);

    _toggleButton.setLabelVisible(false);
    _toggleButton.onClick = [this]() {
        notifyValueChanged(getLabelText() + (getToggleState() ? ": On" : ": Off"));
    };
}

bool FormToggleField::getToggleState() const {
    return _toggleButton.getToggleState();
}

void FormToggleField::setToggleState(bool shouldBeOn, juce::NotificationType notification) {
    _toggleButton.setToggleState(shouldBeOn, notification);
}

CustomToggleButton& FormToggleField::getToggleButton() noexcept {
    return _toggleButton;
}

const CustomToggleButton& FormToggleField::getToggleButton() const noexcept {
    return _toggleButton;
}

juce::var FormToggleField::getValue() const {
    return getToggleState();
}

void FormToggleField::setValue(const juce::var& value) {
    setToggleState(static_cast<bool>(value), juce::dontSendNotification);
}

void FormToggleField::resizedControl(juce::Rectangle<int> controlArea) {
    const auto switchWidth = juce::jmin(78, controlArea.getWidth());
    _toggleButton.setBounds(controlArea.removeFromLeft(switchWidth));
}
