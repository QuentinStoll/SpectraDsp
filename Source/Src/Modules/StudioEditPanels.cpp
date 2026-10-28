#include "Modules/StudioEditPanels.h"

#include <array>
#include <cmath>

#include "EqBandLayout.h"
#include "PluginProcessor.h"
#include "Stylesheet.h"

namespace {
void configurePanelTitle(juce::Label& label, const juce::String& text) {
    label.setText(text.toUpperCase(), juce::dontSendNotification);
    label.setFont(ProfilerStyle::Fonts::moduleTitle());
    label.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setInterceptsMouseClicks(false, false);
}

void configureBypassLabel(juce::Label& label) {
    label.setText(label.getText().toUpperCase(), juce::dontSendNotification);
    label.setFont(ProfilerStyle::Fonts::control());
    label.setColour(juce::Label::textColourId, juce::Colour(0xffffb020));
    label.setJustificationType(juce::Justification::centredRight);
    label.setInterceptsMouseClicks(false, false);
}

FileDropZone::Options makeAmpDropOptions(ProfilerAudioProcessor& processor) {
    FileDropZone::Options options;
    options.title = "Amp Model";
    options.emptyDetail = "No Model Loaded";
    options.chooserTitle = "Select an Amp file";
    options.filePattern = "*.nam;*.json;*.txt";
    options.icon = CustomLookAndFeel::RigIcon::AmpHead;
    options.categoryColour = ProfilerStyle::Colors::rigAmp;
    options.loadFile = [&processor](const juce::File& file) {
        return processor.loadAmpFile(file);
    };
    options.unloadFile = [&processor]() {
        processor.unloadAmpFile();
    };
    options.isLoaded = [&processor]() {
        return processor.isAmpFileLoaded();
    };
    options.getCurrentFile = [&processor]() {
        return processor.getCurrentAmpFile();
    };
    options.successMessage = "Amp loaded";
    options.failureMessage = "Could not load amp file";
    return options;
}

FileDropZone::Options makeIrDropOptions(ProfilerAudioProcessor& processor) {
    FileDropZone::Options options;
    options.title = "Impulse Response";
    options.emptyDetail = "No IR Loaded";
    options.chooserTitle = "Select an IR file";
    options.filePattern = "*.wav;*.aiff;*.aif;*.flac";
    options.icon = CustomLookAndFeel::RigIcon::Cabinet;
    options.categoryColour = ProfilerStyle::Colors::rigCab;
    options.loadFile = [&processor](const juce::File& file) {
        return processor.loadIRFile(file);
    };
    options.unloadFile = [&processor]() {
        processor.unloadIRFile();
    };
    options.isLoaded = [&processor]() {
        return processor.isIRLoaded();
    };
    options.getCurrentFile = [&processor]() {
        return processor.getCurrentIRFile();
    };
    options.successMessage = "IR loaded";
    options.failureMessage = "Could not load IR file";
    return options;
}
}  // namespace

namespace {
constexpr double kEqDisplaySampleRate = 48000.0;
constexpr float kEqHandleRadius = 3.4f;
constexpr float kEqHitRadius = 12.0f;

float eqParamValue(juce::AudioProcessorValueTreeState& apvts, const char* parameterId, float fallback) {
    if (const auto* value = apvts.getRawParameterValue(parameterId)) {
        return value->load();
    }

    return fallback;
}

void addBandMagnitude(juce::dsp::IIR::Coefficients<float>::Ptr coefficients,
                      const double* frequencies,
                      double* magnitudes,
                      int numPoints) {
    if (coefficients == nullptr) {
        return;
    }

    std::array<double, 256> band{};
    band.fill(1.0);
    coefficients->getMagnitudeForFrequencyArray(frequencies, band.data(), static_cast<size_t>(numPoints), kEqDisplaySampleRate);
    for (int i = 0; i < numPoints; ++i) {
        magnitudes[i] *= band[static_cast<size_t>(i)];
    }
}

juce::String formatHz(float hz) {
    if (hz >= 1000.0f) {
        return juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    }

    return juce::String(juce::roundToInt(hz)) + " Hz";
}
}  // namespace

