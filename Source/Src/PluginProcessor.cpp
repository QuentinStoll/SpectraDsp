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
const juce::Identifier kRouteHeaderId{"routeHeader"};
const juce::Identifier kRoutePathBId{"routePathB"};
const juce::Identifier kRouteBypassId{"routeBypass"};

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

void writeRouteState(juce::ValueTree state, std::uint32_t header, std::uint32_t pathB, std::uint8_t bypass) {
    if (!state.isValid()) {
        return;
    }
    state.setProperty(kRouteHeaderId, static_cast<int>(header), nullptr);
    state.setProperty(kRoutePathBId, static_cast<int>(pathB), nullptr);
    state.setProperty(kRouteBypassId, static_cast<int>(bypass), nullptr);
}

std::uint32_t readRouteHeader(const juce::ValueTree& tree) {
    return static_cast<std::uint32_t>(static_cast<int>(tree.getProperty(kRouteHeaderId, 0)));
}

std::uint32_t readRoutePathB(const juce::ValueTree& tree) {
    return static_cast<std::uint32_t>(static_cast<int>(tree.getProperty(kRoutePathBId, 0)));
}

std::uint8_t readRouteBypass(const juce::ValueTree& tree) {
    return static_cast<std::uint8_t>(static_cast<int>(tree.getProperty(kRouteBypassId, 0)) & 0x3F);
}

void splitPathGains(bool ab, float route, float& gainA, float& gainB) noexcept {
    if (!ab) {
        gainA = 1.0f;
        gainB = 1.0f;
        return;
    }
    const auto mix = juce::jlimit(0.0f, 1.0f, route);
    if (mix <= 0.5f) {
        gainA = 1.0f;
        gainB = mix * 2.0f;
    } else {
        gainA = (1.0f - mix) * 2.0f;
        gainB = 1.0f;
    }
}

void stereoBalance(float pan, float& leftGain, float& rightGain) noexcept {
    const auto limited = juce::jlimit(-100.0f, 100.0f, pan) / 100.0f;
    if (limited >= 0.0f) {
        leftGain = 1.0f - limited;
        rightGain = 1.0f;
    } else {
        leftGain = 1.0f;
        rightGain = 1.0f + limited;
    }
}

std::uint64_t readChainLayout(const juce::ValueTree& tree) {
    const auto lowDefault = static_cast<int>(SignalChain::defaultPacked & 0xFFFFFFFFu);
    const auto low = static_cast<std::uint32_t>(static_cast<int>(tree.getProperty(kChainLayoutId, lowDefault)));
    const auto high = static_cast<std::uint32_t>(static_cast<int>(tree.getProperty(kChainLayoutHighId, 0)));
    return (static_cast<std::uint64_t>(high) << 32) | static_cast<std::uint64_t>(low);
}

}  // namespace

struct BranchWorker : public juce::Thread {
    BranchWorker() : juce::Thread("Spectra Branch") {}

    ~BranchWorker() override { stop(); }

    void start() {
        if (isThreadRunning()) {
            return;
        }
        _quit.store(false, std::memory_order_release);
        if (!startRealtimeThread(juce::Thread::RealtimeOptions{}.withPriority(8))) {
            startThread(juce::Thread::Priority::high);
        }
    }

    void stop() {
        _quit.store(true, std::memory_order_release);
        _ready.signal();
        stopThread(2000);
    }

    using Job = void (*)(void*);

    void startJob(Job job, void* context) {
        _done.reset();
        _context = context;
        _job = job;
        _ready.signal();
    }

    void waitJob() {
        _done.wait();
        _job = nullptr;
        _context = nullptr;
    }

    void run() override {
        while (!threadShouldExit() && !_quit.load(std::memory_order_acquire)) {
            _ready.wait();
            if (threadShouldExit() || _quit.load(std::memory_order_acquire)) {
                break;
            }
            try {
                if (_job != nullptr) {
                    _job(_context);
                }
            } catch (...) {
            }
            _done.signal();
        }
    }

    juce::WaitableEvent _ready;
    juce::WaitableEvent _done;
    std::atomic<bool> _quit{false};
    Job _job = nullptr;
    void* _context = nullptr;
};

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
                                               juce::File playViewSettingsFile,
                                               juce::File presetDirectory,
                                               juce::File catalogDirectory)
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
      _profileManager(_apvts, std::move(profileDirectory), std::move(playViewSettingsFile)),
      _presetManager(std::move(presetDirectory)),
      _profileCatalog(std::move(catalogDirectory))
#else
    : _fx(_apvts),
      _profileManager(_apvts, std::move(profileDirectory), std::move(playViewSettingsFile)),
      _presetManager(std::move(presetDirectory)),
      _profileCatalog(std::move(catalogDirectory))
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
    _pedalModelParam = _apvts.getRawParameterValue("pedalModel");
    _conditionProfileParam = _apvts.getRawParameterValue("conditionProfile");
    _conditionAmountParam = _apvts.getRawParameterValue("conditionAmount");
    _splitModeParam = _apvts.getRawParameterValue("splitMode");
    _splitRouteParam = _apvts.getRawParameterValue("splitRoute");
    _routeALevelParam = _apvts.getRawParameterValue("routeALevel");
    _routeAPanParam = _apvts.getRawParameterValue("routeAPan");
    _routeBLevelParam = _apvts.getRawParameterValue("routeBLevel");
    _routeBPanParam = _apvts.getRawParameterValue("routeBPan");
    _routeBPolarityParam = _apvts.getRawParameterValue("routeBPolarity");
    _routeLevelParam = _apvts.getRawParameterValue("routeLevel");
    writeChainLayout(_apvts.state, _chainLayoutPacked.load());
    writeChainBypass(_apvts.state, 0);
    writeRouteState(_apvts.state, 0, 0, 0);
    for (auto& mix : _ampWetMix) {
        mix.setCurrentAndTargetValue(1.0f);
    }
    _spectra.open();
}

