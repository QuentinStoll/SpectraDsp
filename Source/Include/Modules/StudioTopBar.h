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

    std::function<void()> onSettingsClicked;
    std::function<void()> onLibraryClicked;
    std::function<void()> onPresetApplied;
    std::function<void(const juce::String&, bool)> onStatus;
    std::function<void(const juce::String&)> onResetCompleted;

    juce::String restoreDefaults();

   private:
    class IconButton : public juce::Button {
       public:
        enum class Icon {
            Gear,
            Library
        };

        IconButton(const juce::String& name, Icon icon);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

       private:
        Icon _icon;
    };

    ProfilerAudioProcessor& _audioProcessor;
    juce::AudioProcessorValueTreeState& _apvts;

    juce::Label _logoLabel;
    CustomComboBox _presetMenu;
    IconButton _libraryButton{"Library", IconButton::Icon::Library};
    CustomTextButton _muteButton{"MUTE"};
    IconButton _settingsButton{"Settings", IconButton::Icon::Gear};

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> _muteAttach;
    bool _isUpdatingProfileMenu = false;

    void refreshProfileMenu();
    void restoreLastUsedProfile();
    void selectProfileFromMenu();
    void resetParametersToDefaults();
    void resetLoadedFiles();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioTopBar)
};