EqResponseDisplay::EqResponseDisplay(juce::AudioProcessorValueTreeState& apvts)
    : _apvts(apvts) {
    listenToEqParams(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

EqResponseDisplay::~EqResponseDisplay() {
    endBandGesture();
    listenToEqParams(false);
}

void EqResponseDisplay::listenToEqParams(bool shouldListen) {
    auto apply = [this, shouldListen](const char* parameterId) {
        if (shouldListen) {
            _apvts.addParameterListener(parameterId, this);
        } else {
            _apvts.removeParameterListener(parameterId, this);
        }
    };

    for (const auto& band : EqBands::specs) {
        apply(band.gainId);
        apply(band.freqId);
    }
    apply("isEqEnabled");
}

void EqResponseDisplay::setBypassed(bool shouldBeBypassed) {
    if (_bypassed == shouldBeBypassed) {
        return;
    }

    _bypassed = shouldBeBypassed;
    invalidateEqCurve();
    repaint();
}

void EqResponseDisplay::parameterChanged(const juce::String&, float) {
    invalidateEqCurve();
    requestRepaint();
}

void EqResponseDisplay::requestRepaint() {
    if (juce::MessageManager::existsAndIsCurrentThread()) {
        _repaintPosted.store(false, std::memory_order_relaxed);
        repaint();
        return;
    }

    bool expected = false;
    if (!_repaintPosted.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<EqResponseDisplay>(this)]() {
        if (safeThis == nullptr) {
            return;
        }

        safeThis->_repaintPosted.store(false, std::memory_order_relaxed);
        safeThis->repaint();
    });
}

void EqResponseDisplay::invalidateEqCurve() {
    _curveValid = false;
}

void EqResponseDisplay::ensureEqCurve() const {
    if (_curveValid) {
        return;
    }

    _curveMagnitudes.fill(1.0);
    if (!_bypassed) {
        std::array<double, kCurvePoints> frequencies{};
        const auto logMin = std::log10(static_cast<double>(EqBands::minHz));
        const auto logMax = std::log10(static_cast<double>(EqBands::maxHz));
        for (int i = 0; i < kCurvePoints; ++i) {
            const auto t = static_cast<double>(i) / static_cast<double>(kCurvePoints - 1);
            frequencies[static_cast<size_t>(i)] = std::pow(10.0, logMin + t * (logMax - logMin));
        }

        for (int band = 0; band < EqBands::count; ++band) {
            const auto& spec = EqBands::specs[band];
            const auto freq = juce::jlimit(EqBands::minHz, static_cast<float>(kEqDisplaySampleRate * 0.45), bandFreq(band));
            const auto gain = juce::Decibels::decibelsToGain(bandGain(band));
            juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
            switch (spec.type) {
                case EqBands::Type::LowShelf:
                    coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                        kEqDisplaySampleRate, freq, EqBands::shelfQ, gain);
                    break;
                case EqBands::Type::HighShelf:
                    coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                        kEqDisplaySampleRate, freq, EqBands::shelfQ, gain);
                    break;
                case EqBands::Type::Peak:
                default:
                    coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                        kEqDisplaySampleRate, freq, EqBands::peakQ, gain);
                    break;
            }
            addBandMagnitude(coefficients, frequencies.data(), _curveMagnitudes.data(), kCurvePoints);
        }
    }

    _curveValid = true;
}

float EqResponseDisplay::curveDbAt(float hz) const {
    ensureEqCurve();
    const auto clamped = juce::jlimit(EqBands::minHz, EqBands::maxHz, hz);
    const auto t = std::log10(clamped / EqBands::minHz) / std::log10(EqBands::maxHz / EqBands::minHz);
    const auto position = t * static_cast<float>(kCurvePoints - 1);
    const auto index = juce::jlimit(0, kCurvePoints - 2, static_cast<int>(std::floor(position)));
    const auto frac = position - static_cast<float>(index);
    const auto mag0 = static_cast<float>(_curveMagnitudes[static_cast<size_t>(index)]);
    const auto mag1 = static_cast<float>(_curveMagnitudes[static_cast<size_t>(index + 1)]);
    return juce::Decibels::gainToDecibels(mag0 + (mag1 - mag0) * frac, -EqBands::maxDb);
}