ProfilerAudioProcessor::~ProfilerAudioProcessor() {
    stopBranchWorker();
}

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
    for (const auto stage : getParallelRoute().pathB) {
        tail = juce::jmax(tail, _fx.tailSecondsFor(stage));
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

    for (auto& drive : _pedalDrives) {
        drive.prepare(sampleRate, samplesPerBlock);
    }
    const auto conditionSalt = static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt());
    for (int instance = 0; instance < kChainCopies; ++instance) {
        _ampConditions[static_cast<std::size_t>(instance)].prepare(
            sampleRate, conditionSalt + static_cast<std::uint32_t>(instance) * 0x9E3779B9u);
    }
    const auto scratchFrames = static_cast<std::size_t>(juce::jmax(1, samplesPerBlock));
    for (auto& scratch : _ampInputScratch) {
        scratch.assign(scratchFrames, 0.0f);
    }
    for (auto& scratch : _ampWetScratch) {
        scratch.assign(scratchFrames, 0.0f);
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
    for (auto& channel : _splitLeft) {
        channel.assign(scratchFrames, 0.0f);
    }
    for (auto& channel : _splitRight) {
        channel.assign(scratchFrames, 0.0f);
    }
    for (int lane = 0; lane < 2; ++lane) {
        _alignLeft[static_cast<size_t>(lane)].assign(48000, 0.0f);
        _alignRight[static_cast<size_t>(lane)].assign(48000, 0.0f);
        _alignWrite[static_cast<size_t>(lane)] = 0;
    }
    _spectraAmp.prepare(sampleRate, samplesPerBlock);
    startBranchWorker();
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
    stopBranchWorker();
    _chain.reset();
    for (auto& eqChain : _eqChains) {
        eqChain.reset();
    }
    for (auto& lowCut : _cabLowCuts) {
        lowCut.reset();
    }
    for (auto& drive : _pedalDrives) {
        drive.reset();
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
    if (right != nullptr) {
        juce::FloatVectorOperations::copy(right, left, numSamples);
    }

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
    if (_cabLowCutSmoothed.isSmoothing()) {
        updateCabLowCutCoefficients();
    }
    ensureAudioScratch(numSamples);
    const auto layout = SignalChain::Layout::fromPacked(_chainLayoutPacked.load(std::memory_order_relaxed));
    const auto graph = getForkGraph();
    const auto route = getParallelRoute();
    const float ampGain = juce::Decibels::decibelsToGain(getParameterValue(_gainParam, 0.0f));
    if (graph.forks.size() > 1) {
        processForkSequence(layout, graph, left, right, numSamples, ampGain);
    } else if (!route.active()) {
        BranchTask serial;
        serial.processor = this;
        serial.layout = layout;
        serial.left = left;
        serial.right = right;
        serial.numSamples = numSamples;
        serial.begin = 0;
        serial.end = SignalChain::movableSlotCount;
        serial.sharedGain = true;
        serial.ampGain = ampGain;
        processBranch(serial);
    } else {
        auto mixedRoute = route;
        if (!graph.forks.empty()) {
            mixedRoute.mix = graph.forks.front().mix;
        }
        processParallel(layout, mixedRoute, left, right, numSamples, ampGain);
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
    const auto route = getParallelRoute();
    int latency = 0;
    if (!route.active()) {
        latency = latencyRange(layout, 0, SignalChain::movableSlotCount, false);
    } else {
        latency = latencyRange(layout, 0, route.splitAfter + 1, true);
        const auto pathA = latencyRange(layout, route.splitAfter + 1, route.mergeAfter + 1, true);
        const auto pathB = latencyPathB(route, true);
        latency += juce::jmax(pathA, pathB);
        latency += latencyRange(layout, route.mergeAfter + 1, SignalChain::movableSlotCount, true);
    }
    if (latency != _reportedLatency) {
        _reportedLatency = latency;
        setLatencySamples(latency);
    }
}

SignalChain::Layout ProfilerAudioProcessor::getChainLayout() const noexcept {
    return SignalChain::Layout::fromPacked(_chainLayoutPacked.load(std::memory_order_relaxed));
}

SignalChain::ParallelRoute ProfilerAudioProcessor::getParallelRoute() const noexcept {
    return SignalChain::ParallelRoute::fromPacked(_routeHeader.load(std::memory_order_relaxed),
                                                  _routePathB.load(std::memory_order_relaxed));
}

void ProfilerAudioProcessor::setParallelRoute(const SignalChain::ParallelRoute& route) {
    SignalChain::ParallelRoute clean;
    clean.splitAfter = juce::jlimit(-1, SignalChain::movableSlotCount - 1, route.splitAfter);
    clean.mergeAfter = juce::jlimit(-1, SignalChain::movableSlotCount - 1, route.mergeAfter);
    if (clean.mergeAfter < 0 || clean.mergeAfter >= SignalChain::movableSlotCount ||
        clean.mergeAfter <= clean.splitAfter) {
        clean.splitAfter = -1;
        clean.mergeAfter = -1;
    }
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        const auto stage = route.pathBAt(index);
        clean.pathB[static_cast<size_t>(index)] =
            SignalChain::isEffectStage(stage) ? stage : SignalChain::Stage::Empty;
    }

    const auto previous = getParallelRoute();
    auto bypass = _routeBypass.load(std::memory_order_relaxed);
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        if (previous.pathBAt(index) != clean.pathBAt(index)) {
            bypass = static_cast<std::uint8_t>(bypass & ~(1u << index));
        }
    }
    _routeBypass.store(bypass, std::memory_order_relaxed);
    _routeHeader.store(clean.packedHeader(), std::memory_order_relaxed);
    _routePathB.store(clean.packedPathB(), std::memory_order_relaxed);
    writeRouteState(_apvts.state, clean.packedHeader(), clean.packedPathB(), bypass);
    {
        const juce::ScopedLock lock(_forkLock);
        clean.mix = juce::jlimit(0.0f, 1.0f, route.mix);
        _forkGraph = SignalChain::graphFromRoute(clean);
    }
    syncAmpCopies();
}

void ProfilerAudioProcessor::clearParallelRoute() {
    _routeHeader.store(0, std::memory_order_relaxed);
    _routePathB.store(0, std::memory_order_relaxed);
    _routeBypass.store(0, std::memory_order_relaxed);
    writeRouteState(_apvts.state, 0, 0, 0);
    {
        const juce::ScopedLock lock(_forkLock);
        _forkGraph = {};
    }
    syncAmpCopies();
}

SignalChain::ForkGraph ProfilerAudioProcessor::getForkGraph() const {
    const juce::ScopedLock lock(_forkLock);
    return _forkGraph;
}

