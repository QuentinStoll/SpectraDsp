#include "Fx/FxRack.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

#include "Fx/FxCatalog.h"
#include "Fx/FxDsp.h"

namespace Fx {
namespace {
struct Seqlock {
    std::atomic<std::uint32_t> sequence{0};
    TunerSnapshot data{};

    void publish(const TunerSnapshot& snapshot) noexcept {
        sequence.fetch_add(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        data = snapshot;
        std::atomic_thread_fence(std::memory_order_release);
        sequence.fetch_add(1, std::memory_order_relaxed);
    }

    bool read(TunerSnapshot& snapshot) const noexcept {
        for (int attempt = 0; attempt < 4; ++attempt) {
            const auto first = sequence.load(std::memory_order_acquire);
            if ((first & 1u) != 0u) {
                continue;
            }
            snapshot = data;
            std::atomic_thread_fence(std::memory_order_acquire);
            if (sequence.load(std::memory_order_relaxed) == first) {
                return true;
            }
        }
        return false;
    }
};
}  // namespace

struct FxRack::Impl {
    explicit Impl(juce::AudioProcessorValueTreeState& state)
        : apvts(state) {
        for (int index = 0; index < moduleCount(); ++index) {
            const auto& module = moduleAt(index);
            for (int param = 0; param < module.paramCount; ++param) {
                values[module.params[param].id] = apvts.getRawParameterValue(module.params[param].id);
            }
        }
    }

    float get(const char* id, float fallback) const {
        const auto it = values.find(std::string(id));
        if (it == values.end() || it->second == nullptr) {
            return fallback;
        }
        return it->second->load(std::memory_order_relaxed);
    }

    bool enabled(const char* id) const noexcept { return get(id, 1.0f) > 0.5f; }

    juce::AudioProcessorValueTreeState& apvts;
    std::unordered_map<std::string, std::atomic<float>*> values;
    FxDsp::Harmonizer harmonizer;
    FxDsp::Octaver octaver;
    FxDsp::PlateReverb plate;
    FxDsp::HallReverb hall;
    FxDsp::ShimmerReverb shimmer;
    FxDsp::SpringReverb spring;
    FxDsp::GranularReverb granular;
    FxDsp::TapeDelay tape;
    FxDsp::PingPongDelay ping;
    FxDsp::DarkDelay dark;
    FxDsp::ReverseDelay reverse;
    FxDsp::DropPitch drop;
    FxDsp::EnsembleChorus ensemble;
    FxDsp::LeadChorus lead;
    FxDsp::Phaser4 phaser4;
    FxDsp::Phaser8 phaser8;
    FxDsp::Flanger flangerSubtle;
    FxDsp::Flanger flangerHard;
    FxDsp::Compressor compBlack;
    FxDsp::Compressor compBrutal;
    FxDsp::Compressor compClear;
    FxDsp::ParametricEq parametric;
    FxDsp::ToneEq tone;
    FxDsp::DynamicEq dynamic;
    FxDsp::NoiseGate gate;
    FxDsp::Tuner tuner;
    Seqlock tunerLock;
    std::array<float, 32 * FxDsp::kVoices> wetMix{};
    std::vector<float> dryLeft;
    std::vector<float> dryRight;
    std::vector<float> scratch;
    double sampleRate = 48000.0;
    double bpm = 120.0;
    int capacity = 0;
    float bypassCoef = 0.1f;