juce::Rectangle<float> EqResponseDisplay::plotArea() const {
    auto bounds = getLocalBounds().toFloat();
    constexpr float leftGutter = 28.0f;
    constexpr float bottomGutter = 16.0f;
    constexpr float rightPad = 10.0f;
    constexpr float topPad = 8.0f;
    return {bounds.getX() + leftGutter,
            bounds.getY() + topPad,
            bounds.getWidth() - leftGutter - rightPad,
            bounds.getHeight() - topPad - bottomGutter};
}

float EqResponseDisplay::xForHz(float hz) const {
    const auto plot = plotArea();
    const auto clamped = juce::jlimit(EqBands::minHz, EqBands::maxHz, hz);
    const auto t = std::log10(clamped / EqBands::minHz) / std::log10(EqBands::maxHz / EqBands::minHz);
    return plot.getX() + t * plot.getWidth();
}

float EqResponseDisplay::yForDb(float db) const {
    const auto plot = plotArea();
    const auto normalised = juce::jmap(juce::jlimit(EqBands::minDb, EqBands::maxDb, db),
                                       EqBands::minDb, EqBands::maxDb, 1.0f, 0.0f);
    return plot.getY() + normalised * plot.getHeight();
}

float EqResponseDisplay::hzFromX(float x) const {
    const auto plot = plotArea();
    const auto t = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
    return EqBands::minHz * std::pow(EqBands::maxHz / EqBands::minHz, t);
}

float EqResponseDisplay::dbFromY(float y) const {
    const auto plot = plotArea();
    const auto t = juce::jlimit(0.0f, 1.0f, (y - plot.getY()) / juce::jmax(1.0f, plot.getHeight()));
    return juce::jmap(t, 0.0f, 1.0f, EqBands::maxDb, EqBands::minDb);
}

float EqResponseDisplay::bandGain(int band) const {
    return eqParamValue(_apvts, EqBands::specs[band].gainId, EqBands::specs[band].defaultDb);
}

float EqResponseDisplay::bandFreq(int band) const {
    return eqParamValue(_apvts, EqBands::specs[band].freqId, EqBands::specs[band].defaultHz);
}

juce::Point<float> EqResponseDisplay::bandPoint(int band) const {
    const auto hz = bandFreq(band);
    return {xForHz(hz), yForDb(curveDbAt(hz))};
}

int EqResponseDisplay::findBandAt(juce::Point<float> pos) const {
    int best = -1;
    auto bestDistance = kEqHitRadius;
    for (int band = 0; band < EqBands::count; ++band) {
        const auto distance = pos.getDistanceFrom(bandPoint(band));
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = band;
        }
    }
    return best;
}

