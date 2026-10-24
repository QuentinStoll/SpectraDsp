#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <common/TracyColor.hpp>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#define RTNEURAL_DEFAULT_STATIC 1
#define RTNEURAL_ENABLE_LSTM 1
#define RTNEURAL_ENABLE_GRU 1
#define RTNEURAL_ENABLE_DENSE 1

#include <RTNeural/RTNeural.h>

#include <tracy/Tracy.hpp>

#include "AmpCapture.h"
#include "Fx/FxCatalog.h"
#include "Logging.h"
#include "SettingsPath.h"
#if !(defined(PROFILER_HEADLESS_TESTS) && PROFILER_HEADLESS_TESTS)
#include "PluginEditor.h"
#endif
#include "UiSettings.h"

namespace {
const juce::Identifier kChainLayoutId{"chainLayout"};
const juce::Identifier kChainLayoutHighId{"chainLayoutHigh"};
const juce::Identifier kChainBypassId{"chainBypass"};

void writeChainLayout(juce::ValueTree state, std::uint64_t packed) {
    if (!state.isValid()) {
        return;
    }

    state.setProperty(kChainLayoutHighId, static_cast<int>((packed >> 32) & 0xFFFFFFFFu), nullptr);
    state.setProperty(kChainLayoutId, static_cast<int>(packed & 0xFFFFFFFFu), nullptr);
}

void writeChainBypass(juce::ValueTree state, std::uint16_t mask) {
    if (!state.isValid()) {
        return;
    }
    state.setProperty(kChainBypassId, static_cast<int>(mask), nullptr);
}

std::uint16_t readChainBypass(const juce::ValueTree& tree) {
    return static_cast<std::uint16_t>(static_cast<int>(tree.getProperty(kChainBypassId, 0)) & 0x0FFF);
}

std::uint64_t readChainLayout(const juce::ValueTree& tree) {
    const auto lowDefault = static_cast<int>(SignalChain::defaultPacked & 0xFFFFFFFFu);
    const auto low = static_cast<std::uint32_t>(static_cast<int>(tree.getProperty(kChainLayoutId, lowDefault)));
    const auto high = static_cast<std::uint32_t>(static_cast<int>(tree.getProperty(kChainLayoutHighId, 0)));
    return (static_cast<std::uint64_t>(high) << 32) | static_cast<std::uint64_t>(low);
}

}  // namespace

float ProfilerAudioProcessor::getParameterValue(const std::atomic<float>* parameter, float fallback) noexcept {
    return parameter != nullptr ? parameter->load() : fallback;
}

bool ProfilerAudioProcessor::isCompatibleAmpModel(const RTNeural::Model<float>& model) {
    return !model.layers.empty() &&
           model.getInSize() == 1 &&
           model.getOutSize() >= 1;
}

float ProfilerAudioProcessor::getMasterGainLinear(float masterPercent) noexcept {
    const auto percent = juce::jlimit(0.0f, 100.0f, masterPercent);

    if (percent <= 0.0f) {
        return 0.0f;
    }

    if (percent <= 50.0f) {
        return percent / 50.0f;
    }

    return juce::Decibels::decibelsToGain(juce::jmap(percent, 50.0f, 100.0f, 0.0f, 12.0f));
}

//==============================================================================
ProfilerAudioProcessor::ProfilerAudioProcessor(juce::File profileDirectory,
                                               juce::File playViewSettingsFile)
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::mono(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      _fx(_apvts),
      _profileManager(_apvts, std::move(profileDirectory), std::move(playViewSettingsFile))
#else
    : _fx(_apvts),
      _profileManager(_apvts, std::move(profileDirectory), std::move(playViewSettingsFile))
#endif
{
    Log::logSystemInfoOnFileStart = (bool)(UiSettings::loadHardwareInfoSetting() - 1);
    Log::LogConfig config = Log::LogConfig::fromFile((juce::File)("/home/krt/dev/EIP/VstProfiler/.config/log_settings.json"));
    if (Log::LogRegistry::find(config.name) == nullptr) {
        Log::LogRegistry::create(config.name, config);
    }

    _masterParam = _apvts.getRawParameterValue("master");
    _gainParam = _apvts.getRawParameterValue("gain");
    _noiseParam = _apvts.getRawParameterValue("noise");
    _inputParam = _apvts.getRawParameterValue("input");
    _outputParam = _apvts.getRawParameterValue("output");

    for (int band = 0; band < EqBands::count; ++band) {
        _eqGainParams[static_cast<size_t>(band)] = _apvts.getRawParameterValue(EqBands::specs[band].gainId);
        _eqFreqParams[static_cast<size_t>(band)] = _apvts.getRawParameterValue(EqBands::specs[band].freqId);
    }

    _isMuteParam = _apvts.getRawParameterValue("isMute");
    _isEqEnabledParam = _apvts.getRawParameterValue("isEqEnabled");
    _isGateEnabledParam = _apvts.getRawParameterValue("isGateEnabled");
    _isAmpEnabledParam = _apvts.getRawParameterValue("isAmpEnabled");
    _isCabEnabledParam = _apvts.getRawParameterValue("isCabEnabled");
    _cabLowCutParam = _apvts.getRawParameterValue("cabLowCut");
    _isPedalEnabledParam = _apvts.getRawParameterValue("isPedalEnabled");
    _pedalDriveParam = _apvts.getRawParameterValue("pedalDrive");
    _pedalToneParam = _apvts.getRawParameterValue("pedalTone");
    _pedalLevelParam = _apvts.getRawParameterValue("pedalLevel");
    writeChainLayout(_apvts.state, _chainLayoutPacked.load());
    writeChainBypass(_apvts.state, 0);
    for (auto& mix : _ampWetMix) {
        mix.setCurrentAndTargetValue(1.0f);
    }
    _spectra.open();
}

ProfilerAudioProcessor::~ProfilerAudioProcessor() = default;

//==============================================================================
const juce::String ProfilerAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool ProfilerAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ProfilerAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ProfilerAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ProfilerAudioProcessor::getTailLengthSeconds() const {
    const auto layout = getChainLayout();
    double tail = 0.0;
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        tail = juce::jmax(tail, _fx.tailSecondsFor(layout.atSlot(SignalChain::chainSlotForMovableIndex(index))));
    }
    return tail;
}
int ProfilerAudioProcessor::getNumPrograms() { return 1; }
int ProfilerAudioProcessor::getCurrentProgram() { return 0; }
void ProfilerAudioProcessor::setCurrentProgram(int /*index*/) {}
const juce::String ProfilerAudioProcessor::getProgramName(int /*index*/) { return {}; }
void ProfilerAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

