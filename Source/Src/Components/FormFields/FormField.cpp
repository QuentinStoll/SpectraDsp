#include "Components/FormFields/FormField.h"

#include "ActionFeedback.h"
#include "Stylesheet.h"

FormField::FormField(const juce::Identifier& fieldId, const juce::String& labelText)
    : _fieldId(fieldId) {
    addAndMakeVisible(_label);

    _label.setText(labelText, juce::dontSendNotification);
    _label.setJustificationType(juce::Justification::centredLeft);
    _label.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _label.setFont(ProfilerStyle::Fonts::regular(14.0f));
}

const juce::Identifier& FormField::getFieldId() const noexcept {
    return _fieldId;
}

juce::String FormField::getLabelText() const {
    return _label.getText();
}

void FormField::setLabelText(const juce::String& text) {
    _label.setText(text, juce::dontSendNotification);
}

void FormField::setLabelWidth(int width) {
    _labelWidth = juce::jmax(0, width);
    resized();
}

int FormField::getLabelWidth() const noexcept {
    return _labelWidth;
}

int FormField::getPreferredHeight() const {
    return 30;
}

void FormField::resized() {
    auto area = getLocalBounds();
    const auto rowHeight = juce::jmin(getPreferredHeight(), area.getHeight());
    auto row = area.withSizeKeepingCentre(area.getWidth(), rowHeight);

    const auto labelWidth = juce::jlimit(0, row.getWidth(), _labelWidth);
    _label.setBounds(row.removeFromLeft(labelWidth));

    if (row.getWidth() > 0) {
        row.removeFromLeft(juce::jmin(8, row.getWidth()));
    }

    resizedControl(row);
}

void FormField::notifyValueChanged(const juce::String& statusMessage) {
    ActionFeedback::post(*this, statusMessage.isNotEmpty() ? statusMessage : getLabelText() + " updated.");
    if (onValueChanged) {
        onValueChanged(*this);
    }
}