void EqResponseDisplay::setRangedParam(const char* parameterId, float value) {
    auto* parameter = _apvts.getParameter(parameterId);
    if (parameter == nullptr) {
        return;
    }

    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void EqResponseDisplay::setBandFromPosition(int band, juce::Point<float> pos) {
    setRangedParam(EqBands::specs[band].freqId, hzFromX(pos.x));
    setRangedParam(EqBands::specs[band].gainId, dbFromY(pos.y));
}

void EqResponseDisplay::beginBandGesture(int band) {
    if (_gestureActive) {
        return;
    }

    if (auto* gain = _apvts.getParameter(EqBands::specs[band].gainId)) {
        gain->beginChangeGesture();
    }
    if (auto* freq = _apvts.getParameter(EqBands::specs[band].freqId)) {
        freq->beginChangeGesture();
    }
    _gestureActive = true;
}

void EqResponseDisplay::endBandGesture() {
    if (!_gestureActive || _dragBand < 0) {
        _gestureActive = false;
        _dragBand = -1;
        return;
    }

    if (auto* gain = _apvts.getParameter(EqBands::specs[_dragBand].gainId)) {
        gain->endChangeGesture();
    }
    if (auto* freq = _apvts.getParameter(EqBands::specs[_dragBand].freqId)) {
        freq->endChangeGesture();
    }
    _gestureActive = false;
    _dragBand = -1;
}

void EqResponseDisplay::resetBand(int band) {
    const auto& spec = EqBands::specs[band];
    if (auto* gain = _apvts.getParameter(spec.gainId)) {
        gain->setValueNotifyingHost(gain->getDefaultValue());
    }
    if (auto* freq = _apvts.getParameter(spec.freqId)) {
        freq->setValueNotifyingHost(freq->getDefaultValue());
    }
}

void EqResponseDisplay::updateHover(juce::Point<float> pos) {
    const auto hovered = findBandAt(pos);
    if (_hoverBand != hovered) {
        _hoverBand = hovered;
        setMouseCursor(hovered >= 0 ? juce::MouseCursor::DraggingHandCursor
                                    : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void EqResponseDisplay::mouseDown(const juce::MouseEvent& event) {
    _dragBand = findBandAt(event.position);
    if (_dragBand < 0) {
        return;
    }

    beginBandGesture(_dragBand);
    _hoverBand = _dragBand;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void EqResponseDisplay::mouseDrag(const juce::MouseEvent& event) {
    if (_dragBand < 0) {
        return;
    }

    auto pos = event.position;
    if (event.mods.isShiftDown()) {
        const auto current = bandPoint(_dragBand);
        pos = current + (pos - current) * 0.18f;
    }
    setBandFromPosition(_dragBand, pos);
}

void EqResponseDisplay::mouseUp(const juce::MouseEvent& event) {
    endBandGesture();
    updateHover(event.position);
}

void EqResponseDisplay::mouseMove(const juce::MouseEvent& event) {
    updateHover(event.position);
}

void EqResponseDisplay::mouseExit(const juce::MouseEvent&) {
    if (_dragBand >= 0) {
        return;
    }

    if (_hoverBand != -1) {
        _hoverBand = -1;
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void EqResponseDisplay::mouseDoubleClick(const juce::MouseEvent& event) {
    const auto band = findBandAt(event.position);
    if (band >= 0) {
        resetBand(band);
    }
}

void EqResponseDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff08080A));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour(0xff2A2A32));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    const auto plot = plotArea();
    if (plot.getWidth() < 12.0f || plot.getHeight() < 12.0f) {
        return;
    }

    g.setColour(juce::Colour(0xff16161C));
    const float dbLines[] = {18.0f, 12.0f, 6.0f, -6.0f, -12.0f, -18.0f};
    for (const auto db : dbLines) {
        g.drawHorizontalLine(juce::roundToInt(yForDb(db)), plot.getX(), plot.getRight());
    }

    const float markerHz[] = {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f};
    for (const auto hz : markerHz) {
        g.drawVerticalLine(juce::roundToInt(xForHz(hz)), plot.getY(), plot.getBottom());
    }

    g.setColour(juce::Colour(0xff3A3A46));
    g.drawHorizontalLine(juce::roundToInt(yForDb(0.0f)), plot.getX(), plot.getRight());

    ensureEqCurve();

    juce::Path curve;
    juce::Path fill;
    fill.startNewSubPath(plot.getX(), yForDb(0.0f));
    for (int i = 0; i < kCurvePoints; ++i) {
        const auto t = static_cast<float>(i) / static_cast<float>(kCurvePoints - 1);
        const auto x = plot.getX() + t * plot.getWidth();
        const auto db = juce::Decibels::gainToDecibels(static_cast<float>(_curveMagnitudes[static_cast<size_t>(i)]), -EqBands::maxDb);
        const auto y = yForDb(db);
        if (i == 0) {
            curve.startNewSubPath(x, y);
        } else {
            curve.lineTo(x, y);
        }
        fill.lineTo(x, y);
    }
    fill.lineTo(plot.getRight(), yForDb(0.0f));
    fill.closeSubPath();

    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(plot.toNearestInt());
        auto gradient = juce::ColourGradient(ProfilerStyle::Colors::rigEq.withAlpha(_bypassed ? 0.04f : 0.20f),
                                             plot.getCentreX(), plot.getY(),
                                             ProfilerStyle::Colors::rigEq.withAlpha(0.0f),
                                             plot.getCentreX(), plot.getBottom(),
                                             false);
        g.setGradientFill(gradient);
        g.fillPath(fill);
        g.setColour(ProfilerStyle::Colors::rigEq.withAlpha(_bypassed ? 0.32f : 0.96f));
        g.strokePath(curve, juce::PathStrokeType(_bypassed ? 1.0f : 1.4f,
                                                 juce::PathStrokeType::mitered,
                                                 juce::PathStrokeType::rounded));
    }

    const auto activeBand = _dragBand >= 0 ? _dragBand : _hoverBand;
    if (activeBand >= 0) {
        const auto markerX = xForHz(bandFreq(activeBand));
        g.setColour(ProfilerStyle::Colors::rigEq.withAlpha(0.28f));
        g.drawVerticalLine(juce::roundToInt(markerX), plot.getY(), plot.getBottom());
    }

    for (int band = 0; band < EqBands::count; ++band) {
        const auto centre = bandPoint(band);
        const auto selected = band == _dragBand || band == _hoverBand;
        const auto radius = selected ? kEqHandleRadius + 0.7f : kEqHandleRadius;
        auto handle = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
        g.setColour(ProfilerStyle::Colors::rigEq.withAlpha(_bypassed ? 0.35f : (selected ? 1.0f : 0.95f)));
        g.fillEllipse(handle);
        g.setColour(juce::Colour(0xff08080A).withAlpha(0.55f));
        g.drawEllipse(handle, 0.9f);
    }

    g.setFont(ProfilerStyle::Fonts::micro());
    g.setColour(ProfilerStyle::Colors::caption.withAlpha(0.55f));
    const float dbLabels[] = {12.0f, 0.0f, -12.0f};
    for (const auto db : dbLabels) {
        auto label = juce::Rectangle<float>(bounds.getX() + 2.0f, yForDb(db) - 6.0f, 24.0f, 12.0f);
        g.drawText(db == 0.0f ? "0" : juce::String(static_cast<int>(db)),
                   label, juce::Justification::centredRight, false);
    }

    const struct {
        float hz;
        const char* text;
    } freqLabels[] = {{100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}};
    for (const auto& freq : freqLabels) {
        g.drawText(freq.text,
                   juce::Rectangle<float>(xForHz(freq.hz) - 16.0f, plot.getBottom() + 1.0f, 32.0f, 14.0f),
                   juce::Justification::centred, false);
    }

    if (activeBand >= 0) {
        const auto hz = bandFreq(activeBand);
        const auto db = bandGain(activeBand);
        const auto readout = formatHz(hz) + "  " + juce::String(db, 1) + " dB";
        g.setColour(ProfilerStyle::Colors::text.withAlpha(0.82f));
        g.drawText(readout,
                   juce::Rectangle<float>(plot.getX(), plot.getY() + 2.0f, plot.getWidth() - 4.0f, 14.0f),
                   juce::Justification::centredRight, false);
    }
}

InputGatePanel::InputGatePanel(juce::AudioProcessorValueTreeState& apvts) {
    configurePanelTitle(_title, "Input");
    configureBypassLabel(_bypassLabel);
    _summary.setFont(ProfilerStyle::Fonts::regular(12.0f));
    _summary.setColour(juce::Label::textColourId, ProfilerStyle::Colors::caption);
    _summary.setJustificationType(juce::Justification::centredLeft);
    _summary.setInterceptsMouseClicks(false, false);
    _gateEnabled.setLabelVisible(false);

    addAndMakeVisible(_title);
    addAndMakeVisible(_summary);
    addAndMakeVisible(_bypassLabel);
    addAndMakeVisible(_gateEnabled);
    addAndMakeVisible(_inputTrimKnob);
    addAndMakeVisible(_noiseGateKnob);
    _gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "isGateEnabled", _gateEnabled);
    _inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "input", _inputTrimKnob.getSlider());
    _noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noise", _noiseGateKnob.getSlider());

    _gateEnabled.onClick = [this]() {
        refreshBypassState();
    };
    refreshBypassState();
}

