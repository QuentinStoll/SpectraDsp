#pragma once

#include <JuceHeader.h>

class ActionStatusBar : public juce::Component {
   public:
    ActionStatusBar();
    void showMessage(const juce::String& message, bool error = false);
    juce::String getMessage() const { return _message.getText(); }
    void paint(juce::Graphics&) override;
    void resized() override;

   private:
    juce::TextEditor _message;
    juce::TextButton _clear{"Clear"};
    bool _error = false;
};
