#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "Components/CustomComboBox.h"
#include "Components/CustomKnob.h"
#include "Components/CustomToggleButton.h"
#include "Fx/FxCatalog.h"
#include "Fx/TunerSnapshot.h"

class ProfilerAudioProcessor;

class FxEditPanel : public juce::Component,
                    private juce::Timer {
   public:
    explicit FxEditPanel(ProfilerAudioProcessor& processor);
    ~FxEditPanel() override = default;

    void setStage(SignalChain::Stage stage, int chainSlot);
    void refreshBypass();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

   private:
    struct KnobControl {
        std::unique_ptr<CustomKnob> knob;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct ChoiceControl {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<CustomComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    struct BoolControl {
        std::unique_ptr<CustomToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    ProfilerAudioProcessor& _processor;
    SignalChain::Stage _stage = SignalChain::Stage::Empty;
    int _chainSlot = -1;
    juce::Label _title;
    juce::Label _algorithm;
    CustomToggleButton _bypass{"On"};
    std::vector<KnobControl> _knobs;
    std::vector<ChoiceControl> _choices;
    std::vector<BoolControl> _flags;
    std::vector<juce::Component*> _order;
    juce::Label _note;
    juce::Label _cents;
    juce::Label _poly;
    Fx::TunerSnapshot _tuner;

    void timerCallback() override;
    void rebuild();
    void paintStrobe(juce::Graphics& g, juce::Rectangle<int> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxEditPanel)
};
