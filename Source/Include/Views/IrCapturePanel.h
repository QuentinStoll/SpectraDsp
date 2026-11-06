#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

#include "Components/CustomTextButton.h"
#include "Components/CustomTextEditor.h"
#include "MarketplaceClient.h"

class ProfilerAudioProcessor;

class IrCapturePanel : public juce::Component,
                       private juce::Timer {
   public:
    explicit IrCapturePanel(ProfilerAudioProcessor& processor);
    ~IrCapturePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    ProfilerAudioProcessor& _processor;
    Marketplace::Client _client;
    juce::ThreadPool _jobs{1};
    bool _busy = false;

    juce::Label _title;
    juce::Label _body;
    juce::Label _nameLabel;
    CustomTextEditor _nameEditor;
    CustomTextButton _cloneButton{"Clone IR", ProfilerStyle::Theme::Orange};
    juce::Label _status;

    void refreshAccountState(bool updateStatus = true);
    void setStatus(const juce::String& message);
    void startCapture();
    void sealAndUpload();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IrCapturePanel)
};