void ProfilerAudioProcessor::setForkGraph(const SignalChain::ForkGraph& graph) {
    SignalChain::ForkGraph valid;
    auto sorted = graph;
    SignalChain::sortForks(sorted);
    for (auto fork : sorted.forks) {
        fork.mix = juce::jlimit(0.0f, 1.0f, fork.mix);
        if (!SignalChain::forkIntervalFits(valid, fork.row, fork.splitGap, fork.mergeGap)) {
            continue;
        }
        valid.forks.push_back(fork);
    }
    {
        const juce::ScopedLock lock(_forkLock);
        _forkGraph = valid;
    }
    if (valid.forks.empty()) {
        _routeHeader.store(0, std::memory_order_relaxed);
        _routePathB.store(0, std::memory_order_relaxed);
        writeRouteState(_apvts.state, 0, 0, _routeBypass.load(std::memory_order_relaxed));
    } else {
        const auto route = SignalChain::routeFromFork(valid.forks.front());
        _routeHeader.store(route.packedHeader(), std::memory_order_relaxed);
        _routePathB.store(route.packedPathB(), std::memory_order_relaxed);
        writeRouteState(_apvts.state, route.packedHeader(), route.packedPathB(), _routeBypass.load(std::memory_order_relaxed));
    }
    syncAmpCopies();
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
    const auto pathB = SignalChain::pathBIndexForSlot(chainSlot);
    if (pathB >= 0) {
        return isPathBBypassed(pathB);
    }
    const int bit = SignalChain::movableIndexForSlot(chainSlot);
    if (bit < 0) {
        return false;
    }
    return (_slotBypassMask.load(std::memory_order_relaxed) & (1u << bit)) != 0;
}

void ProfilerAudioProcessor::setChainSlotBypassed(int chainSlot, bool bypassed) {
    const auto pathB = SignalChain::pathBIndexForSlot(chainSlot);
    if (pathB >= 0) {
        auto mask = _routeBypass.load(std::memory_order_relaxed);
        if (bypassed) {
            mask = static_cast<std::uint8_t>(mask | (1u << pathB));
        } else {
            mask = static_cast<std::uint8_t>(mask & ~(1u << pathB));
        }
        _routeBypass.store(mask, std::memory_order_relaxed);
        writeRouteState(_apvts.state,
                        _routeHeader.load(std::memory_order_relaxed),
                        _routePathB.load(std::memory_order_relaxed),
                        mask);
        return;
    }
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
    const auto pathB = SignalChain::pathBIndexForSlot(slot);
    if (pathB >= 0) {
        auto route = getParallelRoute();
        route.clearPathB(pathB);
        setParallelRoute(route);
        return;
    }
    auto layout = getChainLayout();
    layout.clear(slot);
    setChainLayout(layout);
}

void ProfilerAudioProcessor::clearAllChainBlocks() {
    auto layout = getChainLayout();
    layout.clearAll();
    setChainLayout(layout);
    clearParallelRoute();
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
        const auto routeAmps = [&] {
            int count = 0;
            const auto route = getParallelRoute();
            for (const auto stage : route.pathB) {
                if (stage == SignalChain::Stage::Amp) {
                    ++count;
                }
            }
            return count;
        }();
        const auto extraAmps = juce::jmax(0, getChainLayout().count(SignalChain::Stage::Amp) + routeAmps - 1);
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

    int pathBAmps = 0;
    for (const auto& fork : getForkGraph().forks) {
        for (int index = 0; index < fork.columnCount(); ++index) {
            if (fork.laneAt(index) == SignalChain::Stage::Amp) {
                ++pathBAmps;
            }
        }
    }
    _spectraAmp.setInstanceCount(juce::jmax(1, getChainLayout().count(SignalChain::Stage::Amp) + pathBAmps));
}

void ProfilerAudioProcessor::resetChainLayout() {
    setChainLayout({});
    clearParallelRoute();
}

void ProfilerAudioProcessor::placeChainStage(SignalChain::Stage stage, int slot) {
    const auto pathB = SignalChain::pathBIndexForSlot(slot);
    if (pathB >= 0) {
        auto route = getParallelRoute();
        route.placePathB(stage, pathB);
        setParallelRoute(route);
        return;
    }
    auto layout = getChainLayout();
    layout.place(stage, slot);
    setChainLayout(layout);
}

void ProfilerAudioProcessor::processGateStage(juce::dsp::ProcessContextReplacing<float>& context) {
    if (!_chain.isBypassed<NoiseGate>()) {
        _chain.get<NoiseGate>().process(context);
    }
}

void ProfilerAudioProcessor::processAmpStage(float* channel,
                                             int numSamples,
                                             int instance,
                                             bool slotEnabled,
                                             bool sharedGain,
                                             float ampGain) {
    if (channel == nullptr || numSamples <= 0) {
        return;
    }

    float* channels[] = {channel};
    juce::dsp::AudioBlock<float> block(channels, 1, static_cast<size_t>(numSamples));
    juce::dsp::ProcessContextReplacing<float> context(block);
    if (sharedGain) {
        _chain.get<Gain>().process(context);
    } else {
        juce::FloatVectorOperations::multiply(channel, ampGain, numSamples);
    }

    const auto mixIndex = static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance));
    auto& wetMix = _ampWetMix[mixIndex];
    const bool ampEnabled = slotEnabled && getParameterValue(_isAmpEnabledParam, 1.0f) > 0.5f;
    wetMix.setTargetValue(ampEnabled ? 1.0f : 0.0f);

    auto& condition = _ampConditions[mixIndex];
    condition.set(static_cast<int>(std::lround(getParameterValue(_conditionProfileParam, 0.0f))),
                  getParameterValue(_conditionAmountParam, 0.0f) * 0.01f);
    auto& driven = _ampInputScratch[mixIndex];
    auto& wetBuffer = _ampWetScratch[mixIndex];
    if (static_cast<int>(driven.size()) < numSamples || static_cast<int>(wetBuffer.size()) < numSamples) {
        wetMix.skip(numSamples);
        _dcBlockers[mixIndex].process(context);
        return;
    }

    auto mixConditioned = [&]() {
        condition.processPost(wetBuffer.data(), numSamples);
        for (int i = 0; i < numSamples; ++i) {
            const float dry = channel[i];
            float wet = wetBuffer[static_cast<size_t>(i)];
            if (!std::isfinite(wet)) {
                wet = dry;
            }
            const float mix = wetMix.getNextValue();
            channel[i] = dry + (wet - dry) * mix;
        }
    };

    if (_spectraAmp.isLoaded()) {
        juce::FloatVectorOperations::copy(driven.data(), channel, numSamples);
        condition.processPre(driven.data(), numSamples);
        if (_spectraAmp.render(instance, driven.data(), wetBuffer.data(), numSamples)) {
            mixConditioned();
        } else {
            wetMix.skip(numSamples);
        }
        _dcBlockers[mixIndex].process(context);
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

            // Keep the recurrent state tracking the input while the block is bypassed.
            // Freezing it makes the amp take seconds to recover when it is switched back on.
            for (int i = 0; i < numSamples; ++i) {
                driven[static_cast<size_t>(i)] = channel[i] * inputGain;
            }
            condition.processPre(driven.data(), numSamples);
            for (int i = 0; i < numSamples; ++i) {
                const float inputSample[] = {driven[static_cast<size_t>(i)]};
                model->forward(inputSample);
                float wet = model->getOutputs()[0];

                if (!std::isfinite(wet)) {
                    model->reset();
                    wet = 0.0f;
                }

                // CoreAudioML / AIDA models store a residual in in_skip: the network
                // predicts the difference, and the input channels are added back.
                if (inputSkip > 0) {
                    wet += channel[i] * inputGain;
                }
                wet *= outputGain;
                wetBuffer[static_cast<size_t>(i)] = std::isfinite(wet) ? wet : channel[i];
            }
            mixConditioned();
        } else {
            wetMix.skip(numSamples);
        }
    }

    _dcBlockers[mixIndex].process(context);
}