//==============================================================================
void ProfilerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // A device restart breaks the timeline between send and return.
    stopAmpCapture();
    if (_ampCapture.state.load(std::memory_order_acquire) == static_cast<int>(AmpCaptureState::Stopping)) {
        _ampCapture.state.store(static_cast<int>(AmpCaptureState::Stopped), std::memory_order_release);
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 1;

    for (auto& convolver : _cabConvolvers) {
        convolver.prepare(spec);
    }
    if (_irBytes.getSize() > 0) {
        loadIrIntoConvolvers(_irBytes.getData(), _irBytes.getSize());
    }

    _inputTrim.prepare(spec);
    _inputTrim.setRampDurationSeconds(PARAMETER_RAMP_SECONDS);
    _inputTrim.setGainDecibels(getParameterValue(_inputParam, 0.0f));

    _chain.reset();
    _chain.prepare(spec);

    _chain.get<Gain>().setGainDecibels(getParameterValue(_gainParam, 0.0f));
    _chain.get<Gain>().setRampDurationSeconds(PARAMETER_RAMP_SECONDS);

    _masterVolume.prepare(spec);
    _masterVolume.setRampDurationSeconds(PARAMETER_RAMP_SECONDS);
    _masterVolume.setGainLinear(getMasterGainLinear(getParameterValue(_masterParam, 50.0f)));

    _outputTrim.prepare(spec);
    _outputTrim.setRampDurationSeconds(PARAMETER_RAMP_SECONDS);
    _outputTrim.setGainDecibels(getParameterValue(_outputParam, 0.0f));

    _chain.get<NoiseGate>().setThreshold(getParameterValue(_noiseParam, 10.0f) - 60.0f);
    _chain.get<NoiseGate>().setAttack(5.0f);
    _chain.get<NoiseGate>().setRelease(100.0f);
    _chain.get<NoiseGate>().setRatio(10.0f);
    _chain.setBypassed<NoiseGate>(getParameterValue(_isGateEnabledParam, 1.0f) <= 0.5f || getParameterValue(_noiseParam, 0.0f) <= 0.0f);

    const bool eqEnabled = getParameterValue(_isEqEnabledParam, 1.0f) > 0.5f;

    for (auto& eqChain : _eqChains) {
        eqChain.reset();
        eqChain.prepare(spec);
    }
    setEqBypassed(!eqEnabled);

    for (int band = 0; band < EqBands::count; ++band) {
        const auto index = static_cast<size_t>(band);
        _eqGainSmoothed[index].reset(sampleRate, PARAMETER_RAMP_SECONDS);
        _eqFreqSmoothed[index].reset(sampleRate, PARAMETER_RAMP_SECONDS);
        _eqGainSmoothed[index].setCurrentAndTargetValue(
            getParameterValue(_eqGainParams[index], EqBands::specs[band].defaultDb));
        _eqFreqSmoothed[index].setCurrentAndTargetValue(
            getParameterValue(_eqFreqParams[index], EqBands::specs[band].defaultHz));
    }
    updateEqCoefficients();
    _eqCoeffsDirty = false;
    for (auto& eqChain : _eqChains) {
        eqChain.reset();
    }

    _cabLowCutSmoothed.reset(sampleRate, PARAMETER_RAMP_SECONDS);
    _cabLowCutSmoothed.setCurrentAndTargetValue(getParameterValue(_cabLowCutParam, 80.0f));
    for (auto& lowCut : _cabLowCuts) {
        lowCut.reset();
        lowCut.prepare(spec);
    }
    updateCabLowCutCoefficients();
    for (auto& lowCut : _cabLowCuts) {
        lowCut.reset();
    }

    for (auto& tone : _pedalToneFilters) {
        tone.reset();
        tone.prepare(spec);
    }
    updatePedalToneCoefficients();
    for (auto& tone : _pedalToneFilters) {
        tone.reset();
    }

    for (auto& blocker : _dcBlockers) {
        *blocker.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, 35.0f);
        blocker.prepare(spec);
        blocker.reset();
    }

    const auto ampMix = getParameterValue(_isAmpEnabledParam, 1.0f) > 0.5f ? 1.0f : 0.0f;
    for (auto& mix : _ampWetMix) {
        mix.reset(sampleRate, AMP_BYPASS_CROSSFADE_SECONDS);
        mix.setCurrentAndTargetValue(ampMix);
    }
    _directInputLeft.assign(static_cast<std::size_t>(juce::jmax(1, samplesPerBlock)), 0.0f);
    _directInputRight.assign(static_cast<std::size_t>(juce::jmax(1, samplesPerBlock)), 0.0f);
    _fx.prepare(sampleRate, samplesPerBlock);
    _spectraWet.assign(static_cast<std::size_t>(juce::jmax(1, samplesPerBlock)), 0.0f);
    _spectraAmp.prepare(sampleRate, samplesPerBlock);
    updateReportedLatency();

    {
        const juce::ScopedLock ampLock(_ampModelLock);
        if (_neuralAmp != nullptr) {
            _neuralAmp->reset();
            _ampLoaded = true;
        }
        for (auto& copy : _ampCopies) {
            if (copy != nullptr) {
                copy->reset();
            }
        }
    }
}

void ProfilerAudioProcessor::releaseResources() {
    _chain.reset();
    for (auto& eqChain : _eqChains) {
        eqChain.reset();
    }
    for (auto& lowCut : _cabLowCuts) {
        lowCut.reset();
    }
    for (auto& tone : _pedalToneFilters) {
        tone.reset();
    }
    _inputTrim.reset();
    _masterVolume.reset();
    _outputTrim.reset();
    _fx.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ProfilerAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }

    // The processor accepts one mono input and exposes a stereo output so the
    // processed mono amp signal can be heard identically on both sides.
#if !JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#endif

    return true;
#endif
}
#endif