void InputGatePanel::refreshBypassState() {
    const auto enabled = _gateEnabled.getToggleState();
    _inputTrimKnob.setEnabled(true);
    _noiseGateKnob.setEnabled(enabled);
    _bypassLabel.setVisible(!enabled);
    resized();
}

void InputGatePanel::paint(juce::Graphics&) {}

void InputGatePanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    _gateEnabled.setBounds(header.removeFromRight(48).withSizeKeepingCentre(48, 20));
    header.removeFromRight(8);
    _bypassLabel.setBounds(header.removeFromRight(80));
    header.removeFromRight(8);
    _title.setBounds(header.removeFromLeft(140));
    _summary.setBounds(header);

    constexpr int knobWidth = 108;
    constexpr int gap = 24;
    auto row = juce::Rectangle<int>(knobWidth * 2 + gap, juce::jmin(88, area.getHeight())).withCentre(area.getCentre());
    _inputTrimKnob.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(gap);
    _noiseGateKnob.setBounds(row);
}

AmpProfilerPanel::AmpProfilerPanel(ProfilerAudioProcessor& processor)
    : _ampDrop(makeAmpDropOptions(processor)) {
    configurePanelTitle(_title, "Amp");
    configureBypassLabel(_bypassLabel);
    _ampEnabled.setLabelVisible(false);
    addAndMakeVisible(_title);
    addAndMakeVisible(_bypassLabel);
    addAndMakeVisible(_ampEnabled);
    addAndMakeVisible(_ampDrop);
    addAndMakeVisible(_outputKnob);

    _ampEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor._apvts, "isAmpEnabled", _ampEnabled);
    _outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor._apvts, "gain", _outputKnob.getSlider());

    _ampEnabled.onClick = [this]() {
        refreshAssets();
        if (onAssetsChanged) {
            onAssetsChanged();
        }
    };
    _ampDrop.onStatusMessage = [this](const juce::String& message, bool success) {
        if (onStatusMessage) {
            onStatusMessage(message, success);
        }
    };
    _ampDrop.onChanged = [this]() {
        refreshAssets();
        if (onAssetsChanged) {
            onAssetsChanged();
        }
    };

    refreshAssets();
}

