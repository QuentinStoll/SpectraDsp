#include "Components/FormFields/FormComboField.h"

FormComboField::FormComboField(const juce::Identifier& fieldId, const juce::String& labelText, const juce::StringArray& options)
    : FormField(fieldId, labelText) {
    addAndMakeVisible(_comboBox);
    setOptions(options);

    _comboBox.onChange = [this]() {
        notifyValueChanged(getLabelText() + ": " + getSelectedText());
    };
}

void FormComboField::setOptions(const juce::StringArray& options) {
    const auto previousSelection = getSelectedText();

    _options = options;
    _comboBox.clear(juce::dontSendNotification);

    for (int i = 0; i < _options.size(); ++i) {
        _comboBox.addItem(_options[i], i + 1);
    }

    setSelectedText(previousSelection, juce::dontSendNotification);
}

juce::String FormComboField::getSelectedText() const {
    return _comboBox.getText();
}

void FormComboField::setSelectedText(const juce::String& text, juce::NotificationType notification) {
    if (text.isEmpty()) {
        _comboBox.setSelectedId(0, notification);
        return;
    }

    const auto optionIndex = _options.indexOf(text);
    if (optionIndex >= 0) {
        _comboBox.setSelectedItemIndex(optionIndex, notification);
    } else {
        _comboBox.setSelectedId(0, notification);
    }
}

CustomComboBox& FormComboField::getComboBox() noexcept {
    return _comboBox;
}

const CustomComboBox& FormComboField::getComboBox() const noexcept {
    return _comboBox;
}

juce::var FormComboField::getValue() const {
    return getSelectedText();
}

void FormComboField::setValue(const juce::var& value) {
    setSelectedText(value.toString(), juce::dontSendNotification);
}

void FormComboField::resizedControl(juce::Rectangle<int> controlArea) {
    _comboBox.setBounds(controlArea);
}
