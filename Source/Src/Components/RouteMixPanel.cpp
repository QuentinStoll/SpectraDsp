#include "Components/RouteMixPanel.h"

#include "Stylesheet.h"

RouteMixPanel::RouteMixPanel(juce::AudioProcessorValueTreeState& state)
    : _state(state) {
    _title.setText("Path mix", juce::dontSendNotification);
    _title.setFont(juce::FontOptions(16.0f));
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_title);

    _modeLabel.setText("Split", juce::dontSendNotification);
    _modeLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    addAndMakeVisible(_modeLabel);
    _mode.addItemList({"Y", "A/B"}, 1);
    addAndMakeVisible(_mode);

    configureSlider(_route, "A/B route");
    configureSlider(_aLevel, "A level");
    configureSlider(_aPan, "A pan");
    configureSlider(_bLevel, "B level");
    configureSlider(_bPan, "B pan");
    configureSlider(_merge, "Merge");
    for (auto* slider : {&_route, &_aLevel, &_aPan, &_bLevel, &_bPan, &_merge}) {
        addAndMakeVisible(slider);
    }
    _polarity.setColour(juce::ToggleButton::textColourId, ProfilerStyle::Colors::text);
    _polarity.setColour(juce::ToggleButton::tickColourId, ProfilerStyle::Colors::accent);
    addAndMakeVisible(_polarity);

    _modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(_state, "splitMode", _mode);
    _routeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(_state, "splitRoute", _route);
    _aLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(_state, "routeALevel", _aLevel);
    _aPanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(_state, "routeAPan", _aPan);
    _bLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(_state, "routeBLevel", _bLevel);
    _bPanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(_state, "routeBPan", _bPan);
    _polarityAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(_state, "routeBPolarity", _polarity);
    _mergeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(_state, "routeLevel", _merge);
}

void RouteMixPanel::configureSlider(juce::Slider& slider, const juce::String& name) {
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 18);
    slider.setColour(juce::Slider::textBoxTextColourId, ProfilerStyle::Colors::text);
    slider.setColour(juce::Slider::textBoxOutlineColourId, ProfilerStyle::Colors::border);
    slider.setColour(juce::Slider::thumbColourId, ProfilerStyle::Colors::accent);
    slider.setColour(juce::Slider::trackColourId, ProfilerStyle::Colors::border);
    slider.setTooltip(name);
    slider.setName(name);
}

void RouteMixPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

void RouteMixPanel::resized() {
    auto area = getLocalBounds().reduced(16, 12);
    auto header = area.removeFromTop(28);
    _title.setBounds(header.removeFromLeft(120));
    _modeLabel.setBounds(header.removeFromLeft(48));
    _mode.setBounds(header.removeFromLeft(90).reduced(0, 2));
    _polarity.setBounds(header.removeFromLeft(150));

    auto place = [&area](juce::Slider& slider) {
        slider.setBounds(area.removeFromTop(32).reduced(0, 3));
    };
    place(_route);
    place(_aLevel);
    place(_aPan);
    place(_bLevel);
    place(_bPan);
    place(_merge);
}
