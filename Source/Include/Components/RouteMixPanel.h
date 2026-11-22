#pragma once

#include <JuceHeader.h>

class RouteMixPanel : public juce::Component {
   public:
    explicit RouteMixPanel(juce::AudioProcessorValueTreeState& state);
    ~RouteMixPanel() override = default;

    void setFork(int forkId, bool hasPathBBlocks, bool pathBBypassed);
    int getForkId() const noexcept { return _forkId; }

    std::function<void(int forkId, bool bypassed)> onBypassChanged;
    std::function<void(int forkId)> onClearBlock;
    std::function<void()> onClearAll;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& _state;
    int _forkId = -1;
    juce::Label _title;
    juce::Label _modeLabel;
    juce::ComboBox _mode;
    juce::TextButton _bypassButton{"Bypass"};
    juce::TextButton _clearBlockButton{"Clear block"};
    juce::TextButton _clearAllButton{"Clear all"};
    juce::Slider _route;
    juce::Slider _aLevel;
    juce::Slider _aPan;
    juce::Slider _bLevel;
    juce::Slider _bPan;
    juce::ToggleButton _polarity{"Invert path B"};
    juce::Slider _merge;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> _modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _routeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _aLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _aPanAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _bLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _bPanAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> _polarityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _mergeAttachment;

    void configureSlider(juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RouteMixPanel)
};
