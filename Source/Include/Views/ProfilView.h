#pragma once

#include <JuceHeader.h>

#include "Components/NotificationBanner.h"
#include "PluginProcessor.h"
#include "Views/MarketplaceAccountPanel.h"
#include "Views/MarketplaceGridPanel.h"
#include "Views/PresetListPanel.h"

class ProfilView : public juce::Component,
                   private juce::ChangeListener {
   public:
    ProfilView(ProfilerAudioProcessor& p);
    ~ProfilView() override;

    std::function<void(const juce::String& warning)> onPresetLoaded;

    void downloadAccountProfile(const juce::String& id);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

   private:
    ProfilerAudioProcessor& _audioProcessor;
    PresetListPanel _presetPanel;
    MarketplaceGridPanel _marketplacePanel;
    MarketplaceAccountPanel _accountPanel;
    NotificationBanner _notificationBanner;

    void refreshMarketplace();
    void showMessage(const juce::String& message, bool success);
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
