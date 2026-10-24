#pragma once

#include <JuceHeader.h>

#include <memory>
#include <optional>

#include "AmpCapture.h"
#include "Components/CustomTextButton.h"
#include "Components/CustomTextEditor.h"

class ProfilerAudioProcessor;

class AmpCapturePanel : public juce::Component,
                        private juce::Timer {
   public:
    explicit AmpCapturePanel(ProfilerAudioProcessor& processor);
    ~AmpCapturePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    enum class Mode {
        None,
        RoundTrip,
        Full
    };

    ProfilerAudioProcessor& _processor;
    juce::ThreadPool _jobs{1};
    std::unique_ptr<juce::FileChooser> _chooser;
    juce::File _reampFile;
    juce::File _lastFolder;
    Mode _mode = Mode::None;
    bool _busy = false;
    bool _stopRequested = false;
    bool _auto = false;
    bool _polling = false;
    bool _pollBusy = false;
    int _roundTrips = 0;
    int _pollTick = 0;
    juce::String _cloudJobId;
    juce::String _cloudTitle;
    std::optional<int> _lastRoundTrip;

    juce::Label _title;
    juce::Label _body;
    juce::Label _reampLabel;
    juce::Label _reampPath;
    CustomTextButton _chooseButton{"Choose", ProfilerStyle::Theme::Dark};
    juce::Label _nameLabel;
    CustomTextEditor _nameEditor;
    CustomTextButton _roundTripButton{"Test round trip", ProfilerStyle::Theme::Dark};
    CustomTextButton _captureButton{"Capture amp", ProfilerStyle::Theme::Orange};
    CustomTextButton _stopButton{"Stop", ProfilerStyle::Theme::Dark};
    CustomTextButton _folderButton{"Open folder", ProfilerStyle::Theme::Dark};
    juce::TextEditor _status;

    void chooseReamp();
    void cloneAmp();
    void setStatus(const juce::String& text);
    void start(Mode mode);
    void uploadClone(std::shared_ptr<std::vector<float>> recorded, AmpCapture::Analysis analysis, juce::String name);
    void pollCloud();
    void finish();
    void showResult(const juce::String& text);
    void refreshButtons();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpCapturePanel)
};