/**
    Performs the real-time audio processing.
    This implementation handles gain scaling, a main Dsp chain,
    an oversampled non-linear amp stage, and an IR convolution stage.
*/
void ProfilerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& /*midiMessages*/) {
    ZoneScopedNC("test2", tracy::Color::Purple);

    juce::ScopedNoDenormals noDenormals;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0) {
        return;
    }

    if (renderIrCapture(buffer) || renderAmpCapture(buffer)) {
        return;
    }

    _rmsLevelInput.store(juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, numSamples), -60.0f),
                         std::memory_order_relaxed);

    // Handle Mute
    if (getParameterValue(_isMuteParam, 0.0f) > 0.5f) {
        buffer.clear();
        _rmsLevelOutput.store(-60.0f, std::memory_order_relaxed);
        return;
    }

    // Handle any incoming MIDI messages (e.g., for parameter automation)
    for (auto i = getTotalNumInputChannels(); i < juce::jmin(getTotalNumOutputChannels(), numChannels); ++i)
        buffer.clear(i, 0, numSamples);

    _inputTrim.setGainDecibels(getParameterValue(_inputParam, 0.0f));
    _chain.get<Gain>().setGainDecibels(getParameterValue(_gainParam, 0.0f));
    const auto noiseGateValue = getParameterValue(_noiseParam, 0.0f);
    const bool gateEnabled = getParameterValue(_isGateEnabledParam, 1.0f) > 0.5f;
    _chain.get<NoiseGate>().setThreshold(noiseGateValue - 60.0f);
    // A zero control value is a true bypass, rather than a -60 dB gate.
    _chain.setBypassed<NoiseGate>(!gateEnabled || noiseGateValue <= 0.0f);

    bool eqSmoothing = false;
    for (int band = 0; band < EqBands::count; ++band) {
        const auto index = static_cast<size_t>(band);
        _eqGainSmoothed[index].setTargetValue(getParameterValue(_eqGainParams[index], EqBands::specs[band].defaultDb));
        _eqFreqSmoothed[index].setTargetValue(
            getParameterValue(_eqFreqParams[index], EqBands::specs[band].defaultHz));
        eqSmoothing = eqSmoothing || _eqGainSmoothed[index].isSmoothing() || _eqFreqSmoothed[index].isSmoothing();
    }

    const bool eqEnabled = getParameterValue(_isEqEnabledParam, 1.0f) > 0.5f;
    setEqBypassed(!eqEnabled);

    if (eqEnabled && (_eqCoeffsDirty || eqSmoothing)) {
        updateEqCoefficients();
        _eqCoeffsDirty = eqSmoothing;
    }

    if (static_cast<int>(_directInputLeft.size()) < numSamples) {
        _directInputLeft.resize(static_cast<std::size_t>(numSamples));
        _directInputRight.resize(static_cast<std::size_t>(numSamples));
    }
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + numSamples, _directInputLeft.begin());
    if (numChannels > 1) {
        std::copy(buffer.getReadPointer(1), buffer.getReadPointer(1) + numSamples, _directInputRight.begin());
    } else {
        std::copy(_directInputLeft.begin(), _directInputLeft.begin() + numSamples, _directInputRight.begin());
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto monoBlock = block.getSingleChannelBlock(0);
    juce::dsp::ProcessContextReplacing<float> context(monoBlock);
    _inputTrim.process(context);
    processGateStage(context);

    float* left = buffer.getWritePointer(0);
    float* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    auto mirror = [&]() {
        if (right != nullptr) {
            juce::FloatVectorOperations::copy(right, left, numSamples);
        }
    };
    auto foldToMono = [&]() {
        if (right == nullptr) {
            return;
        }
        for (int sample = 0; sample < numSamples; ++sample) {
            left[sample] = 0.5f * (left[sample] + right[sample]);
        }
        mirror();
    };
    mirror();

    double bpm = 120.0;
    if (auto* hostPlayHead = getPlayHead()) {
        if (auto position = hostPlayHead->getPosition()) {
            if (auto value = position->getBpm()) {
                if (*value > 1.0) {
                    bpm = *value;
                }
            }
        }
    }
    _fx.setTempo(bpm);

    _cabLowCutSmoothed.setTargetValue(getParameterValue(_cabLowCutParam, 80.0f));
    const auto layout = SignalChain::Layout::fromPacked(_chainLayoutPacked.load(std::memory_order_relaxed));
    int ampInstance = 0;
    int cabInstance = 0;
    int eqInstance = 0;
    int pedalInstance = 0;
    std::array<int, 32> fxInstance{};
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto chainSlot = SignalChain::chainSlotForMovableIndex(index);
        const auto stage = layout.atSlot(chainSlot);
        const bool slotEnabled = !isChainSlotBypassed(chainSlot);
        switch (stage) {
            case SignalChain::Stage::Cab:
                foldToMono();
                processCabStage(context, cabInstance++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Eq:
                foldToMono();
                processEqStage(context, eqInstance++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Pedal:
                foldToMono();
                processPedalStage(buffer, context, numSamples, pedalInstance++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Amp:
                foldToMono();
                processAmpStage(buffer, context, numSamples, ampInstance++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Empty:
                break;
            default: {
                const auto stageIndex = static_cast<size_t>(static_cast<int>(stage));
                const int instance = stageIndex < fxInstance.size() ? fxInstance[stageIndex]++ : 0;
                _fx.process(stage, instance, left, right != nullptr ? right : left, numSamples, slotEnabled,
                            _directInputLeft.data(), _directInputRight.data());
                break;
            }
        }
    }

    _cabLowCutSmoothed.skip(numSamples);
    for (auto& smoothed : _eqGainSmoothed) {
        smoothed.skip(numSamples);
    }
    for (auto& smoothed : _eqFreqSmoothed) {
        smoothed.skip(numSamples);
    }

    _masterVolume.setGainLinear(getMasterGainLinear(getParameterValue(_masterParam, 50.0f)));
    _outputTrim.setGainDecibels(getParameterValue(_outputParam, 0.0f));
    {
        juce::dsp::AudioBlock<float> outputBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> outputContext(outputBlock);
        _masterVolume.process(outputContext);
        _outputTrim.process(outputContext);
    }
    updateReportedLatency();

    _rmsLevelOutput.store(juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, numSamples), -60.0f),
                          std::memory_order_relaxed);
}

float ProfilerAudioProcessor::getRmsLevelInput() const noexcept {
    return _rmsLevelInput.load(std::memory_order_relaxed);
}

float ProfilerAudioProcessor::getRmsLevelOutput() const noexcept {
    return _rmsLevelOutput.load(std::memory_order_relaxed);
}

bool ProfilerAudioProcessor::readTuner(Fx::TunerSnapshot& snapshot) const {
    return _fx.readTuner(snapshot);
}

void ProfilerAudioProcessor::updateReportedLatency() {
    const auto layout = getChainLayout();
    int latency = 0;
    int activeAmps = 0;
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto chainSlot = SignalChain::chainSlotForMovableIndex(index);
        if (isChainSlotBypassed(chainSlot)) {
            continue;
        }
        const auto stage = layout.atSlot(chainSlot);
        latency += _fx.latencySamplesFor(stage);
        if (stage == SignalChain::Stage::Amp) {
            ++activeAmps;
        }
    }
    if (_spectraAmp.isLoaded()) {
        latency += activeAmps * _spectraAmp.latencySamples();
    }
    if (latency != _reportedLatency) {
        _reportedLatency = latency;
        setLatencySamples(latency);
    }
}

SignalChain::Layout ProfilerAudioProcessor::getChainLayout() const noexcept {
    return SignalChain::Layout::fromPacked(_chainLayoutPacked.load(std::memory_order_relaxed));
}

void ProfilerAudioProcessor::setChainLayout(const SignalChain::Layout& layout) {
    const auto valid = layout.isValid() ? layout : SignalChain::Layout{};
    const auto stored = SignalChain::Layout::fromPacked(valid.packed());
    const auto previous = getChainLayout();
    auto mask = _slotBypassMask.load(std::memory_order_relaxed);
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        if (previous.atSlot(slot) != stored.atSlot(slot)) {
            mask = static_cast<std::uint16_t>(mask & ~(1u << index));
        }
    }
    _slotBypassMask.store(mask, std::memory_order_relaxed);
    const auto packed = stored.packed();
    _chainLayoutPacked.store(packed, std::memory_order_relaxed);
    writeChainLayout(_apvts.state, packed);
    writeChainBypass(_apvts.state, mask);
    syncAmpCopies();
}

void ProfilerAudioProcessor::swapChainSlots(int fromSlot, int toSlot) {
    const int fromBit = SignalChain::movableIndexForSlot(fromSlot);
    const int toBit = SignalChain::movableIndexForSlot(toSlot);
    if (fromBit < 0 || toBit < 0 || fromBit == toBit) {
        return;
    }

    auto mask = _slotBypassMask.load(std::memory_order_relaxed);
    const bool fromBypassed = (mask & (1u << fromBit)) != 0;
    const bool toBypassed = (mask & (1u << toBit)) != 0;
    if (fromBypassed) {
        mask = static_cast<std::uint16_t>(mask | (1u << toBit));
    } else {
        mask = static_cast<std::uint16_t>(mask & ~(1u << toBit));
    }
    if (toBypassed) {
        mask = static_cast<std::uint16_t>(mask | (1u << fromBit));
    } else {
        mask = static_cast<std::uint16_t>(mask & ~(1u << fromBit));
    }
    _slotBypassMask.store(mask, std::memory_order_relaxed);

    auto layout = getChainLayout();
    layout.moveSlot(fromSlot, toSlot);
    const auto packed = layout.packed();
    _chainLayoutPacked.store(packed, std::memory_order_relaxed);
    writeChainLayout(_apvts.state, packed);
    writeChainBypass(_apvts.state, mask);
    syncAmpCopies();
}