void AmpProfilerPanel::refreshAssets() {
    _ampDrop.refresh();
    const auto loaded = _ampDrop.isFileLoaded();
    const auto enabled = _ampEnabled.getToggleState();
    _ampEnabled.setVisible(loaded);
    _outputKnob.setEnabled(loaded && enabled);
    _bypassLabel.setText(loaded ? "BYPASSED" : "NO MODEL LOADED", juce::dontSendNotification);
    _bypassLabel.setVisible(!loaded || !enabled);
    resized();
}

void AmpProfilerPanel::paint(juce::Graphics&) {}

void AmpProfilerPanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    _ampEnabled.setBounds(header.removeFromRight(48).withSizeKeepingCentre(48, 20));
    header.removeFromRight(8);
    _bypassLabel.setBounds(header.removeFromRight(130));
    header.removeFromRight(8);
    _title.setBounds(header);
    area.removeFromTop(4);

    auto side = area.removeFromRight(112);
    area.removeFromRight(10);
    _ampDrop.setBounds(area);
    _outputKnob.setBounds(side.withSizeKeepingCentre(side.getWidth(), juce::jmin(92, side.getHeight())));
}

CabinetIrPanel::CabinetIrPanel(ProfilerAudioProcessor& processor)
    : _irDrop(makeIrDropOptions(processor)) {
    configurePanelTitle(_title, "Cab");
    configureBypassLabel(_bypassLabel);
    _cabEnabled.setLabelVisible(false);
    addAndMakeVisible(_title);
    addAndMakeVisible(_bypassLabel);
    addAndMakeVisible(_cabEnabled);
    addAndMakeVisible(_irDrop);
    addAndMakeVisible(_lowCutKnob);

    _cabEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor._apvts, "isCabEnabled", _cabEnabled);
    _lowCutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor._apvts, "cabLowCut", _lowCutKnob.getSlider());

    _cabEnabled.onClick = [this]() {
        refreshAssets();
        if (onAssetsChanged) {
            onAssetsChanged();
        }
    };
    _irDrop.onStatusMessage = [this](const juce::String& message, bool success) {
        if (onStatusMessage) {
            onStatusMessage(message, success);
        }
    };
    _irDrop.onChanged = [this]() {
        refreshAssets();
        if (onAssetsChanged) {
            onAssetsChanged();
        }
    };

    refreshAssets();
}