void ProfilerAudioProcessor::processCabStage(float* channel, int numSamples, int instance, bool slotEnabled) {
    const bool cabEnabled = slotEnabled && getParameterValue(_isCabEnabledParam, 1.0f) > 0.5f;
    if (channel == nullptr || numSamples <= 0 || !_irLoaded || !cabEnabled) {
        return;
    }

    float* channels[] = {channel};
    juce::dsp::AudioBlock<float> block(channels, 1, static_cast<size_t>(numSamples));
    juce::dsp::ProcessContextReplacing<float> context(block);
    const auto index = static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance));
    _cabConvolvers[index].process(context);
    _cabLowCuts[index].process(context);
}

void ProfilerAudioProcessor::processEqStage(float* channel, int numSamples, int instance, bool slotEnabled) {
    if (channel == nullptr || numSamples <= 0 || !slotEnabled) {
        return;
    }
    float* channels[] = {channel};
    juce::dsp::AudioBlock<float> block(channels, 1, static_cast<size_t>(numSamples));
    juce::dsp::ProcessContextReplacing<float> context(block);
    _eqChains[static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance))].process(context);
}

void ProfilerAudioProcessor::processPedalStage(float* channel,
                                               int numSamples,
                                               int instance,
                                               bool slotEnabled) {
    if (channel == nullptr || numSamples <= 0 || !slotEnabled || getParameterValue(_isPedalEnabledParam, 1.0f) <= 0.5f) {
        return;
    }

    const auto index = static_cast<size_t>(juce::jlimit(0, kChainCopies - 1, instance));
    const auto model = (int)std::lround(getParameterValue(_pedalModelParam, 0.0f));
    _pedalDrives[index].process(channel,
                                numSamples,
                                model,
                                getParameterValue(_pedalDriveParam, 4.0f),
                                getParameterValue(_pedalToneParam, 65.0f),
                                getParameterValue(_pedalLevelParam, 0.0f));
}

void ProfilerAudioProcessor::noteStage(SignalChain::Stage stage, StageCursor& cursor) const noexcept {
    switch (stage) {
        case SignalChain::Stage::Amp:
            ++cursor.amp;
            break;
        case SignalChain::Stage::Cab:
            ++cursor.cab;
            break;
        case SignalChain::Stage::Eq:
            ++cursor.eq;
            break;
        case SignalChain::Stage::Pedal:
            ++cursor.pedal;
            break;
        case SignalChain::Stage::Empty:
            break;
        default: {
            const auto index = static_cast<size_t>(static_cast<int>(stage));
            if (index < cursor.fx.size()) {
                ++cursor.fx[index];
            }
            break;
        }
    }
}

void ProfilerAudioProcessor::advanceCursor(const SignalChain::Layout& layout,
                                           int begin,
                                           int end,
                                           StageCursor& cursor) const noexcept {
    for (int index = begin; index < end; ++index) {
        noteStage(layout.atSlot(SignalChain::chainSlotForMovableIndex(index)), cursor);
    }
}

int ProfilerAudioProcessor::latencyFor(SignalChain::Stage stage, bool slotBypassed, bool includeCab) const {
    if (slotBypassed || stage == SignalChain::Stage::Empty) {
        return 0;
    }
    int latency = _fx.latencySamplesFor(stage);
    if (stage == SignalChain::Stage::Amp && _spectraAmp.isLoaded()) {
        latency += _spectraAmp.latencySamples();
    }
    if (stage == SignalChain::Stage::Pedal && getParameterValue(_isPedalEnabledParam, 1.0f) > 0.5f) {
        latency += _pedalDrives[0].latencySamples();
    }
    if (includeCab && stage == SignalChain::Stage::Cab && _irLoaded &&
        getParameterValue(_isCabEnabledParam, 1.0f) > 0.5f) {
        latency += _cabConvolvers[0].getLatency();
    }
    return latency;
}

int ProfilerAudioProcessor::latencyRange(const SignalChain::Layout& layout,
                                         int begin,
                                         int end,
                                         bool includeCab) const {
    int latency = 0;
    for (int index = begin; index < end; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        latency += latencyFor(layout.atSlot(slot), isChainSlotBypassed(slot), includeCab);
    }
    return latency;
}

int ProfilerAudioProcessor::latencyPathB(const SignalChain::ParallelRoute& route, bool includeCab) const {
    int latency = 0;
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        latency += latencyFor(route.pathBAt(index), isPathBBypassed(index), includeCab);
    }
    return latency;
}

bool ProfilerAudioProcessor::rangeHas(const SignalChain::Layout& layout,
                                      int begin,
                                      int end,
                                      SignalChain::Stage stage) const noexcept {
    for (int index = begin; index < end; ++index) {
        if (layout.atSlot(SignalChain::chainSlotForMovableIndex(index)) == stage) {
            return true;
        }
    }
    return false;
}

bool ProfilerAudioProcessor::pathBHas(const SignalChain::ParallelRoute& route, SignalChain::Stage stage) const noexcept {
    for (const auto candidate : route.pathB) {
        if (candidate == stage) {
            return true;
        }
    }
    return false;
}

bool ProfilerAudioProcessor::rangeIsHeavy(const SignalChain::Layout& layout, int begin, int end) const {
    for (int index = begin; index < end; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        const auto stage = layout.atSlot(slot);
        if (stage == SignalChain::Stage::Amp && (_spectraAmp.isLoaded() || _ampLoaded)) {
            return true;
        }
        if (stage == SignalChain::Stage::Cab && !isChainSlotBypassed(slot) && _irLoaded &&
            getParameterValue(_isCabEnabledParam, 1.0f) > 0.5f) {
            return true;
        }
    }
    return false;
}

bool ProfilerAudioProcessor::pathBIsHeavy(const SignalChain::ParallelRoute& route) const {
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        const auto stage = route.pathBAt(index);
        if (stage == SignalChain::Stage::Amp && (_spectraAmp.isLoaded() || _ampLoaded)) {
            return true;
        }
        if (stage == SignalChain::Stage::Cab && !isPathBBypassed(index) && _irLoaded &&
            getParameterValue(_isCabEnabledParam, 1.0f) > 0.5f) {
            return true;
        }
    }
    return false;
}