bool ProfilerAudioProcessor::isChainSlotBypassed(int chainSlot) const noexcept {
    const int bit = SignalChain::movableIndexForSlot(chainSlot);
    if (bit < 0) {
        return false;
    }
    return (_slotBypassMask.load(std::memory_order_relaxed) & (1u << bit)) != 0;
}

void ProfilerAudioProcessor::setChainSlotBypassed(int chainSlot, bool bypassed) {
    const int bit = SignalChain::movableIndexForSlot(chainSlot);
    if (bit < 0) {
        return;
    }
    auto mask = _slotBypassMask.load(std::memory_order_relaxed);
    if (bypassed) {
        mask = static_cast<std::uint16_t>(mask | (1u << bit));
    } else {
        mask = static_cast<std::uint16_t>(mask & ~(1u << bit));
    }
    _slotBypassMask.store(mask, std::memory_order_relaxed);
    writeChainBypass(_apvts.state, mask);
}

void ProfilerAudioProcessor::toggleChainSlotBypass(int chainSlot) {
    setChainSlotBypassed(chainSlot, !isChainSlotBypassed(chainSlot));
}

void ProfilerAudioProcessor::clearChainSlot(int slot) {
    auto layout = getChainLayout();
    layout.clear(slot);
    setChainLayout(layout);
}

void ProfilerAudioProcessor::setEqBypassed(bool bypassed) {
    for (auto& eqChain : _eqChains) {
        eqChain.setBypassed<LowShelf>(bypassed);
        eqChain.setBypassed<Peak1>(bypassed);
        eqChain.setBypassed<Peak2>(bypassed);
        eqChain.setBypassed<Peak3>(bypassed);
        eqChain.setBypassed<Peak4>(bypassed);
        eqChain.setBypassed<HighShelf>(bypassed);
    }
}

bool ProfilerAudioProcessor::loadIrIntoConvolvers(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return false;
    }

    // Convolution reads a raw pointer later, on a background thread. Decode here, while the
    // caller's buffer is still intact, and hand over a buffer the convolver owns.
    auto stream = std::make_unique<juce::MemoryInputStream>(data, size, false);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (reader == nullptr || reader->numChannels < 1 || reader->lengthInSamples <= 0 || !(reader->sampleRate > 0.0)) {
        return false;
    }

    constexpr juce::int64 kMaxIrSamples = 48000 * 30;
    if (reader->lengthInSamples > kMaxIrSamples) {
        return false;
    }

    const int length = static_cast<int>(reader->lengthInSamples);
    const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> decoded(channels, length);
    if (!reader->read(decoded.getArrayOfWritePointers(), channels, 0, length)) {
        return false;
    }

    const double rate = reader->sampleRate;
    for (auto& convolver : _cabConvolvers) {
        juce::AudioBuffer<float> impulse(decoded.getNumChannels(), decoded.getNumSamples());
        for (int channel = 0; channel < decoded.getNumChannels(); ++channel) {
            impulse.copyFrom(channel, 0, decoded, channel, 0, decoded.getNumSamples());
        }
        convolver.loadImpulseResponse(std::move(impulse),
                                      rate,
                                      juce::dsp::Convolution::Stereo::no,
                                      juce::dsp::Convolution::Trim::yes,
                                      juce::dsp::Convolution::Normalise::yes);
    }
    return true;
}

void ProfilerAudioProcessor::syncAmpCopies(bool force) {
    {
        const juce::ScopedLock ampLock(_ampModelLock);
        const auto extraAmps = juce::jmax(0, getChainLayout().count(SignalChain::Stage::Amp) - 1);
        if (force || static_cast<int>(_ampCopies.size()) != extraAmps) {
            _ampCopies.clear();
            if (extraAmps > 0 && _neuralAmp != nullptr && _ampModelBytes.getSize() > 0) {
                for (int index = 0; index < extraAmps; ++index) {
                    auto copy = parseAmpModel(_ampModelBytes.getData(), _ampModelBytes.getSize(), nullptr);
                    if (copy != nullptr) {
                        _ampCopies.push_back(std::move(copy));
                    }
                }
            }
        }
    }

    _spectraAmp.setInstanceCount(juce::jmax(1, getChainLayout().count(SignalChain::Stage::Amp)));
}

void ProfilerAudioProcessor::resetChainLayout() {
    setChainLayout({});
}

void ProfilerAudioProcessor::placeChainStage(SignalChain::Stage stage, int slot) {
    auto layout = getChainLayout();
    layout.place(stage, slot);
    setChainLayout(layout);
}

void ProfilerAudioProcessor::processGateStage(juce::dsp::ProcessContextReplacing<float>& context) {
    if (!_chain.isBypassed<NoiseGate>()) {
        _chain.get<NoiseGate>().process(context);
    }
}

void ProfilerAudioProcessor::processAmpStage(juce::AudioBuffer<float>& buffer,
                                             juce::dsp::ProcessContextReplacing<float>& context,
                                             int numSamples,
                                             int instance,
                                             bool slotEnabled) {
    _chain.get<Gain>().process(context);

    const auto mixIndex = static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance));
    auto& wetMix = _ampWetMix[mixIndex];
    const bool ampEnabled = slotEnabled && getParameterValue(_isAmpEnabledParam, 1.0f) > 0.5f;
    wetMix.setTargetValue(ampEnabled ? 1.0f : 0.0f);

    if (_spectraAmp.isLoaded()) {
        auto* channel0Data = buffer.getWritePointer(0);
        if (static_cast<int>(_spectraWet.size()) >= numSamples &&
            _spectraAmp.render(instance, channel0Data, _spectraWet.data(), numSamples)) {
            for (int i = 0; i < numSamples; ++i) {
                const float dry = channel0Data[i];
                float wet = _spectraWet[static_cast<std::size_t>(i)];
                if (!std::isfinite(wet)) {
                    wet = dry;
                }
                const float mix = wetMix.getNextValue();
                channel0Data[i] = dry + (wet - dry) * mix;
            }
        } else {
            wetMix.skip(numSamples);
        }
        _dcBlockers[static_cast<std::size_t>(juce::jlimit(0, kChainCopies - 1, instance))].process(context);
        return;
    }

    {
        const juce::ScopedLock ampLock(_ampModelLock);
        auto* model = _neuralAmp.get();
        if (instance > 0) {
            const auto copyIndex = static_cast<size_t>(instance - 1);
            if (copyIndex < _ampCopies.size() && _ampCopies[copyIndex] != nullptr) {
                model = _ampCopies[copyIndex].get();
            }
        }

        if (_ampLoaded && model != nullptr) {
            const auto inputSkip = _ampInputSkip;
            const auto inputGain = _ampInputGain;
            const auto outputGain = _ampOutputGain;
            auto* channel0Data = buffer.getWritePointer(0);

            // Keep the recurrent state tracking the input while the block is bypassed.
            // Freezing it makes the amp take seconds to recover when it is switched back on.
            for (int i = 0; i < numSamples; ++i) {
                const float dry = channel0Data[i];
                const float driven = dry * inputGain;
                const float inputSample[] = {driven};
                model->forward(inputSample);
                float wet = model->getOutputs()[0];

                if (!std::isfinite(wet)) {
                    model->reset();
                    wet = 0.0f;
                }

                // CoreAudioML / AIDA models store a residual in in_skip: the network
                // predicts the difference, and the input channels are added back.
                if (inputSkip > 0) {
                    wet += driven;
                }
                wet *= outputGain;
                if (!std::isfinite(wet)) {
                    wet = dry;
                }

                const float mix = wetMix.getNextValue();
                channel0Data[i] = dry + (wet - dry) * mix;
            }
        } else {
            wetMix.skip(numSamples);
        }
    }

    _dcBlockers[static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance))].process(context);
}

