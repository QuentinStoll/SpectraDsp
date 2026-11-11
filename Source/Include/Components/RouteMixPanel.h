#pragma once

#include <JuceHeader.h>

class RouteMixPanel : public juce::Component {
   public:
    explicit RouteMixPanel(juce::AudioProcessorValueTreeState& state);
    ~RouteMixPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& _state;
    juce::Label _title;
    juce::Label _modeLabel;
    juce::ComboBox _mode;
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