void CabinetIrPanel::refreshAssets() {
    _irDrop.refresh();
    const auto loaded = _irDrop.isFileLoaded();
    const auto enabled = _cabEnabled.getToggleState();
    _cabEnabled.setVisible(loaded);
    _lowCutKnob.setEnabled(loaded && enabled);
    _bypassLabel.setText(loaded ? "BYPASSED" : "NO IR LOADED", juce::dontSendNotification);
    _bypassLabel.setVisible(!loaded || !enabled);
    resized();
}

void CabinetIrPanel::paint(juce::Graphics&) {}

void CabinetIrPanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    _cabEnabled.setBounds(header.removeFromRight(48).withSizeKeepingCentre(48, 20));
    header.removeFromRight(8);
    _bypassLabel.setBounds(header.removeFromRight(110));
    header.removeFromRight(8);
    _title.setBounds(header);
    area.removeFromTop(4);

    auto side = area.removeFromRight(112);
    area.removeFromRight(10);
    _irDrop.setBounds(area);
    _lowCutKnob.setBounds(side.withSizeKeepingCentre(side.getWidth(), juce::jmin(92, side.getHeight())));
}

EqPostFxPanel::EqPostFxPanel(juce::AudioProcessorValueTreeState& apvts)
    : _eqDisplay(apvts),
      _eqModule(apvts) {
    configurePanelTitle(_title, "Eq");
    configureBypassLabel(_bypassLabel);
    _eqEnabled.setLabelVisible(false);
    addAndMakeVisible(_title);
    addAndMakeVisible(_bypassLabel);
    addAndMakeVisible(_eqEnabled);
    addAndMakeVisible(_eqDisplay);
    addAndMakeVisible(_eqModule);
    _eqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "isEqEnabled", _eqEnabled);

    _eqEnabled.onClick = [this]() {
        refreshBypassState();
    };
    refreshBypassState();
}

void EqPostFxPanel::refreshBypassState() {
    const auto enabled = _eqEnabled.getToggleState();
    _eqModule.setEnabled(enabled);
    _eqDisplay.setBypassed(!enabled);
    _bypassLabel.setVisible(!enabled);
    resized();
    repaint();
}

void EqPostFxPanel::paint(juce::Graphics&) {}

void EqPostFxPanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    _eqEnabled.setBounds(header.removeFromRight(48).withSizeKeepingCentre(48, 20));
    header.removeFromRight(8);
    _bypassLabel.setBounds(header.removeFromRight(80));
    header.removeFromRight(8);
    _title.setBounds(header);
    area.removeFromTop(6);

    const auto graphHeight = juce::jlimit(72, 108, area.getHeight() - 72);
    _eqDisplay.setBounds(area.removeFromTop(graphHeight));
    area.removeFromTop(8);
    _eqModule.setBounds(area);
}

