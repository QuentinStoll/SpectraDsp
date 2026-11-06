#include "Components/ActionStatusBar.h"

#include "Stylesheet.h"

ActionStatusBar::ActionStatusBar() {
    setTitle("Action status");
    _message.setTitle("Latest action");
    _message.setMultiLine(true, true);
    _message.setReadOnly(true);
    _message.setCaretVisible(false);
    _message.setScrollbarsShown(true);
    _message.setFont(ProfilerStyle::Fonts::regular(16.0f));
    _message.setIndents(8, 5);
    _message.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    _message.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    _clear.setTitle("Clear action status");
    _clear.onClick = [this]() { showMessage("Ready for your next action."); };
    addAndMakeVisible(_message);
    addAndMakeVisible(_clear);
    showMessage("Action status messages enabled. Ready for your next action.");
}

void ActionStatusBar::showMessage(const juce::String& message, bool error) {
    _error = error;
    _message.setText((error ? "Error: " : "") + message, false);
    _message.setCaretPosition(0);
    repaint();
}
void ActionStatusBar::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    g.setColour(ProfilerStyle::Contrast::outline(_error ? juce::Colour(0xffff4d4f) : ProfilerStyle::Colors::accent));
    g.fillRect(0, 0, getWidth(), 2);
}
void ActionStatusBar::resized() {
    auto area = getLocalBounds().reduced(8, 6);
    _clear.setBounds(area.removeFromRight(72).withSizeKeepingCentre(72, 32));
    area.removeFromRight(8);
    _message.setBounds(area);
}
