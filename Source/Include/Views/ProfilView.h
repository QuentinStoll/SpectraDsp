#pragma once

#include <JuceHeader.h>

#include "Components/ModalOverlay.h"
#include "Components/NotificationBanner.h"
#include "Modules/CardGridModule.h"
#include "PluginProcessor.h"
#include "Views/MarketplaceAccountPanel.h"

class ProfilView : public juce::Component,
                   private juce::ChangeListener {
   public:
    ProfilView(ProfilerAudioProcessor& p);
    ~ProfilView();

    void downloadAccountProfile(const juce::String& id);

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::Viewport _viewport;
    juce::Component _libraryPage;
    MarketplaceAccountPanel _accountPanel;
    juce::Label _presetHeading;
    CardGridModule _grid;
    ModalOverlay _modalOverlay;
    NotificationBanner _notificationBanner;
    ProfilerAudioProcessor& _audioProcessor;

    void showDeletePresetModal(int presetNumber);
    void deletePreset(int presetNumber);
    void saveCurrentChain();
    void refreshPresetGrid();
    bool storeEncryptedProfile(const juce::String& accessToken,
                               const juce::String& id,
                               const juce::String& title,
                               const juce::String& kind,
                               const juce::MemoryBlock& cipher,
                               const juce::MemoryBlock& key,
                               int conditionProfile,
                               float conditionAmount,
                               juce::String* errorMessage);
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
};