    void blend(size_t slot, float* left, float* right, int numSamples, bool enabledNow) {
        float mix = slot < wetMix.size() ? wetMix[slot] : (enabledNow ? 1.0f : 0.0f);
        const float target = enabledNow ? 1.0f : 0.0f;
        for (int sample = 0; sample < numSamples; ++sample) {
            mix += bypassCoef * (target - mix);
            left[sample] = dryLeft[(size_t)sample] + (left[sample] - dryLeft[(size_t)sample]) * mix;
            right[sample] = dryRight[(size_t)sample] + (right[sample] - dryRight[(size_t)sample]) * mix;
        }
        if (slot < wetMix.size()) {
            wetMix[slot] = mix;
        }
    }
};

FxRack::FxRack(juce::AudioProcessorValueTreeState& apvts)
    : _impl(std::make_unique<Impl>(apvts)) {}

FxRack::~FxRack() = default;

void FxRack::prepare(double sampleRate, int maxBlock) {
    _impl->sampleRate = juce::jmax(8000.0, sampleRate);
    _impl->capacity = juce::jmax(1, maxBlock);
    _impl->dryLeft.assign((size_t)_impl->capacity, 0.0f);
    _impl->dryRight.assign((size_t)_impl->capacity, 0.0f);
    _impl->scratch.assign((size_t)_impl->capacity, 0.0f);
    _impl->bypassCoef = 1.0f - std::exp(-1.0f / (0.002f * (float)_impl->sampleRate));
    _impl->harmonizer.prepare(_impl->sampleRate, maxBlock);
    _impl->octaver.prepare(_impl->sampleRate, maxBlock);
    _impl->plate.prepare(_impl->sampleRate, maxBlock);
    _impl->hall.prepare(_impl->sampleRate, maxBlock);
    _impl->shimmer.prepare(_impl->sampleRate, maxBlock);
    _impl->spring.prepare(_impl->sampleRate, maxBlock);
    _impl->granular.prepare(_impl->sampleRate, maxBlock);
    _impl->tape.prepare(_impl->sampleRate, maxBlock);
    _impl->ping.prepare(_impl->sampleRate, maxBlock);
    _impl->dark.prepare(_impl->sampleRate, maxBlock);
    _impl->reverse.prepare(_impl->sampleRate, maxBlock);
    _impl->drop.prepare(_impl->sampleRate, maxBlock);
    _impl->ensemble.prepare(_impl->sampleRate, maxBlock);
    _impl->lead.prepare(_impl->sampleRate, maxBlock);
    _impl->phaser4.prepare(_impl->sampleRate, maxBlock);
    _impl->phaser8.prepare(_impl->sampleRate, maxBlock);
    _impl->flangerSubtle.prepare(_impl->sampleRate, maxBlock);
    _impl->flangerHard.prepare(_impl->sampleRate, maxBlock);
    _impl->compBlack.prepare(_impl->sampleRate, maxBlock);
    _impl->compBrutal.prepare(_impl->sampleRate, maxBlock);
    _impl->compClear.prepare(_impl->sampleRate, maxBlock);
    _impl->parametric.prepare(_impl->sampleRate, maxBlock);
    _impl->tone.prepare(_impl->sampleRate, maxBlock);
    _impl->dynamic.prepare(_impl->sampleRate, maxBlock);
    _impl->gate.prepare(_impl->sampleRate, maxBlock);
    _impl->tuner.prepare(_impl->sampleRate, maxBlock);
    reset();
}

void FxRack::reset() {
    _impl->wetMix.fill(1.0f);
}

void FxRack::setTempo(double bpm) noexcept {
    if (bpm > 1.0) {
        _impl->bpm = bpm;
    }
}

int FxRack::latencySamplesFor(SignalChain::Stage stage) const {
    if (!_impl->enabled(moduleFor(stage) != nullptr ? moduleFor(stage)->bypassId() : "")) {
        return 0;
    }
    switch (stage) {
        case SignalChain::Stage::PitchHarmonizer:
            return _impl->harmonizer.latencySamples(_impl->get("harmLatency", 4.0f));
        case SignalChain::Stage::PitchDrop:
            return _impl->drop.latencySamples(_impl->get("dropLatency", 20.0f));
        case SignalChain::Stage::NoiseGate:
            return _impl->gate.latencySamples(_impl->get("ngtLook", 8.0f));
        default:
            return 0;
    }
}

double FxRack::tailSecondsFor(SignalChain::Stage stage) const {
    if (moduleFor(stage) == nullptr || !_impl->enabled(moduleFor(stage)->bypassId())) {
        return 0.0;
    }
    switch (stage) {
        case SignalChain::Stage::ReverbPlate:
            return _impl->get("plateDecay", 1.4f);
        case SignalChain::Stage::ReverbHall:
            return _impl->get("hallDecay", 3.6f);
        case SignalChain::Stage::ReverbShimmer:
            return _impl->get("shimDecay", 4.5f);
        case SignalChain::Stage::ReverbSpring:
            return _impl->get("springDecay", 1.1f);
        case SignalChain::Stage::ReverbGranular:
            return 8.0;
        case SignalChain::Stage::DelayTape:
        case SignalChain::Stage::DelayPingPong:
            return 1.5;
        case SignalChain::Stage::DelayDark:
            return 4.0;
        case SignalChain::Stage::DelayReverse:
            return 2.5;
        default:
            return 0.0;
    }
}

bool FxRack::readTuner(TunerSnapshot& snapshot) const {
    return _impl->tunerLock.read(snapshot);
}

void FxRack::process(SignalChain::Stage stage,
                     int instance,
                     float* left,
                     float* right,
                     int numSamples,
                     bool enabled,
                     const float* monitorLeft,
                     const float* monitorRight) {
    if (left == nullptr || numSamples <= 0 || moduleFor(stage) == nullptr) {
        return;
    }
    if (instance < 0 || instance >= FxDsp::kVoices || _impl->capacity <= 0) {
        return;
    }

    const bool mono = right == nullptr || right == left;
    int offset = 0;
    while (offset < numSamples) {
        const int count = juce::jmin(_impl->capacity, numSamples - offset);
        float* sliceL = left + offset;
        float* sliceR = mono ? _impl->scratch.data() : right + offset;
        if (mono) {
            std::copy(sliceL, sliceL + count, sliceR);
        }
        const bool active = enabled && _impl->enabled(moduleFor(stage)->bypassId());
        const auto mixIndex = (size_t)stage * (size_t)FxDsp::kVoices + (size_t)instance;
        const float mix = mixIndex < _impl->wetMix.size() ? _impl->wetMix[mixIndex] : (active ? 1.0f : 0.0f);
        if (!active && mix <= 0.0008f) {
            if (mono) {
                std::copy(sliceR, sliceR + count, sliceL);
            }
            offset += count;
            continue;
        }

        std::copy(sliceL, sliceL + count, _impl->dryLeft.begin());
        std::copy(sliceR, sliceR + count, _impl->dryRight.begin());
        auto& fx = *_impl;
        switch (stage) {
            case SignalChain::Stage::PitchHarmonizer:
                fx.harmonizer.process(instance, sliceL, sliceR, count,
                                      {(int)fx.get("harmMode", 1.0f), fx.get("harmInterval", 0.0f), fx.get("harmMix", 50.0f),
                                       fx.get("harmLatency", 4.0f), fx.enabled("harmFormant")});
                break;
            case SignalChain::Stage::PitchDrop:
                fx.drop.process(instance, sliceL, sliceR, count,
                                {fx.get("dropSemi", -2.0f), fx.get("dropMix", 100.0f), fx.get("dropLatency", 20.0f)});
                break;
            case SignalChain::Stage::PitchOctaver:
                fx.octaver.process(instance, sliceL, sliceR, count,
                                   {(int)fx.get("octMode", 2.0f), fx.get("octMix1", 55.0f), fx.get("octMix2", 30.0f),
                                    fx.get("octHp", 90.0f), fx.get("octTrigger", 35.0f)});
                break;
            case SignalChain::Stage::ReverbPlate:
                fx.plate.process(instance, sliceL, sliceR, count,
                                 {fx.get("plateDecay", 1.4f), fx.get("platePre", 8.0f), fx.get("plateMix", 22.0f),
                                  fx.get("plateDamp", 35.0f), fx.get("plateTone", 62.0f)});
                break;
            case SignalChain::Stage::ReverbHall:
                fx.hall.process(instance, sliceL, sliceR, count,
                                {fx.get("hallDecay", 3.6f), fx.get("hallPre", 18.0f), fx.get("hallWidth", 70.0f),
                                 fx.get("hallSize", 55.0f), fx.get("hallTone", 48.0f), fx.get("hallMix", 24.0f)});
                break;
            case SignalChain::Stage::ReverbShimmer:
                fx.shimmer.process(instance, sliceL, sliceR, count,
                                   {fx.get("shimDecay", 4.5f), fx.get("shimMix", 28.0f), (int)fx.get("shimPitch", 0.0f),
                                    fx.get("shimAmount", 45.0f), fx.get("shimDamp", 40.0f)});
                break;
            case SignalChain::Stage::ReverbSpring:
                fx.spring.process(instance, sliceL, sliceR, count,
                                  {fx.get("springDecay", 1.1f), fx.get("springMix", 28.0f), fx.get("springBoing", 45.0f),
                                   fx.get("springDamp", 55.0f)});
                break;
            case SignalChain::Stage::ReverbGranular:
                fx.granular.process(instance, sliceL, sliceR, count,
                                    {fx.get("grainSize", 140.0f), fx.get("grainDensity", 12.0f), fx.get("grainFeedback", 62.0f),
                                     fx.get("grainDiffuse", 70.0f), fx.get("grainTone", 40.0f), fx.get("grainMix", 30.0f)});
                break;
            case SignalChain::Stage::DelayTape:
                fx.tape.process(instance, sliceL, sliceR, count,
                                {fx.get("tapeTime", 120.0f), fx.get("tapeFb", 35.0f), fx.get("tapeMix", 28.0f),
                                 fx.get("tapeWow", 30.0f), fx.get("tapeSat", 28.0f), fx.get("tapeTone", 45.0f)});
                break;
            case SignalChain::Stage::DelayPingPong:
                fx.ping.process(instance, sliceL, sliceR, count,
                                {fx.get("pingTime", 375.0f), fx.enabled("pingSync"), (int)fx.get("pingDiv", 3.0f),
                                 fx.get("pingFb", 35.0f), fx.get("pingMix", 28.0f), fx.get("pingDepth", 100.0f),
                                 fx.get("pingHp", 80.0f), fx.get("pingLp", 9000.0f), fx.bpm});
                break;
            case SignalChain::Stage::DelayDark:
                fx.dark.process(instance, sliceL, sliceR, count,
                                {fx.get("darkTime", 480.0f), fx.get("darkFb", 72.0f), fx.get("darkAmount", 65.0f),
                                 fx.get("darkMix", 30.0f), fx.get("darkTone", 35.0f)});
                break;
            case SignalChain::Stage::DelayReverse:
                fx.reverse.process(instance, sliceL, sliceR, count,
                                   {fx.get("revdTime", 600.0f), (int)fx.get("revdMode", 1.0f), fx.get("revdFb", 45.0f),
                                    fx.get("revdMix", 40.0f), fx.get("revdTone", 50.0f)});
                break;
            case SignalChain::Stage::ChorusEnsemble:
                fx.ensemble.process(instance, sliceL, sliceR, count,
                                    {fx.get("ensRate", 0.6f), fx.get("ensDepth", 35.0f), fx.get("ensMix", 40.0f),
                                     fx.get("ensWidth", 75.0f)});
                break;
            case SignalChain::Stage::ChorusLead:
                fx.lead.process(instance, sliceL, sliceR, count,
                                {fx.get("cmodRate", 2.4f), fx.get("cmodDepth", 55.0f), fx.get("cmodMix", 45.0f),
                                 fx.get("cmodVibe", 30.0f)});
                break;
            case SignalChain::Stage::Phaser4:
                fx.phaser4.process(instance, sliceL, sliceR, count,
                                   {fx.get("ph4Rate", 0.4f), fx.get("ph4Depth", 60.0f), fx.get("ph4Mix", 50.0f),
                                    fx.get("ph4Center", 900.0f)});
                break;
            case SignalChain::Stage::Phaser8:
                fx.phaser8.process(instance, sliceL, sliceR, count,
                                   {fx.get("ph8Rate", 1.2f), fx.get("ph8Depth", 70.0f), fx.get("ph8Mix", 55.0f),
                                    fx.get("ph8Q", 2.8f), fx.get("ph8Sweep", 65.0f)});
                break;
            case SignalChain::Stage::FlangerSubtle:
                fx.flangerSubtle.process(instance, sliceL, sliceR, count,
                                         {fx.get("flsRate", 0.35f), fx.get("flsDepth", 45.0f), fx.get("flsMix", 35.0f),
                                          fx.get("flsFb", 18.0f), 0.0f, false});
                break;
            case SignalChain::Stage::FlangerHard:
                fx.flangerHard.process(instance, sliceL, sliceR, count,
                                       {fx.get("flhRate", 2.8f), fx.get("flhDepth", 70.0f), fx.get("flhMix", 45.0f),
                                        fx.get("flhFb", 62.0f), fx.get("flhRing", 40.0f), true});
                break;
            case SignalChain::Stage::CompBlack:
                fx.compBlack.process(instance, sliceL, sliceR, count,
                                     {fx.get("cbmThr", -18.0f), fx.get("cbmAtk", 3.0f), fx.get("cbmRel", 90.0f),
                                      fx.get("cbmRatio", 6.0f), fx.get("cbmKnee", 1.0f), fx.get("cbmMake", 0.0f),
                                      fx.get("cbmSc", 100.0f), true, true});
                break;
            case SignalChain::Stage::CompBrutal:
                fx.compBrutal.process(instance, sliceL, sliceR, count,
                                      {fx.get("cbrThr", -16.0f), fx.get("cbrAtk", 22.0f), fx.get("cbrRel", 45.0f),
                                       fx.get("cbrRatio", 12.0f), 0.0f, fx.get("cbrMake", 0.0f), 100.0f, false, true});
                break;
            case SignalChain::Stage::CompClear:
                fx.compClear.process(instance, sliceL, sliceR, count,
                                     {fx.get("cgnThr", -20.0f), fx.get("cgnAtk", 15.0f), fx.get("cgnRel", 160.0f),
                                      fx.get("cgnRatio", 2.5f), fx.get("cgnKnee", 10.0f), fx.get("cgnMake", 0.0f),
                                      100.0f, false, false});
                break;
            case SignalChain::Stage::EqParametric: {
                FxDsp::ParametricEqSettings eq;
                const char* freq[4] = {"eqp1F", "eqp2F", "eqp3F", "eqp4F"};
                const char* gain[4] = {"eqp1G", "eqp2G", "eqp3G", "eqp4G"};
                const char* q[4] = {"eqp1Q", "eqp2Q", "eqp3Q", "eqp4Q"};
                for (int band = 0; band < 4; ++band) {
                    eq.frequency[band] = fx.get(freq[band], eq.frequency[band]);
                    eq.gain[band] = fx.get(gain[band], 0.0f);
                    eq.q[band] = fx.get(q[band], eq.q[band]);
                }
                fx.parametric.process(instance, sliceL, sliceR, count, eq);
                break;
            }
            case SignalChain::Stage::EqTone:
                fx.tone.process(instance, sliceL, sliceR, count,
                                {fx.get("eqtLow", 0.0f), fx.get("eqtMid", 0.0f), fx.get("eqtHigh", 0.0f)});
                break;
            case SignalChain::Stage::EqDynamic: {
                FxDsp::DynamicEqSettings eq;
                const char* freq[4] = {"dyn1F", "dyn2F", "dyn3F", "dyn4F"};
                const char* q[4] = {"dyn1Q", "dyn2Q", "dyn3Q", "dyn4Q"};
                const char* thr[4] = {"dyn1Thr", "dyn2Thr", "dyn3Thr", "dyn4Thr"};
                const char* ratio[4] = {"dyn1Ratio", "dyn2Ratio", "dyn3Ratio", "dyn4Ratio"};
                for (int band = 0; band < 4; ++band) {
                    eq.frequency[band] = fx.get(freq[band], eq.frequency[band]);
                    eq.q[band] = fx.get(q[band], eq.q[band]);
                    eq.threshold[band] = fx.get(thr[band], eq.threshold[band]);
                    eq.ratio[band] = fx.get(ratio[band], eq.ratio[band]);
                }
                fx.dynamic.process(instance, sliceL, sliceR, count, eq);
                break;
            }
            case SignalChain::Stage::NoiseGate:
                fx.gate.process(instance, sliceL, sliceR, count,
                                {fx.get("ngtThr", -45.0f), fx.get("ngtAtk", 2.0f), fx.get("ngtRel", 80.0f),
                                 fx.get("ngtRange", -60.0f), fx.get("ngtHold", 25.0f), fx.get("ngtLook", 8.0f),
                                 fx.enabled("ngtScOn"), fx.get("ngtSc", 100.0f)});
                break;
            case SignalChain::Stage::Tuner: {
                TunerSnapshot snapshot;
                const float* tuneLeft = monitorLeft != nullptr ? monitorLeft + offset : sliceL;
                const float* tuneRight = monitorRight != nullptr ? monitorRight + offset : sliceR;
                fx.tuner.process(instance, tuneLeft, tuneRight, count, {(int)fx.get("tunMode", 0.0f), true}, snapshot);
                fx.tunerLock.publish(snapshot);
                break;
            }
            default:
                break;
        }
        if (stage != SignalChain::Stage::Tuner) {
            fx.blend(mixIndex, sliceL, sliceR, count, active);
        }
        if (mono) {
            for (int sample = 0; sample < count; ++sample) {
                sliceL[sample] = 0.5f * (sliceL[sample] + sliceR[sample]);
            }
        }
        offset += count;
    }
}
}  // namespace Fx
