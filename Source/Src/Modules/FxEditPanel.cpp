#include "Modules/FxEditPanel.h"

#include "PluginProcessor.h"
#include "Stylesheet.h"

namespace {
juce::String noteLabel(int midi, float cents) {
    if (midi < 0) {
        return "--";
    }
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int octave = midi / 12 - 1;
    return juce::String(names[midi % 12]) + juce::String(octave) + "  " + juce::String(cents, 1) + " ct";
}
}  // namespace

FxEditPanel::FxEditPanel(ProfilerAudioProcessor& processor)
    : _processor(processor) {
    _title.setFont(ProfilerStyle::Fonts::moduleTitle());
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _title.setJustificationType(juce::Justification::centredLeft);
    _algorithm.setFont(ProfilerStyle::Fonts::regular(11.0f));
    _algorithm.setColour(juce::Label::textColourId, ProfilerStyle::Colors::caption);
    _algorithm.setJustificationType(juce::Justification::centredLeft);
    _note.setFont(ProfilerStyle::Fonts::bold(28.0f));
    _note.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _note.setJustificationType(juce::Justification::centredLeft);
    _cents.setFont(ProfilerStyle::Fonts::medium(13.0f));
    _cents.setColour(juce::Label::textColourId, ProfilerStyle::Colors::rigTuner);
    _poly.setFont(ProfilerStyle::Fonts::regular(12.0f));
    _poly.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    addAndMakeVisible(_title);
    addAndMakeVisible(_algorithm);
    _bypass.setClickingTogglesState(true);
    _bypass.onClick = [this]() {
        if (!SignalChain::isMovableSlot(_chainSlot)) {
            return;
        }
        _processor.setChainSlotBypassed(_chainSlot, !_bypass.getToggleState());
    };
    addChildComponent(_bypass);
    addChildComponent(_note);
    addChildComponent(_cents);
    addChildComponent(_poly);
}

void FxEditPanel::visibilityChanged() {
    if (isVisible() && _stage == SignalChain::Stage::Tuner) {
        startTimerHz(30);
    } else if (!isVisible()) {
        stopTimer();
    }
}

void FxEditPanel::refreshBypass() {
    if (!isVisible() || Fx::moduleFor(_stage) == nullptr) {
        return;
    }
    const bool enabled = !SignalChain::isMovableSlot(_chainSlot) || !_processor.isChainSlotBypassed(_chainSlot);
    _bypass.setToggleState(enabled, juce::dontSendNotification);
}

void FxEditPanel::setStage(SignalChain::Stage stage, int chainSlot) {
    _chainSlot = chainSlot;
    if (stage == _stage && _bypass.isVisible() && Fx::moduleFor(stage) != nullptr) {
        refreshBypass();
        return;
    }
    _stage = stage;
    rebuild();
    if (stage == SignalChain::Stage::Tuner && isVisible()) {
        startTimerHz(30);
    } else {
        stopTimer();
    }
}

void FxEditPanel::rebuild() {
    _knobs.clear();
    _choices.clear();
    _flags.clear();
    _order.clear();

    const auto* module = Fx::moduleFor(_stage);
    if (module == nullptr) {
        _title.setText({}, juce::dontSendNotification);
        _algorithm.setText({}, juce::dontSendNotification);
        _bypass.setVisible(false);
        _note.setVisible(false);
        _cents.setVisible(false);
        _poly.setVisible(false);
        return;
    }

    _title.setText(juce::String(module->name).toUpperCase(), juce::dontSendNotification);
    _algorithm.setText(module->algorithm, juce::dontSendNotification);
    _bypass.setVisible(true);
    refreshBypass();
    const bool tuner = _stage == SignalChain::Stage::Tuner;
    _note.setVisible(tuner);
    _cents.setVisible(tuner);
    _poly.setVisible(tuner);

    for (int index = 1; index < module->paramCount; ++index) {
        const auto& spec = module->params[index];
        if (spec.type == Fx::ParamType::Float) {
            auto knob = std::make_unique<CustomKnob>(spec.label, spec.minimum, spec.maximum, spec.defaultValue,
                                                     spec.suffix != nullptr ? spec.suffix : "", spec.step);
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                _processor._apvts, spec.id, knob->getSlider());
            addAndMakeVisible(*knob);
            _order.push_back(knob.get());
            _knobs.push_back({std::move(knob), std::move(attachment)});
        } else if (spec.type == Fx::ParamType::Choice) {
            auto label = std::make_unique<juce::Label>();
            label->setText(juce::String(spec.label).toUpperCase(), juce::dontSendNotification);
            label->setFont(ProfilerStyle::Fonts::micro());
            label->setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
            label->setJustificationType(juce::Justification::centred);
            auto combo = std::make_unique<CustomComboBox>();
            juce::StringArray choices;
            choices.addTokens(spec.choices != nullptr ? spec.choices : "", "|", "");
            combo->addItemList(choices, 1);
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                _processor._apvts, spec.id, *combo);
            addAndMakeVisible(*label);
            addAndMakeVisible(*combo);
            _order.push_back(combo.get());
            _choices.push_back({std::move(label), std::move(combo), std::move(attachment)});
        } else {
            auto toggle = std::make_unique<CustomToggleButton>(spec.label);
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                _processor._apvts, spec.id, *toggle);
            addAndMakeVisible(*toggle);
            _order.push_back(toggle.get());
            _flags.push_back({std::move(toggle), std::move(attachment)});
        }
    }
    resized();
}

void FxEditPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    if (_stage == SignalChain::Stage::Tuner) {
        paintStrobe(g, getLocalBounds().reduced(16, 8).removeFromRight(220).withTrimmedTop(36));
    }
}

void FxEditPanel::paintStrobe(juce::Graphics& g, juce::Rectangle<int> area) const {
    auto bar = area.removeFromTop(28).toFloat();
    g.setColour(ProfilerStyle::Colors::border);
    g.fillRoundedRectangle(bar, 4.0f);
    const float x = bar.getCentreX() + std::sin(_tuner.strobe) * bar.getWidth() * 0.42f;
    const bool locked = _tuner.count > 0 && std::abs(_tuner.cents[0]) < 1.2f;
    g.setColour(locked ? juce::Colour(0xff34D399) : ProfilerStyle::Colors::accent);
    g.fillRoundedRectangle(juce::Rectangle<float>(4.0f, bar.getHeight() - 8.0f).withCentre({x, bar.getCentreY()}), 2.0f);
}

void FxEditPanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    if (_bypass.isVisible()) {
        _bypass.setBounds(header.removeFromRight(52).withSizeKeepingCentre(52, 20));
        header.removeFromRight(8);
    }
    _title.setBounds(header);
    _algorithm.setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    if (_stage == SignalChain::Stage::Tuner) {
        auto readout = area.removeFromLeft(juce::jmax(180, area.getWidth() - 240));
        _note.setBounds(readout.removeFromTop(36));
        _cents.setBounds(readout.removeFromTop(18));
        _poly.setBounds(readout.removeFromTop(36));
    }

    if (_order.empty() || area.isEmpty()) {
        return;
    }
    const int columns = juce::jmin((int)_order.size(), area.getWidth() > 760 ? 8 : 6);
    const int rows = ((int)_order.size() + columns - 1) / columns;
    const int gap = 8;
    const int cellW = juce::jmax(48, (area.getWidth() - gap * (columns - 1)) / columns);
    const int cellH = juce::jmax(36, (area.getHeight() - gap * (rows - 1)) / rows);
    for (int index = 0; index < (int)_order.size(); ++index) {
        const int column = index % columns;
        const int row = index / columns;
        auto bounds = juce::Rectangle<int>(area.getX() + column * (cellW + gap),
                                           area.getY() + row * (cellH + gap),
                                           cellW,
                                           cellH);
        _order[(size_t)index]->setBounds(bounds);
        for (auto& choice : _choices) {
            if (choice.combo.get() == _order[(size_t)index]) {
                choice.label->setBounds(bounds.removeFromTop(14));
                bounds.removeFromTop(2);
                choice.combo->setBounds(bounds.withSizeKeepingCentre(juce::jmin(bounds.getWidth(), 120), 22));
            }
        }
    }
}

void FxEditPanel::timerCallback() {
    if (_stage != SignalChain::Stage::Tuner) {
        stopTimer();
        return;
    }
    Fx::TunerSnapshot snapshot;
    if (_processor.readTuner(snapshot)) {
        _tuner = snapshot;
    }
    if (_tuner.count <= 0) {
        _note.setText("--", juce::dontSendNotification);
        _cents.setText("No note", juce::dontSendNotification);
        _poly.setText({}, juce::dontSendNotification);
    } else {
        _note.setText(noteLabel(_tuner.midi[0], _tuner.cents[0]), juce::dontSendNotification);
        const bool locked = std::abs(_tuner.cents[0]) < 1.2f;
        _cents.setText(locked ? "In tune" : (_tuner.cents[0] > 0.0f ? "Sharp" : "Flat"), juce::dontSendNotification);
        juce::String others;
        for (int index = 1; index < _tuner.count; ++index) {
            if (others.isNotEmpty()) {
                others << "   ";
            }
            others << noteLabel(_tuner.midi[index], _tuner.cents[index]);
        }
        _poly.setText(others, juce::dontSendNotification);
    }
    repaint();
}
