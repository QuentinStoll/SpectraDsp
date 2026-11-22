#pragma once

#include <JuceHeader.h>

#include "Components/CustomComboBox.h"
#include "Components/CustomTextButton.h"

class ProfilerAudioProcessor;

class StudioTopBar : public juce::Component,
                     private juce::ChangeListener {
   public:
    explicit StudioTopBar(ProfilerAudioProcessor& processor);
    ~StudioTopBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    std::function<void()> onSettingsClicked;
    std::function<void()> onLibraryClicked;
    std::function<void()> onLogoClicked;
    std::function<void()> onPresetApplied;
    std::function<void(const juce::String&, bool)> onStatus;
    std::function<void(const juce::String&)> onResetCompleted;

   private:
    class GearButton : public juce::Button {
       public:
        explicit GearButton(const juce::String& name);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class PresetTitleBox : public CustomComboBox {
       public:
        std::function<void()> onOpenRequested;
        void showPopup() override;
    };

    ProfilerAudioProcessor& _audioProcessor;
    juce::AudioProcessorValueTreeState& _apvts;

    juce::ImageComponent _logoMark;
    juce::Label _logoLabel;
    PresetTitleBox _presetMenu;
    CustomTextButton _muteButton{"MUTE"};
    GearButton _settingsButton{"Clone"};

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> _muteAttach;
    bool _isUpdatingProfileMenu = false;

    juce::Rectangle<int> logoArea() const;
    void refreshProfileMenu();
    void restoreLastUsedProfile();
    void selectProfileFromMenu();
    void resetParametersToDefaults();
    void resetLoadedFiles();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioTopBar)
};