PedalDrivePanel::PedalDrivePanel(juce::AudioProcessorValueTreeState& apvts) {
    configurePanelTitle(_title, "Overdrive");
    configureBypassLabel(_bypassLabel);
    _enabled.setLabelVisible(false);

    _model.addItem("Screamer", 1);
    _model.addItem("Distortion", 2);
    _model.addItem("Boost", 3);
    addAndMakeVisible(_title);
    addAndMakeVisible(_bypassLabel);
    addAndMakeVisible(_model);
    addAndMakeVisible(_enabled);
    addAndMakeVisible(_driveKnob);
    addAndMakeVisible(_toneKnob);
    addAndMakeVisible(_levelKnob);

    _enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "isPedalEnabled", _enabled);
    _modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "pedalModel", _model);
    _driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pedalDrive", _driveKnob.getSlider());
    _toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pedalTone", _toneKnob.getSlider());
    _levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pedalLevel", _levelKnob.getSlider());

    _enabled.onClick = [this]() {
        refreshBypassState();
    };
    refreshBypassState();
}

void PedalDrivePanel::refreshBypassState() {
    const auto enabled = _enabled.getToggleState();
    _model.setEnabled(enabled);
    _driveKnob.setEnabled(enabled);
    _toneKnob.setEnabled(enabled);
    _levelKnob.setEnabled(enabled);
    _bypassLabel.setVisible(!enabled);
    resized();
}

void PedalDrivePanel::paint(juce::Graphics&) {}

void PedalDrivePanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    _enabled.setBounds(header.removeFromRight(48).withSizeKeepingCentre(48, 20));
    header.removeFromRight(8);
    _bypassLabel.setBounds(header.removeFromRight(80));
    header.removeFromRight(8);
    _title.setBounds(header);
    area.removeFromTop(4);
    _model.setBounds(area.removeFromTop(24).removeFromLeft(180));
    area.removeFromTop(6);

    constexpr int knobWidth = 108;
    constexpr int gap = 24;
    auto row = juce::Rectangle<int>(knobWidth * 3 + gap * 2, juce::jmin(88, area.getHeight())).withCentre(area.getCentre());
    _driveKnob.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(gap);
    _toneKnob.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(gap);
    _levelKnob.setBounds(row);
}

MasterVolumePanel::MasterVolumePanel(ProfilerAudioProcessor& processor)
    : _processor(processor) {
    configurePanelTitle(_title, "Master");
    _summary.setFont(ProfilerStyle::Fonts::regular(12.0f));
    _summary.setColour(juce::Label::textColourId, ProfilerStyle::Colors::caption);
    _summary.setJustificationType(juce::Justification::centredLeft);
    _summary.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(_title);
    addAndMakeVisible(_summary);
    addAndMakeVisible(_masterKnob);
    addAndMakeVisible(_outputTrimKnob);
    addAndMakeVisible(_outputMeter);

    _masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor._apvts, "master", _masterKnob.getSlider());
    _outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor._apvts, "output", _outputTrimKnob.getSlider());
    startTimerHz(30);
}

MasterVolumePanel::~MasterVolumePanel() {
    stopTimer();
}

void MasterVolumePanel::paint(juce::Graphics&) {}

void MasterVolumePanel::resized() {
    auto area = getLocalBounds().reduced(10, 6);
    auto header = area.removeFromTop(22);
    _title.setBounds(header.removeFromLeft(150));
    _summary.setBounds(header);

    auto meter = area.removeFromRight(14).reduced(4, 8);
    area.removeFromRight(16);
    _outputMeter.setBounds(meter);

    constexpr int knobWidth = 108;
    constexpr int gap = 24;
    auto row = juce::Rectangle<int>(knobWidth * 2 + gap, juce::jmin(88, area.getHeight())).withCentre(area.getCentre());
    _masterKnob.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(gap);
    _outputTrimKnob.setBounds(row);
}

void MasterVolumePanel::timerCallback() {
    _outputMeter.setLevel(_processor.getRmsLevelOutput());
}