bool ProfilerAudioProcessor::isPathBBypassed(int index) const noexcept {
    if (index < 0 || index >= SignalChain::ParallelRoute::pathBCount) {
        return false;
    }
    return (_routeBypass.load(std::memory_order_relaxed) & (1u << index)) != 0;
}

void ProfilerAudioProcessor::ensureAudioScratch(int numSamples) {
    const auto frames = static_cast<std::size_t>(juce::jmax(1, numSamples));
    for (auto& scratch : _ampInputScratch) {
        if (scratch.size() < frames) {
            scratch.resize(frames);
        }
    }
    for (auto& scratch : _ampWetScratch) {
        if (scratch.size() < frames) {
            scratch.resize(frames);
        }
    }
    for (int lane = 0; lane < 2; ++lane) {
        if (_splitLeft[static_cast<size_t>(lane)].size() < frames) {
            _splitLeft[static_cast<size_t>(lane)].resize(frames);
        }
        if (_splitRight[static_cast<size_t>(lane)].size() < frames) {
            _splitRight[static_cast<size_t>(lane)].resize(frames);
        }
    }
}

void ProfilerAudioProcessor::applyAlign(int lane, float* left, float* right, int numSamples, int delaySamples) {
    if (left == nullptr || numSamples <= 0 || delaySamples <= 0) {
        return;
    }
    const auto index = static_cast<size_t>(juce::jlimit(0, 1, lane));
    auto& historyL = _alignLeft[index];
    auto& historyR = _alignRight[index];
    if (historyL.empty() || historyR.empty()) {
        return;
    }
    const int capacity = static_cast<int>(historyL.size());
    const int delay = juce::jmin(delaySamples, capacity - 1);
    int write = _alignWrite[index];
    for (int sample = 0; sample < numSamples; ++sample) {
        historyL[static_cast<size_t>(write)] = left[sample];
        historyR[static_cast<size_t>(write)] = right != nullptr ? right[sample] : left[sample];
        auto read = write - delay;
        if (read < 0) {
            read += capacity;
        }
        left[sample] = historyL[static_cast<size_t>(read)];
        if (right != nullptr) {
            right[sample] = historyR[static_cast<size_t>(read)];
        }
        write = (write + 1) % capacity;
    }
    _alignWrite[index] = write;
}

void ProfilerAudioProcessor::startBranchWorker() {
    if (_branchWorker == nullptr) {
        _branchWorker = std::make_unique<BranchWorker>();
    }
    _branchWorker->start();
}

void ProfilerAudioProcessor::stopBranchWorker() {
    if (_branchWorker != nullptr) {
        _branchWorker->stop();
    }
}

void ProfilerAudioProcessor::runBranchJob(void* raw) {
    auto* task = static_cast<BranchTask*>(raw);
    if (task != nullptr && task->processor != nullptr) {
        task->processor->processBranch(*task);
    }
}