void ProfilerAudioProcessor::processCabStage(juce::dsp::ProcessContextReplacing<float>& context, int instance, bool slotEnabled) {
    const bool cabEnabled = slotEnabled && getParameterValue(_isCabEnabledParam, 1.0f) > 0.5f;
    if (!_irLoaded || !cabEnabled) {
        return;
    }

    const auto index = static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance));
    _cabConvolvers[index].process(context);
    if (_cabLowCutSmoothed.isSmoothing()) {
        updateCabLowCutCoefficients();
    }
    _cabLowCuts[index].process(context);
}

void ProfilerAudioProcessor::processEqStage(juce::dsp::ProcessContextReplacing<float>& context, int instance, bool slotEnabled) {
    if (!slotEnabled) {
        return;
    }
    _eqChains[static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance))].process(context);
}

void ProfilerAudioProcessor::processPedalStage(juce::AudioBuffer<float>& buffer,
                                               juce::dsp::ProcessContextReplacing<float>& context,
                                               int numSamples,
                                               int instance,
                                               bool slotEnabled) {
    if (!slotEnabled || getParameterValue(_isPedalEnabledParam, 1.0f) <= 0.5f) {
        return;
    }

    const auto drive = juce::jmap(getParameterValue(_pedalDriveParam, 4.0f), 0.0f, 10.0f, 1.0f, 16.0f);
    const auto level = juce::Decibels::decibelsToGain(getParameterValue(_pedalLevelParam, 0.0f));
    const auto makeup = 1.0f / std::tanh(drive * 0.35f);
    auto* channel0Data = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i) {
        channel0Data[i] = std::tanh(channel0Data[i] * drive) * makeup * level;
    }

    updatePedalToneCoefficients();
    _pedalToneFilters[static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance))].process(context);
}

void ProfilerAudioProcessor::updatePedalToneCoefficients() {
    const auto sampleRate = getSampleRate();
    if (sampleRate <= 0.0) {
        return;
    }

    const auto tone = juce::jlimit(0.0f, 1.0f, getParameterValue(_pedalToneParam, 65.0f) / 100.0f);
    const auto hz = 400.0f * std::pow(30.0f, tone);
    const auto coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(
        sampleRate, juce::jlimit(400.0f, static_cast<float>(sampleRate * 0.45), hz));
    for (auto& filter : _pedalToneFilters) {
        *filter.state = coefficients;
    }
}

void ProfilerAudioProcessor::updateEqCoefficients() {
    const auto sampleRate = getSampleRate();
    if (sampleRate <= 0.0) {
        return;
    }

    const auto nyquist = static_cast<float>(sampleRate * 0.45);

    auto freqFor = [nyquist](float hz) {
        return juce::jlimit(EqBands::minHz, nyquist, hz);
    };

    auto gainFor = [](float db) {
        return juce::Decibels::decibelsToGain(db);
    };

    const auto lowShelf = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(
        sampleRate, freqFor(_eqFreqSmoothed[0].getCurrentValue()), SHELF_Q, gainFor(_eqGainSmoothed[0].getCurrentValue()));
    const auto peak1 = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate, freqFor(_eqFreqSmoothed[1].getCurrentValue()), PEAK_Q, gainFor(_eqGainSmoothed[1].getCurrentValue()));
    const auto peak2 = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate, freqFor(_eqFreqSmoothed[2].getCurrentValue()), PEAK_Q, gainFor(_eqGainSmoothed[2].getCurrentValue()));
    const auto peak3 = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate, freqFor(_eqFreqSmoothed[3].getCurrentValue()), PEAK_Q, gainFor(_eqGainSmoothed[3].getCurrentValue()));
    const auto peak4 = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate, freqFor(_eqFreqSmoothed[4].getCurrentValue()), PEAK_Q, gainFor(_eqGainSmoothed[4].getCurrentValue()));
    const auto highShelf = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(
        sampleRate, freqFor(_eqFreqSmoothed[5].getCurrentValue()), SHELF_Q, gainFor(_eqGainSmoothed[5].getCurrentValue()));
    for (auto& eqChain : _eqChains) {
        *eqChain.get<LowShelf>().state = lowShelf;
        *eqChain.get<Peak1>().state = peak1;
        *eqChain.get<Peak2>().state = peak2;
        *eqChain.get<Peak3>().state = peak3;
        *eqChain.get<Peak4>().state = peak4;
        *eqChain.get<HighShelf>().state = highShelf;
    }
}

void ProfilerAudioProcessor::updateCabLowCutCoefficients() {
    const auto coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(
        getSampleRate(),
        juce::jlimit(20.0f, 250.0f, _cabLowCutSmoothed.getCurrentValue()),
        SHELF_Q);
    for (auto& lowCut : _cabLowCuts) {
        *lowCut.state = coefficients;
    }
}

//==============================================================================
bool ProfilerAudioProcessor::hasEditor() const {
#if defined(PROFILER_HEADLESS_TESTS) && PROFILER_HEADLESS_TESTS
    return false;
#else
    return true;
#endif
}

juce::AudioProcessorEditor* ProfilerAudioProcessor::createEditor() {
#if defined(PROFILER_HEADLESS_TESTS) && PROFILER_HEADLESS_TESTS
    return nullptr;
#else
    return new ProfilerAudioProcessorEditor(*this);
#endif
}

//==============================================================================
void ProfilerAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    writeChainLayout(_apvts.state, _chainLayoutPacked.load(std::memory_order_relaxed));
    writeChainBypass(_apvts.state, _slotBypassMask.load(std::memory_order_relaxed));
    if (auto xml = _apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void ProfilerAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        if (xml->hasTagName(_apvts.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xml);
            const auto packed = readChainLayout(tree);
            _chainLayoutPacked.store(SignalChain::Layout::fromPacked(packed).packed(), std::memory_order_relaxed);
            _slotBypassMask.store(readChainBypass(tree), std::memory_order_relaxed);
            syncAmpCopies();
            _apvts.replaceState(std::move(tree));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ProfilerAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ==============================================================================
    // Master parameters
    // ==============================================================================
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"master", 1}, "Master Volume", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Gain", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"noise", 1}, "Noise Gate", juce::NormalisableRange<float>(0.0f, 60.0f, 0.1f), 10.0f));

    // Fix P0: Add missing Input and Output parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"input", 1}, "Input Trim", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output", 1}, "Output Trim", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // ==============================================================================
    // EQ parameters
    // ==============================================================================
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"depth", 1}, "Low Shelf", EqBands::gainRange(), EqBands::specs[0].defaultDb));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bass", 1}, "EQ Band 1", EqBands::gainRange(), EqBands::specs[1].defaultDb));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid", 1}, "EQ Band 2", EqBands::gainRange(), EqBands::specs[2].defaultDb));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highMid", 1}, "EQ Band 3", EqBands::gainRange(), EqBands::specs[3].defaultDb));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"treble", 1}, "EQ Band 4", EqBands::gainRange(), EqBands::specs[4].defaultDb));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"presence", 1}, "High Shelf", EqBands::gainRange(), EqBands::specs[5].defaultDb));

    for (const auto& band : EqBands::specs) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{band.freqId, 1},
            juce::String(band.label) + " Freq",
            EqBands::freqRange(),
            band.defaultHz));
    }

    // ==============================================================================
    // Other parameters
    // ==============================================================================
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isMute", 1}, "Mute", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isEqEnabled", 1}, "EQ", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isGateEnabled", 1}, "Gate Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isAmpEnabled", 1}, "Amp Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isCabEnabled", 1}, "Cab Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"cabLowCut", 1}, "Cab Low Cut", juce::NormalisableRange<float>(20.0f, 250.0f, 1.0f), 80.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isPedalEnabled", 1}, "Pedal Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pedalDrive", 1}, "Pedal Drive", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pedalTone", 1}, "Pedal Tone", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 65.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pedalLevel", 1}, "Pedal Level", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    Fx::addParameters(layout);
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ProfilerAudioProcessor();
}

