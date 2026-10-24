#pragma once

#include <JuceHeader.h>

#include "Components/CustomComboBox.h"
#include "Views/AmpCapturePanel.h"
#include "Views/IrCapturePanel.h"

class ProfilerAudioProcessor;

class SettingsView : public juce::Component {
   public:
    explicit SettingsView(ProfilerAudioProcessor& processor);

    static void applySavedBackgroundColour();

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    enum class Page {
        General,
        CloneIr,
        CloneAmp
    };

    juce::TextButton _generalTab{"General"};
    juce::TextButton _captureTab{"Clone IR"};
    juce::TextButton _ampTab{"Clone amp"};
    Page _page = Page::General;
    IrCapturePanel _capturePanel;
    AmpCapturePanel _ampPanel;

    juce::Label _titleLabel;
    juce::Label _backgroundLabel;
    CustomComboBox _backgroundMenu;
    juce::Label _troubleshootingLabel;
    CustomComboBox _troubleshootingMenu;
    juce::Label _hardwareInfoLabel;
    CustomComboBox _hardwareInfoMenu;

    void applyBackgroundColour();
    void refreshColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsView)
};