void ProfilerAudioProcessor::processBranch(BranchTask& task) {
    auto foldToMono = [&task]() {
        if (task.right == nullptr) {
            return;
        }
        for (int sample = 0; sample < task.numSamples; ++sample) {
            task.left[sample] = 0.5f * (task.left[sample] + task.right[sample]);
        }
        juce::FloatVectorOperations::copy(task.right, task.left, task.numSamples);
    };
    auto mirror = [&task]() {
        if (task.right != nullptr) {
            juce::FloatVectorOperations::copy(task.right, task.left, task.numSamples);
        }
    };

    auto runStage = [&](SignalChain::Stage stage, bool slotEnabled) {
        switch (stage) {
            case SignalChain::Stage::Cab:
                foldToMono();
                processCabStage(task.left, task.numSamples, task.cursor.cab++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Eq:
                foldToMono();
                processEqStage(task.left, task.numSamples, task.cursor.eq++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Pedal:
                foldToMono();
                processPedalStage(task.left, task.numSamples, task.cursor.pedal++, slotEnabled);
                mirror();
                break;
            case SignalChain::Stage::Amp:
                foldToMono();
                processAmpStage(task.left, task.numSamples, task.cursor.amp++, slotEnabled, task.sharedGain, task.ampGain);
                mirror();
                break;
            case SignalChain::Stage::Empty:
                break;
            default: {
                const auto stageIndex = static_cast<size_t>(static_cast<int>(stage));
                const int instance = stageIndex < task.cursor.fx.size() ? task.cursor.fx[stageIndex]++ : 0;
                _fx.process(stage,
                            instance,
                            task.left,
                            task.right != nullptr ? task.right : task.left,
                            task.numSamples,
                            slotEnabled,
                            _directInputLeft.data(),
                            _directInputRight.data(),
                            task.lane);
                break;
            }
        }
    };

    if (task.pathB) {
        for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
            runStage(task.route.pathBAt(index), !isPathBBypassed(index));
        }
        return;
    }

    for (int index = task.begin; index < task.end; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        runStage(task.layout.atSlot(slot), !isChainSlotBypassed(slot));
    }
}

void ProfilerAudioProcessor::processForkSequence(const SignalChain::Layout& layout,
                                                 const SignalChain::ForkGraph& graph,
                                                 float* left,
                                                 float* right,
                                                 int numSamples,
                                                 float ampGain) {
    auto ordered = graph;
    SignalChain::sortForks(ordered);
    int cursor = 0;
    StageCursor stageCursor{};
    ensureAudioScratch(numSamples);
    for (const auto& fork : ordered.forks) {
        const auto begin = SignalChain::movableForFork(fork.row, fork.splitGap);
        const auto end = SignalChain::movableForFork(fork.row, fork.mergeGap);
        if (begin < cursor || end <= begin || end > SignalChain::movableSlotCount) {
            continue;
        }
        if (begin > cursor) {
            BranchTask head;
            head.processor = this;
            head.layout = layout;
            head.left = left;
            head.right = right;
            head.numSamples = numSamples;
            head.begin = cursor;
            head.end = begin;
            head.sharedGain = true;
            head.ampGain = ampGain;
            head.cursor = stageCursor;
            processBranch(head);
            stageCursor = head.cursor;
        }

        auto& leftA = _splitLeft[0];
        auto& rightA = _splitRight[0];
        auto& leftB = _splitLeft[1];
        auto& rightB = _splitRight[1];
        std::copy(left, left + numSamples, leftA.begin());
        if (right != nullptr) {
            std::copy(right, right + numSamples, rightA.begin());
        } else {
            std::copy(left, left + numSamples, rightA.begin());
        }
        std::copy(leftA.begin(), leftA.begin() + numSamples, leftB.begin());
        std::copy(rightA.begin(), rightA.begin() + numSamples, rightB.begin());

        BranchTask pathA;
        pathA.processor = this;
        pathA.layout = layout;
        pathA.left = leftA.data();
        pathA.right = rightA.data();
        pathA.numSamples = numSamples;
        pathA.begin = begin;
        pathA.end = end;
        pathA.sharedGain = false;
        pathA.ampGain = ampGain;
        pathA.lane = 0;
        pathA.cursor = stageCursor;
        processBranch(pathA);

        auto cursorB = stageCursor;
        advanceCursor(layout, begin, end, cursorB);
        BranchTask pathB;
        pathB.processor = this;
        pathB.layout = layout;
        pathB.route = SignalChain::routeFromFork(fork);
        pathB.left = leftB.data();
        pathB.right = rightB.data();
        pathB.numSamples = numSamples;
        pathB.pathB = true;
        pathB.sharedGain = false;
        pathB.ampGain = ampGain;
        pathB.lane = 1;
        pathB.cursor = cursorB;
        processBranch(pathB);

        const auto mix = juce::jlimit(0.0f, 1.0f, fork.mix);
        for (int sample = 0; sample < numSamples; ++sample) {
            left[sample] = (1.0f - mix) * leftA[static_cast<size_t>(sample)] + mix * leftB[static_cast<size_t>(sample)];
            if (right != nullptr) {
                right[sample] = (1.0f - mix) * rightA[static_cast<size_t>(sample)] + mix * rightB[static_cast<size_t>(sample)];
            }
        }
        stageCursor = pathA.cursor;
        for (int index = 0; index < fork.columnCount(); ++index) {
            noteStage(fork.laneAt(index), stageCursor);
        }
        cursor = end;
    }
    if (cursor < SignalChain::movableSlotCount) {
        BranchTask tail;
        tail.processor = this;
        tail.layout = layout;
        tail.left = left;
        tail.right = right;
        tail.numSamples = numSamples;
        tail.begin = cursor;
        tail.end = SignalChain::movableSlotCount;
        tail.sharedGain = true;
        tail.ampGain = ampGain;
        tail.cursor = stageCursor;
        processBranch(tail);
    }
}

void ProfilerAudioProcessor::processParallel(const SignalChain::Layout& layout,
                                             const SignalChain::ParallelRoute& route,
                                             float* left,
                                             float* right,
                                             int numSamples,
                                             float ampGain) {
    BranchTask trunk;
    trunk.processor = this;
    trunk.layout = layout;
    trunk.left = left;
    trunk.right = right;
    trunk.numSamples = numSamples;
    trunk.begin = 0;
    trunk.end = route.splitAfter + 1;
    trunk.sharedGain = true;
    trunk.ampGain = ampGain;
    processBranch(trunk);

    float gainA = 1.0f;
    float gainB = 1.0f;
    splitPathGains(std::lround(getParameterValue(_splitModeParam, 0.0f)) >= 1,
                   getParameterValue(_splitRouteParam, 0.5f),
                   gainA,
                   gainB);
    const bool runA = gainA > 0.0001f;
    const bool runB = gainB > 0.0001f;

    auto& leftA = _splitLeft[0];
    auto& rightA = _splitRight[0];
    auto& leftB = _splitLeft[1];
    auto& rightB = _splitRight[1];
    std::copy(left, left + numSamples, leftA.begin());
    if (right != nullptr) {
        std::copy(right, right + numSamples, rightA.begin());
    } else {
        std::copy(left, left + numSamples, rightA.begin());
    }
    std::copy(leftA.begin(), leftA.begin() + numSamples, leftB.begin());
    std::copy(rightA.begin(), rightA.begin() + numSamples, rightB.begin());
    if (runA) {
        juce::FloatVectorOperations::multiply(leftA.data(), gainA, numSamples);
        juce::FloatVectorOperations::multiply(rightA.data(), gainA, numSamples);
    } else {
        juce::FloatVectorOperations::clear(leftA.data(), numSamples);
        juce::FloatVectorOperations::clear(rightA.data(), numSamples);
    }
    if (runB) {
        juce::FloatVectorOperations::multiply(leftB.data(), gainB, numSamples);
        juce::FloatVectorOperations::multiply(rightB.data(), gainB, numSamples);
    } else {
        juce::FloatVectorOperations::clear(leftB.data(), numSamples);
        juce::FloatVectorOperations::clear(rightB.data(), numSamples);
    }

    BranchTask pathA;
    pathA.processor = this;
    pathA.layout = layout;
    pathA.left = leftA.data();
    pathA.right = rightA.data();
    pathA.numSamples = numSamples;
    pathA.begin = route.splitAfter + 1;
    pathA.end = route.mergeAfter + 1;
    pathA.sharedGain = false;
    pathA.ampGain = ampGain;
    pathA.lane = 0;
    advanceCursor(layout, 0, pathA.begin, pathA.cursor);

    BranchTask pathB;
    pathB.processor = this;
    pathB.layout = layout;
    pathB.route = route;
    pathB.left = leftB.data();
    pathB.right = rightB.data();
    pathB.numSamples = numSamples;
    pathB.pathB = true;
    pathB.sharedGain = false;
    pathB.ampGain = ampGain;
    pathB.lane = 1;
    advanceCursor(layout, 0, SignalChain::movableSlotCount, pathB.cursor);

    const bool legacyAmp = !_spectraAmp.isLoaded() && _ampLoaded &&
                           ((runA && rangeHas(layout, pathA.begin, pathA.end, SignalChain::Stage::Amp)) ||
                            (runB && pathBHas(route, SignalChain::Stage::Amp)));
    const bool dualTuner = runA && runB && rangeHas(layout, pathA.begin, pathA.end, SignalChain::Stage::Tuner) &&
                           pathBHas(route, SignalChain::Stage::Tuner);
    const bool heavyA = runA && rangeIsHeavy(layout, pathA.begin, pathA.end);
    const bool heavyB = runB && pathBIsHeavy(route);
    const bool useWorker = heavyA && heavyB && !legacyAmp && !dualTuner && _branchWorker != nullptr &&
                           _branchWorker->isThreadRunning();

    if (useWorker) {
        ZoneScopedN("branch-join");
        _branchWorker->startJob(&ProfilerAudioProcessor::runBranchJob, &pathB);
        processBranch(pathA);
        _branchWorker->waitJob();
    } else {
        if (runA) {
            processBranch(pathA);
        }
        if (runB) {
            processBranch(pathB);
        }
    }

    const int latencyA = latencyRange(layout, pathA.begin, pathA.end, true);
    const int latencyB = latencyPathB(route, true);
    applyAlign(0, leftA.data(), rightA.data(), numSamples, latencyB - latencyA);
    applyAlign(1, leftB.data(), rightB.data(), numSamples, latencyA - latencyB);

    float balanceAL = 1.0f;
    float balanceAR = 1.0f;
    float balanceBL = 1.0f;
    float balanceBR = 1.0f;
    stereoBalance(getParameterValue(_routeAPanParam, 0.0f), balanceAL, balanceAR);
    stereoBalance(getParameterValue(_routeBPanParam, 0.0f), balanceBL, balanceBR);
    const float levelA = juce::Decibels::decibelsToGain(getParameterValue(_routeALevelParam, 0.0f));
    const float levelB = juce::Decibels::decibelsToGain(getParameterValue(_routeBLevelParam, 0.0f)) *
                         (getParameterValue(_routeBPolarityParam, 0.0f) > 0.5f ? -1.0f : 1.0f);
    const float levelMerge = juce::Decibels::decibelsToGain(getParameterValue(_routeLevelParam, 0.0f));
    const float forkMix = juce::jlimit(0.0f, 1.0f, route.mix);
    const float mixA = runA && runB ? 1.0f - forkMix : 1.0f;
    const float mixB = runA && runB ? forkMix : 1.0f;
    for (int sample = 0; sample < numSamples; ++sample) {
        const auto mixedL = (mixA * leftA[static_cast<size_t>(sample)] * balanceAL * levelA +
                             mixB * leftB[static_cast<size_t>(sample)] * balanceBL * levelB) *
                            levelMerge;
        const auto mixedR = (mixA * rightA[static_cast<size_t>(sample)] * balanceAR * levelA +
                             mixB * rightB[static_cast<size_t>(sample)] * balanceBR * levelB) *
                            levelMerge;
        left[sample] = mixedL;
        if (right != nullptr) {
            right[sample] = mixedR;
        }
    }

    BranchTask tail;
    tail.processor = this;
    tail.layout = layout;
    tail.left = left;
    tail.right = right;
    tail.numSamples = numSamples;
    tail.begin = route.mergeAfter + 1;
    tail.end = SignalChain::movableSlotCount;
    tail.sharedGain = true;
    tail.ampGain = ampGain;
    advanceCursor(layout, 0, tail.begin, tail.cursor);
    processBranch(tail);
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
    writeRouteState(_apvts.state,
                    _routeHeader.load(std::memory_order_relaxed),
                    _routePathB.load(std::memory_order_relaxed),
                    _routeBypass.load(std::memory_order_relaxed));
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
            _routeHeader.store(readRouteHeader(tree), std::memory_order_relaxed);
            _routePathB.store(readRoutePathB(tree), std::memory_order_relaxed);
            _routeBypass.store(readRouteBypass(tree), std::memory_order_relaxed);
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
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"pedalModel", 1},
        "Pedal Model",
        juce::StringArray{"Screamer", "Distortion", "Boost"},
        0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"conditionProfile", 1},
        "Condition",
        juce::StringArray{"Off", "Black", "Death", "Raw"},
        0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"conditionAmount", 1},
        "Condition Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"splitMode", 1}, "Split Mode", juce::StringArray{"Y", "A/B"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"splitRoute", 1},
        "Split Route",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"routeALevel", 1},
        "Path A Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"routeAPan", 1},
        "Path A Pan",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"routeBLevel", 1},
        "Path B Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"routeBPan", 1},
        "Path B Pan",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"routeBPolarity", 1}, "Path B Polarity", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"routeLevel", 1},
        "Merge Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f));

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
    _activeIrProfileId.clear();
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
    _activeIrProfileId.clear();
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
    if (loaded) {
        _activeAmpProfileId.clear();
        setAmpCondition(AmpCondition::kOff, 0.0f);
    }
    return loaded;
}