bool ProfilerAudioProcessor::loadIrFromMemory(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return false;
    }

    _irBytes.replaceAll(data, static_cast<size_t>(size));
    if (!loadIrIntoConvolvers(_irBytes.getData(), _irBytes.getSize())) {
        _irBytes.reset();
        _irLoaded = false;
        _currentIRFile = juce::File{};
        return false;
    }

    _irLoaded = true;
    _currentIRFile = juce::File{};
    return true;
}

bool ProfilerAudioProcessor::loadIRFile(const juce::File& file) {
    if (!file.existsAsFile()) {
        return false;
    }

    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes)) {
        return false;
    }

    const auto loaded = loadIrFromMemory(bytes.getData(), bytes.getSize());
    bytes.fillWith(0);
    if (!loaded) {
        return false;
    }

    _currentIRFile = file;
    return true;
}

bool ProfilerAudioProcessor::loadProtectedIr(const void* data, size_t size) {
    juce::MemoryBlock plain;
    if (!_spectra.decryptIr(data, size, plain)) {
        return false;
    }

    const auto loaded = loadIrFromMemory(plain.getData(), plain.getSize());
    plain.fillWith(0);
    return loaded;
}

void ProfilerAudioProcessor::unloadIRFile() {
    _irLoaded = false;
    _currentIRFile = juce::File{};
    _irBytes.reset();
    for (auto& convolver : _cabConvolvers) {
        convolver.reset();
    }
}

std::unique_ptr<RTNeural::Model<float>> ProfilerAudioProcessor::parseAmpModel(const void* data,
                                                                              size_t size,
                                                                              AmpModelMetadata* metadata) const {
    if (metadata != nullptr) {
        *metadata = {};
    }
    if (data == nullptr || size == 0) {
        return nullptr;
    }

    std::vector<char> bytes(size);
    std::memcpy(bytes.data(), data, size);
    std::unique_ptr<RTNeural::Model<float>> loadedAmp;
    try {
        const auto parsed = nlohmann::json::parse(std::string(bytes.data(), bytes.size()));
        if (metadata != nullptr) {
            auto gainFromDecibels = [&parsed](const char* key) {
                if (!parsed.contains(key) || !parsed.at(key).is_number()) {
                    return 1.0f;
                }
                return juce::Decibels::decibelsToGain(static_cast<float>(parsed.at(key).get<double>()));
            };

            metadata->inputSkip = 0;
            if (parsed.contains("in_skip") && parsed.at("in_skip").is_number()) {
                metadata->inputSkip = juce::jmax(0, juce::roundToInt(parsed.at("in_skip").get<double>()));
            }
            metadata->inputGain = gainFromDecibels("in_gain");
            metadata->outputGain = gainFromDecibels("out_gain");
        }
        loadedAmp = RTNeural::json_parser::parseJson<float>(parsed);
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("RTNeural Load Error: " + juce::String(e.what()));
        loadedAmp.reset();
    }
    std::fill(bytes.begin(), bytes.end(), '\0');

    if (loadedAmp == nullptr) {
        return nullptr;
    }

    if (!isCompatibleAmpModel(*loadedAmp)) {
        juce::Logger::writeToLog("RTNeural Load Error: expected a mono-input model with at least one output.");
        return nullptr;
    }

    loadedAmp->reset();
    return loadedAmp;
}

bool ProfilerAudioProcessor::publishAmpModel(std::unique_ptr<RTNeural::Model<float>> model,
                                             const juce::File& sourceFile,
                                             const AmpModelMetadata& metadata) {
    if (model == nullptr) {
        return false;
    }

    const juce::ScopedLock ampLock(_ampModelLock);
    _neuralAmp = std::move(model);
    _ampInputSkip = metadata.inputSkip;
    _ampInputGain = metadata.inputGain;
    _ampOutputGain = metadata.outputGain;
    _ampLoaded = true;
    _currentAmpFile = sourceFile;
    _ampFileLoaded = sourceFile.existsAsFile();
    return true;
}

bool ProfilerAudioProcessor::installAmpPayload(const void* data, size_t size, const juce::File& sourceFile) {
    if (data == nullptr || size == 0) {
        return false;
    }

    _ampModelBytes.replaceAll(data, static_cast<size_t>(size));
    const auto status = _spectraAmp.load(data, size);
    if (status == SpectraAmp::LoadStatus::Loaded) {
        {
            const juce::ScopedLock ampLock(_ampModelLock);
            _neuralAmp = nullptr;
            _ampCopies.clear();
            _ampLoaded = false;
            _ampInputSkip = 0;
            _ampInputGain = 1.0f;
            _ampOutputGain = 1.0f;
            _currentAmpFile = sourceFile;
            _ampFileLoaded = sourceFile.existsAsFile();
        }
        syncAmpCopies(true);
        updateReportedLatency();
        return true;
    }

    if (status == SpectraAmp::LoadStatus::Invalid) {
        _ampModelBytes.reset();
        return false;
    }

    AmpModelMetadata metadata;
    auto loadedAmp = parseAmpModel(data, size, &metadata);
    if (loadedAmp == nullptr) {
        _ampModelBytes.reset();
        return false;
    }

    _spectraAmp.clear();
    const auto loaded = publishAmpModel(std::move(loadedAmp), sourceFile, metadata);
    if (loaded) {
        syncAmpCopies(true);
        updateReportedLatency();
    } else {
        _ampModelBytes.reset();
    }
    return loaded;
}

bool ProfilerAudioProcessor::loadAmpFromMemory(const void* data, size_t size) {
    return installAmpPayload(data, size, {});
}

bool ProfilerAudioProcessor::loadAmpFile(const juce::File& file) {
    if (!file.existsAsFile()) {
        return false;
    }

    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes)) {
        return false;
    }

    const auto loaded = installAmpPayload(bytes.getData(), bytes.getSize(), file);
    bytes.fillWith(0);
    return loaded;
}

bool ProfilerAudioProcessor::loadSpectraModel(const void* cipher, size_t cipherSize, const uint8_t* key, size_t keySize) {
    juce::MemoryBlock plain;
    if (!_spectra.openModel(key, keySize, cipher, cipherSize, plain)) {
        return false;
    }

    const auto loaded = loadAmpFromMemory(plain.getData(), plain.getSize());
    plain.fillWith(0);
    return loaded;
}

bool ProfilerAudioProcessor::loadProtectedAmp(const void* data, size_t size) {
    juce::MemoryBlock plain;
    if (!_spectra.decryptModel(data, size, plain)) {
        return false;
    }

    const auto loaded = installAmpPayload(plain.getData(), plain.getSize(), {});
    plain.fillWith(0);
    return loaded;
}

bool ProfilerAudioProcessor::isSpectraLoaded() const noexcept {
    return _spectra.isLoaded();
}

