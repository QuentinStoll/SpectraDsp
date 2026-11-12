#pragma once

#include <JuceHeader.h>

#include <functional>

#include "Components/CustomComboBox.h"
#include "Components/CustomTextButton.h"
#include "Views/AmpCapturePanel.h"
#include "Views/IrCapturePanel.h"

class ProfilerAudioProcessor;

class SettingsView : public juce::Component {
   public:
    explicit SettingsView(ProfilerAudioProcessor& processor);

    static void applySavedBackgroundColour();
    std::function<void(int)> onInterfaceScaleChanged;
    std::function<void(bool)> onActionStatusChanged;
    void refreshContrastSetting();

    std::function<void()> onCheckForUpdates;

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
    juce::Label _interfaceScaleLabel;
    CustomComboBox _interfaceScaleMenu;
    juce::Label _contrastLabel;
    CustomComboBox _contrastMenu;
    juce::Label _actionStatusLabel;
    CustomComboBox _actionStatusMenu;
    juce::Label _troubleshootingLabel;
    CustomComboBox _troubleshootingMenu;
    juce::Label _hardwareInfoLabel;
    CustomComboBox _hardwareInfoMenu;
    juce::Label _versionLabel;
    CustomTextButton _updateButton{"Check for updates"};

    void applyBackgroundColour();
    void refreshColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsView)
};