void ProfilerAudioProcessor::setAmpCondition(int profile, float amountPercent) {
    const auto index = static_cast<float>(juce::jlimit(AmpCondition::kOff, AmpCondition::kRaw, profile));
    const auto amount = juce::jlimit(0.0f, 100.0f, amountPercent);
    if (auto* parameter = _apvts.getParameter("conditionProfile")) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(index));
        parameter->endChangeGesture();
    }
    if (auto* parameter = _apvts.getParameter("conditionAmount")) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(amount));
        parameter->endChangeGesture();
    }
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
    auto name = title.trim();
    if (name.isEmpty()) {
        name = "Captured IR";
    }

    juce::MemoryBlock keyBlock(key.data(), key.size());
    const auto id = "capture-" + juce::Uuid().toDashedString();
    const auto stored = importEncryptedProfile(ProfileCatalog::Kind::Ir,
                                               id,
                                               name,
                                               sealed,
                                               keyBlock,
                                               0,
                                               0.0f,
                                               true,
                                               errorMessage);
    keyBlock.fillWith(0);
    return stored;
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
        _activeAmpProfileId.clear();
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

bool ProfilerAudioProcessor::isAmpModelLoaded() const noexcept {
    if (_spectraAmp.isLoaded()) {
        return true;
    }
    const juce::ScopedLock ampLock(_ampModelLock);
    return _ampLoaded || _ampFileLoaded;
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

Preset ProfilerAudioProcessor::capturePreset(const juce::String& name) const {
    Preset preset;
    preset.name = name;

    for (auto* parameter : getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            preset.parameters.set(ranged->getParameterID(),
                                  static_cast<double>(ranged->convertFrom0to1(ranged->getValue())));
        }
    }

    preset.chainLayout = _chainLayoutPacked.load(std::memory_order_relaxed);
    preset.chainBypass = _slotBypassMask.load(std::memory_order_relaxed);
    preset.routeHeader = _routeHeader.load(std::memory_order_relaxed);
    preset.routePathB = _routePathB.load(std::memory_order_relaxed);
    preset.routeBypass = _routeBypass.load(std::memory_order_relaxed);
    preset.ampProfileId = _activeAmpProfileId;
    preset.irProfileId = _activeIrProfileId;
    return preset;
}

void ProfilerAudioProcessor::applyPresetDocument(const Preset& preset) {
    for (int index = 0; index < preset.parameters.size(); ++index) {
        const auto id = preset.parameters.getName(index).toString();
        const auto value = preset.parameters.getValueAt(index);
        if (!(value.isInt() || value.isInt64() || value.isDouble())) {
            continue;
        }
        if (auto* parameter = _apvts.getParameter(id)) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(static_cast<double>(value))));
            parameter->endChangeGesture();
        }
    }

    setChainLayout(SignalChain::Layout::fromPacked(preset.chainLayout));
    setParallelRoute(SignalChain::ParallelRoute::fromPacked(preset.routeHeader, preset.routePathB));

    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        setChainSlotBypassed(slot, (preset.chainBypass & (1u << index)) != 0);
    }
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        setChainSlotBypassed(SignalChain::pathBSlotForIndex(index), (preset.routeBypass & (1u << index)) != 0);
    }
}