bool ProfilerAudioProcessor::unlockSpectraSession(const juce::String& accessToken) {
    return _spectra.unlock(accessToken);
}

void ProfilerAudioProcessor::lockSpectraSession() {
    _spectra.lock();
}

bool ProfilerAudioProcessor::beginIrCapture(juce::String* errorMessage) {
    const auto sampleRate = getSampleRate();
    std::size_t recordFrames = 0;
    std::vector<float> sweep;
    if (_irCapture.state.load(std::memory_order_acquire) == static_cast<int>(IrCaptureState::Recording)) {
        if (errorMessage != nullptr) {
            *errorMessage = "A capture is already running.";
        }
        return false;
    }
    if (wrapperType != juce::AudioProcessor::wrapperType_Standalone) {
        if (errorMessage != nullptr) {
            *errorMessage = "IR capture runs in the standalone.";
        }
        return false;
    }
    if (!_spectra.isLoaded()) {
        if (errorMessage != nullptr) {
            *errorMessage = "SpectraDsp is not loaded.";
        }
        return false;
    }
    if (sampleRate < 8000.0) {
        if (errorMessage != nullptr) {
            *errorMessage = "The audio device is not running.";
        }
        return false;
    }
    if (!_spectra.prepareIrSweep(sampleRate, sweep, recordFrames) || sweep.empty() || recordFrames < sweep.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not prepare the sweep.";
        }
        return false;
    }

    _irCapture.state.store(static_cast<int>(IrCaptureState::Idle), std::memory_order_release);
    _irCapture.sweep = std::move(sweep);
    _irCapture.recorded.assign(recordFrames, 0.0f);
    _irCapture.sweepFrames = _irCapture.sweep.size();
    _irCapture.sampleRate = sampleRate;
    _irCapture.index.store(0, std::memory_order_relaxed);
    _irCapture.state.store(static_cast<int>(IrCaptureState::Recording), std::memory_order_release);
    return true;
}

bool ProfilerAudioProcessor::irCaptureFinished() const noexcept {
    return _irCapture.state.load(std::memory_order_acquire) == static_cast<int>(IrCaptureState::Complete);
}

bool ProfilerAudioProcessor::sealIrCapture(juce::MemoryBlock& sealed, std::array<std::uint8_t, 32>& key, juce::String* errorMessage) {
    sealed.reset();
    key.fill(0);
    if (_irCapture.state.load(std::memory_order_acquire) != static_cast<int>(IrCaptureState::Complete)) {
        if (errorMessage != nullptr) {
            *errorMessage = "The sweep has not finished.";
        }
        return false;
    }

    const auto sealedOk = _spectra.sealIr(_irCapture.recorded.data(), _irCapture.recorded.size(), _irCapture.sampleRate, key, sealed);
    std::fill(_irCapture.sweep.begin(), _irCapture.sweep.end(), 0.0f);
    std::fill(_irCapture.recorded.begin(), _irCapture.recorded.end(), 0.0f);
    _irCapture.sweep.clear();
    _irCapture.recorded.clear();
    _irCapture.sweepFrames = 0;
    _irCapture.state.store(static_cast<int>(IrCaptureState::Idle), std::memory_order_release);
    if (!sealedOk) {
        key.fill(0);
        sealed.reset();
        if (errorMessage != nullptr) {
            *errorMessage = "Could not seal the impulse response.";
        }
        return false;
    }
    return true;
}

bool ProfilerAudioProcessor::storeCapturedIr(const juce::String& title,
                                             const std::array<std::uint8_t, 32>& key,
                                             const juce::MemoryBlock& sealed,
                                             juce::String* errorMessage) {
    juce::MemoryBlock wav;
    if (!_spectra.openIr(key.data(), key.size(), sealed.getData(), sealed.getSize(), wav)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open the captured IR.";
        }
        return false;
    }

    auto stem = title.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
    stem = stem.trim();
    if (stem.isEmpty()) {
        stem = "Captured IR";
    }

    const auto directory = getSettingsFolder().getChildFile("Marketplace");
    if (!directory.isDirectory() && !directory.createDirectory().wasOk()) {
        wav.fillWith(0);
        if (errorMessage != nullptr) {
            *errorMessage = "Could not create the capture folder.";
        }
        return false;
    }

    auto file = directory.getChildFile(stem + ".wav");
    for (int suffix = 2; file.existsAsFile(); ++suffix) {
        file = directory.getChildFile(stem + " " + juce::String(suffix) + ".wav");
    }
    if (!file.replaceWithData(wav.getData(), wav.getSize())) {
        wav.fillWith(0);
        if (errorMessage != nullptr) {
            *errorMessage = "Could not write the impulse response.";
        }
        return false;
    }
    wav.fillWith(0);

    juce::NamedValueSet values;
    values.set("profileName", title.trim().isNotEmpty() ? title.trim() : stem);
    values.set("irPath", file.getFullPathName());
    return _profileManager.createProfile(values, errorMessage);
}

bool ProfilerAudioProcessor::renderIrCapture(juce::AudioBuffer<float>& buffer) {
    if (_irCapture.state.load(std::memory_order_acquire) != static_cast<int>(IrCaptureState::Recording)) {
        return false;
    }

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto* input = buffer.getReadPointer(0);
    auto index = _irCapture.index.load(std::memory_order_relaxed);
    const auto recordFrames = static_cast<std::uint32_t>(_irCapture.recorded.size());
    const auto sweepFrames = static_cast<std::uint32_t>(_irCapture.sweepFrames);
    for (int sample = 0; sample < numSamples; ++sample) {
        float output = 0.0f;
        if (index < recordFrames) {
            _irCapture.recorded[index] = input[sample];
            if (index < sweepFrames) {
                output = _irCapture.sweep[index];
            }
            ++index;
        }
        for (int channel = 0; channel < numChannels; ++channel) {
            buffer.getWritePointer(channel)[sample] = output;
        }
    }

    _irCapture.index.store(index, std::memory_order_release);
    if (index >= recordFrames) {
        _irCapture.state.store(static_cast<int>(IrCaptureState::Complete), std::memory_order_release);
    }
    _rmsLevelInput.store(juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, numSamples), -60.0f),
                         std::memory_order_relaxed);
    _rmsLevelOutput.store(juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, numSamples), -60.0f),
                          std::memory_order_relaxed);
    return true;
}

bool ProfilerAudioProcessor::beginAmpCapture(std::vector<float> send, juce::String* errorMessage) {
    const auto fail = [errorMessage](const juce::String& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    };

    releaseStoppedAmpCapture();
    if (_ampCapture.state.load(std::memory_order_acquire) != static_cast<int>(AmpCaptureState::Idle) ||
        _irCapture.state.load(std::memory_order_acquire) == static_cast<int>(IrCaptureState::Recording)) {
        return fail("A capture is already running.");
    }
    if (wrapperType != juce::AudioProcessor::wrapperType_Standalone) {
        return fail("Amp capture runs in the standalone.");
    }
    if (std::abs(getSampleRate() - AmpCapture::kSampleRate) > 0.5) {
        return fail("Set the audio device to 48 kHz.");
    }
    if (send.empty()) {
        return fail("The reamp signal is empty.");
    }

    const auto recordFrames = send.size() + static_cast<std::size_t>(AmpCapture::kTailFrames);
    _ampCapture.send = std::move(send);
    _ampCapture.recorded.assign(recordFrames, 0.0f);
    _ampCapture.index.store(0, std::memory_order_relaxed);
    _ampCapture.state.store(static_cast<int>(AmpCaptureState::Recording), std::memory_order_release);
    return true;
}

bool ProfilerAudioProcessor::isAmpCaptureRunning() const noexcept {
    const auto state = _ampCapture.state.load(std::memory_order_acquire);
    return state == static_cast<int>(AmpCaptureState::Recording) || state == static_cast<int>(AmpCaptureState::Stopping);
}

float ProfilerAudioProcessor::getAmpCaptureProgress() const noexcept {
    const auto frames = _ampCapture.recorded.size();
    if (frames == 0) {
        return 0.0f;
    }
    return static_cast<float>(_ampCapture.index.load(std::memory_order_acquire)) / static_cast<float>(frames);
}

bool ProfilerAudioProcessor::ampCaptureFinished() const noexcept {
    return _ampCapture.state.load(std::memory_order_acquire) == static_cast<int>(AmpCaptureState::Complete);
}

bool ProfilerAudioProcessor::takeAmpCapture(std::vector<float>& sent, std::vector<float>& recorded) {
    if (!ampCaptureFinished()) {
        return false;
    }
    sent = std::move(_ampCapture.send);
    recorded = std::move(_ampCapture.recorded);
    _ampCapture.send.clear();
    _ampCapture.recorded.clear();
    _ampCapture.state.store(static_cast<int>(AmpCaptureState::Idle), std::memory_order_release);
    return true;
}

void ProfilerAudioProcessor::stopAmpCapture() {
    auto expected = static_cast<int>(AmpCaptureState::Recording);
    _ampCapture.state.compare_exchange_strong(expected, static_cast<int>(AmpCaptureState::Stopping),
                                              std::memory_order_acq_rel);
}

bool ProfilerAudioProcessor::releaseStoppedAmpCapture() {
    if (_ampCapture.state.load(std::memory_order_acquire) != static_cast<int>(AmpCaptureState::Stopped)) {
        return false;
    }
    _ampCapture.send.clear();
    _ampCapture.recorded.clear();
    _ampCapture.state.store(static_cast<int>(AmpCaptureState::Idle), std::memory_order_release);
    return true;
}

bool ProfilerAudioProcessor::renderAmpCapture(juce::AudioBuffer<float>& buffer) {
    const auto state = _ampCapture.state.load(std::memory_order_acquire);
    if (state == static_cast<int>(AmpCaptureState::Stopping)) {
        _ampCapture.state.store(static_cast<int>(AmpCaptureState::Stopped), std::memory_order_release);
    }
    if (state == static_cast<int>(AmpCaptureState::Stopping) || state == static_cast<int>(AmpCaptureState::Stopped)) {
        buffer.clear();
        return true;
    }
    if (state != static_cast<int>(AmpCaptureState::Recording)) {
        return false;
    }

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto* input = buffer.getReadPointer(0);
    auto index = _ampCapture.index.load(std::memory_order_relaxed);
    const auto recordFrames = static_cast<std::uint32_t>(_ampCapture.recorded.size());
    const auto sendFrames = static_cast<std::uint32_t>(_ampCapture.send.size());
    const auto inputRms = buffer.getRMSLevel(0, 0, numSamples);
    for (int sample = 0; sample < numSamples; ++sample) {
        float output = 0.0f;
        if (index < recordFrames) {
            _ampCapture.recorded[index] = input[sample];
            if (index < sendFrames) {
                output = _ampCapture.send[index];
            }
            ++index;
        }
        for (int channel = 0; channel < numChannels; ++channel) {
            buffer.getWritePointer(channel)[sample] = output;
        }
    }

    _ampCapture.index.store(index, std::memory_order_release);
    if (index >= recordFrames) {
        _ampCapture.state.store(static_cast<int>(AmpCaptureState::Complete), std::memory_order_release);
    }
    _rmsLevelInput.store(juce::Decibels::gainToDecibels(inputRms, -60.0f), std::memory_order_relaxed);
    _rmsLevelOutput.store(juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, numSamples), -60.0f),
                          std::memory_order_relaxed);
    return true;
}

void ProfilerAudioProcessor::unloadAmpFile() {
    {
        const juce::ScopedLock ampLock(_ampModelLock);
        _ampLoaded = false;
        _ampInputSkip = 0;
        _ampInputGain = 1.0f;
        _ampOutputGain = 1.0f;
        _neuralAmp = nullptr;
        _ampCopies.clear();
        _ampModelBytes.reset();
        _ampFileLoaded = false;
        _currentAmpFile = juce::File{};
    }
    _spectraAmp.clear();
    updateReportedLatency();
}

bool ProfilerAudioProcessor::isIRLoaded() const noexcept {
    return _irLoaded;
}

bool ProfilerAudioProcessor::isAmpFileLoaded() const noexcept {
    const juce::ScopedLock ampLock(_ampModelLock);
    return _ampFileLoaded;
}

juce::File ProfilerAudioProcessor::getCurrentIRFile() const {
    return _currentIRFile;
}

juce::File ProfilerAudioProcessor::getCurrentAmpFile() const {
    const juce::ScopedLock ampLock(_ampModelLock);
    return _currentAmpFile;
}

bool ProfilerAudioProcessor::applyProfile(int profileIndex, juce::String* errorMessage) {
    if (!_profileManager.applyProfile(profileIndex, errorMessage)) {
        return false;
    }

    const auto* profile = _profileManager.getProfile(profileIndex);
    const auto values = _profileManager.getProfileValues(profileIndex);
    applyProfileFileValues(values);

    _appliedProfileId = profile != nullptr ? profile->id : juce::String{};
    return true;
}

void ProfilerAudioProcessor::syncLoadedFilesWithCurrentProfile() {
    const auto currentProfileIndex = _profileManager.getCurrentProfileIndex();
    const auto* profile = _profileManager.getProfile(currentProfileIndex);

    if (profile == nullptr) {
        unloadIRFile();
        unloadAmpFile();
        clearAppliedProfile();
        return;
    }

    applyProfileFileValues(_profileManager.getProfileValues(currentProfileIndex));
    _appliedProfileId = profile->id;
}

void ProfilerAudioProcessor::applyProfileFileValues(const juce::NamedValueSet& values) {
    if (const auto* irPath = values.getVarPointer("irPath")) {
        const auto irPathText = irPath->toString().trim();
        const auto irFile = juce::File(irPathText);
        if (irPathText.isEmpty()) {
            unloadIRFile();
        } else if (irFile.existsAsFile()) {
            loadIRFile(irFile);
        } else {
            unloadIRFile();
            _currentIRFile = irFile;
        }
    } else {
        unloadIRFile();
    }

    if (const auto* ampPath = values.getVarPointer("ampPath")) {
        const auto ampPathText = ampPath->toString().trim();
        const auto ampFile = juce::File(ampPathText);
        if (ampPathText.isEmpty()) {
            unloadAmpFile();
        } else if (ampFile.existsAsFile()) {
            loadAmpFile(ampFile);
        } else {
            unloadAmpFile();
            _currentAmpFile = ampFile;
        }
    } else {
        unloadAmpFile();
    }
}

juce::String ProfilerAudioProcessor::getAppliedProfileId() const {
    return _appliedProfileId;
}

void ProfilerAudioProcessor::clearAppliedProfile() {
    _appliedProfileId.clear();
}

ProfileManager& ProfilerAudioProcessor::getProfileManager() noexcept {
    return _profileManager;
}

const ProfileManager& ProfilerAudioProcessor::getProfileManager() const noexcept {
    return _profileManager;
}