bool ProfilerAudioProcessor::saveCurrentPreset(const juce::String& name,
                                               juce::String* errorMessage,
                                               bool overwriteCurrent) {
    auto preset = capturePreset(name);
    if (overwriteCurrent) {
        if (const auto* current = _presetManager.getPreset(_presetManager.getCurrentIndex())) {
            preset.id = current->id;
            preset.createdAt = current->createdAt;
            preset.name = current->name;
        }
    }
    if (!_presetManager.savePreset(preset, errorMessage)) {
        return false;
    }

    _presetManager.setCurrentPresetId(preset.id);
    return true;
}

bool ProfilerAudioProcessor::applyPreset(int presetIndex, juce::String* errorMessage) {
    const auto* preset = _presetManager.getPreset(presetIndex);
    if (preset == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Preset not found.";
        }
        return false;
    }

    const auto snapshot = *preset;
    juce::String profileError;

    if (snapshot.ampProfileId.isNotEmpty()) {
        if (!loadCatalogProfile(ProfileCatalog::Kind::Amp, snapshot.ampProfileId, &profileError)) {
            unloadAmpFile();
            _activeAmpProfileId = snapshot.ampProfileId;
        }
    } else {
        unloadAmpFile();
        _activeAmpProfileId.clear();
    }

    if (snapshot.irProfileId.isNotEmpty()) {
        juce::String irError;
        if (!loadCatalogProfile(ProfileCatalog::Kind::Ir, snapshot.irProfileId, &irError)) {
            unloadIRFile();
            _activeIrProfileId = snapshot.irProfileId;
            if (profileError.isEmpty()) {
                profileError = irError;
            }
        }
    } else {
        unloadIRFile();
        _activeIrProfileId.clear();
    }

    applyPresetDocument(snapshot);
    _presetManager.setCurrentPresetId(snapshot.id);
    if (profileError.isNotEmpty() && errorMessage != nullptr) {
        *errorMessage = profileError;
    }
    return true;
}

bool ProfilerAudioProcessor::importEncryptedProfile(ProfileCatalog::Kind kind,
                                                    const juce::String& id,
                                                    const juce::String& title,
                                                    const juce::MemoryBlock& cipher,
                                                    const juce::MemoryBlock& key,
                                                    int conditionProfile,
                                                    float conditionAmount,
                                                    bool local,
                                                    juce::String* errorMessage) {
    const auto* bytes = static_cast<const char*>(cipher.getData());
    const auto plainAudio = cipher.getSize() >= 4 &&
                            (std::memcmp(bytes, "RIFF", 4) == 0 || std::memcmp(bytes, "FORM", 4) == 0 ||
                             bytes[0] == '{' || bytes[0] == '[');
    if (plainAudio) {
        if (errorMessage != nullptr) {
            *errorMessage = "Marketplace profiles must stay encrypted.";
        }
        return false;
    }

    if (!_profileCatalog.storeCipher(id, title, kind, cipher, conditionProfile, conditionAmount, local, errorMessage)) {
        return false;
    }
    if (key.getSize() == 32) {
        _profileCatalog.rememberKey(id, kind, key);
    }
    return loadCatalogProfile(kind, id, errorMessage);
}

bool ProfilerAudioProcessor::loadCatalogProfile(ProfileCatalog::Kind kind,
                                                const juce::String& id,
                                                juce::String* errorMessage) {
    juce::MemoryBlock cipher;
    if (!_profileCatalog.readCipher(id, kind, cipher)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Download this profile from your library.";
        }
        return false;
    }

    juce::MemoryBlock key;
    const auto hasKey = _profileCatalog.copyKey(id, kind, key);
    juce::MemoryBlock plain;
    bool opened = false;
    if (kind == ProfileCatalog::Kind::Amp) {
        if (hasKey) {
            opened = _spectra.openModel(static_cast<const uint8_t*>(key.getData()),
                                        key.getSize(),
                                        cipher.getData(),
                                        cipher.getSize(),
                                        plain);
        }
        if (!opened) {
            opened = _spectra.decryptModel(cipher.getData(), cipher.getSize(), plain);
        }
    } else {
        if (hasKey) {
            opened = _spectra.openIr(static_cast<const uint8_t*>(key.getData()),
                                     key.getSize(),
                                     cipher.getData(),
                                     cipher.getSize(),
                                     plain);
        }
        if (!opened) {
            opened = _spectra.decryptIr(cipher.getData(), cipher.getSize(), plain);
        }
    }

    const auto loaded = opened && (kind == ProfileCatalog::Kind::Amp
                                       ? loadAmpFromMemory(plain.getData(), plain.getSize())
                                       : loadIrFromMemory(plain.getData(), plain.getSize()));
    plain.fillWith(0);
    key.fillWith(0);
    cipher.fillWith(0);

    if (!loaded) {
        if (errorMessage != nullptr) {
            *errorMessage = kind == ProfileCatalog::Kind::Amp ? "Could not decrypt this amp profile."
                                                              : "Could not decrypt this IR profile.";
        }
        return false;
    }

    if (kind == ProfileCatalog::Kind::Amp) {
        _activeAmpProfileId = id;
        if (const auto* entry = _profileCatalog.find(id, kind)) {
            setAmpCondition(entry->conditionProfile, entry->conditionAmount);
        }
    } else {
        _activeIrProfileId = id;
    }
    return true;
}

juce::String ProfilerAudioProcessor::getActiveAmpProfileId() const {
    return _activeAmpProfileId;
}

juce::String ProfilerAudioProcessor::getActiveIrProfileId() const {
    return _activeIrProfileId;
}

void ProfilerAudioProcessor::clearActiveAmpProfile() {
    _activeAmpProfileId.clear();
}

void ProfilerAudioProcessor::clearActiveIrProfile() {
    _activeIrProfileId.clear();
}

ProfileManager& ProfilerAudioProcessor::getProfileManager() noexcept {
    return _profileManager;
}

const ProfileManager& ProfilerAudioProcessor::getProfileManager() const noexcept {
    return _profileManager;
}

PresetManager& ProfilerAudioProcessor::getPresetManager() noexcept {
    return _presetManager;
}

ProfileCatalog& ProfilerAudioProcessor::getProfileCatalog() noexcept {
    return _profileCatalog;
}
