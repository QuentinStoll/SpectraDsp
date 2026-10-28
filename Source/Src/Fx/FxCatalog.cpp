#include "Fx/FxCatalog.h"

namespace Fx {
namespace {
constexpr ParamSpec flag(const char* id, const char* label, bool enabled) {
    return ParamSpec{id, label, ParamType::Bool, 0.0f, 1.0f, 1.0f, enabled ? 1.0f : 0.0f, 1.0f, "", nullptr};
}

constexpr ParamSpec on(const char* id) {
    return flag(id, "On", true);
}

constexpr ParamSpec choice(const char* id, const char* label, const char* choices, float defaultIndex = 0.0f) {
    return ParamSpec{id, label, ParamType::Choice, 0.0f, 1.0f, 1.0f, defaultIndex, 1.0f, "", choices};
}

constexpr ParamSpec flt(const char* id,
                        const char* label,
                        float minimum,
                        float maximum,
                        float step,
                        float defaultValue,
                        const char* suffix,
                        float skew = 1.0f) {
    return ParamSpec{id, label, ParamType::Float, minimum, maximum, step, defaultValue, skew, suffix, nullptr};
}

constexpr ParamSpec kHarm[] = {
    on("harmOn"),
    choice("harmMode", "Mode", "Octave Down|Octave Up|Fifth|Major Third|Detune", 1.0f),
    flt("harmInterval", "Interval", -12.0f, 12.0f, 0.01f, 0.0f, "st"),
    flt("harmMix", "Mix", 0.0f, 100.0f, 0.1f, 50.0f, "%"),
    flt("harmLatency", "Latency", 2.0f, 40.0f, 0.1f, 22.0f, "ms"),
    flag("harmFormant", "Formant", true)};

constexpr ParamSpec kDrop[] = {
    on("dropOn"),
    flt("dropSemi", "Semitones", -12.0f, 0.0f, 1.0f, -2.0f, "st"),
    flt("dropMix", "Mix", 0.0f, 100.0f, 0.1f, 100.0f, "%"),
    flt("dropLatency", "Latency", 8.0f, 20.0f, 0.1f, 20.0f, "ms")};

constexpr ParamSpec kOct[] = {
    on("octOn"),
    choice("octMode", "Mode", "Sub 1|Sub 2|Sub 1+2", 2.0f),
    flt("octMix1", "Oct 1", 0.0f, 100.0f, 0.1f, 55.0f, "%"),
    flt("octMix2", "Oct 2", 0.0f, 100.0f, 0.1f, 30.0f, "%"),
    flt("octHp", "Sub HP", 40.0f, 400.0f, 1.0f, 90.0f, "Hz", 0.5f),
    flt("octTrigger", "Trigger", 0.0f, 100.0f, 0.1f, 35.0f, "%")};

constexpr ParamSpec kPlate[] = {
    on("plateOn"),
    flt("plateDecay", "Decay", 0.8f, 2.5f, 0.01f, 1.4f, "s"),
    flt("platePre", "Pre-delay", 0.0f, 40.0f, 0.1f, 8.0f, "ms"),
    flt("plateMix", "Mix", 0.0f, 100.0f, 0.1f, 22.0f, "%"),
    flt("plateDamp", "Damping", 0.0f, 100.0f, 0.1f, 35.0f, "%"),
    flt("plateTone", "Tone", 0.0f, 100.0f, 0.1f, 62.0f, "%")};

constexpr ParamSpec kHall[] = {
    on("hallOn"),
    flt("hallDecay", "Decay", 2.0f, 8.0f, 0.01f, 3.6f, "s"),
    flt("hallPre", "Pre-delay", 0.0f, 80.0f, 0.1f, 18.0f, "ms"),
    flt("hallWidth", "Width", 0.0f, 100.0f, 0.1f, 70.0f, "%"),
    flt("hallSize", "Size", 0.0f, 100.0f, 0.1f, 55.0f, "%"),
    flt("hallTone", "Tone", 0.0f, 100.0f, 0.1f, 48.0f, "%"),
    flt("hallMix", "Mix", 0.0f, 100.0f, 0.1f, 24.0f, "%")};

constexpr ParamSpec kShim[] = {
    on("shimOn"),
    flt("shimDecay", "Decay", 2.0f, 8.0f, 0.01f, 4.5f, "s"),
    flt("shimMix", "Mix", 0.0f, 100.0f, 0.1f, 28.0f, "%"),
    choice("shimPitch", "Pitch", "Fifth|Octave", 0.0f),
    flt("shimAmount", "Shimmer", 0.0f, 100.0f, 0.1f, 45.0f, "%"),
    flt("shimDamp", "Damping", 0.0f, 100.0f, 0.1f, 40.0f, "%")};

constexpr ParamSpec kSpring[] = {
    on("springOn"),
    flt("springDecay", "Decay", 0.5f, 2.0f, 0.01f, 1.1f, "s"),
    flt("springMix", "Mix", 0.0f, 100.0f, 0.1f, 28.0f, "%"),
    flt("springBoing", "Boing", 0.0f, 100.0f, 0.1f, 45.0f, "%"),
    flt("springDamp", "Damping", 0.0f, 100.0f, 0.1f, 55.0f, "%")};

constexpr ParamSpec kGrain[] = {
    on("grainOn"),
    flt("grainSize", "Grain", 50.0f, 300.0f, 1.0f, 140.0f, "ms"),
    flt("grainDensity", "Density", 1.0f, 40.0f, 0.1f, 12.0f, "/s"),
    flt("grainFeedback", "Feedback", 0.0f, 95.0f, 0.1f, 62.0f, "%"),
    flt("grainDiffuse", "Diffuse", 0.0f, 100.0f, 0.1f, 70.0f, "%"),
    flt("grainTone", "Tone", 0.0f, 100.0f, 0.1f, 40.0f, "%"),
    flt("grainMix", "Mix", 0.0f, 100.0f, 0.1f, 30.0f, "%")};

constexpr ParamSpec kTape[] = {
    on("tapeOn"),
    flt("tapeTime", "Time", 50.0f, 300.0f, 0.1f, 120.0f, "ms"),
    flt("tapeFb", "Feedback", 0.0f, 85.0f, 0.1f, 35.0f, "%"),
    flt("tapeMix", "Mix", 0.0f, 100.0f, 0.1f, 28.0f, "%"),
    flt("tapeWow", "Wow", 0.0f, 100.0f, 0.1f, 30.0f, "%"),
    flt("tapeSat", "Saturation", 0.0f, 100.0f, 0.1f, 28.0f, "%"),
    flt("tapeTone", "Tone", 0.0f, 100.0f, 0.1f, 45.0f, "%")};

constexpr ParamSpec kPing[] = {
    on("pingOn"),
    flt("pingTime", "Time", 50.0f, 1000.0f, 0.1f, 375.0f, "ms"),
    flag("pingSync", "Sync", false),
    choice("pingDiv", "Division", "1/16|1/8|1/8.|1/4|1/4.|1/2|1/2.|1 bar", 3.0f),
    flt("pingFb", "Feedback", 0.0f, 90.0f, 0.1f, 35.0f, "%"),
    flt("pingMix", "Mix", 0.0f, 100.0f, 0.1f, 28.0f, "%"),
    flt("pingDepth", "Ping-Pong", 0.0f, 100.0f, 0.1f, 100.0f, "%"),
    flt("pingHp", "High Pass", 20.0f, 2000.0f, 1.0f, 80.0f, "Hz", 0.4f),
    flt("pingLp", "Low Pass", 500.0f, 16000.0f, 1.0f, 9000.0f, "Hz", 0.4f)};

constexpr ParamSpec kDark[] = {
    on("darkOn"),
    flt("darkTime", "Time", 300.0f, 1200.0f, 0.1f, 480.0f, "ms"),
    flt("darkFb", "Feedback", 30.0f, 95.0f, 0.1f, 72.0f, "%"),
    flt("darkAmount", "Darkness", 0.0f, 100.0f, 0.1f, 65.0f, "%"),
    flt("darkMix", "Mix", 0.0f, 100.0f, 0.1f, 30.0f, "%"),
    flt("darkTone", "Tone", 0.0f, 100.0f, 0.1f, 35.0f, "%")};

constexpr ParamSpec kRevd[] = {
    on("revdOn"),
    flt("revdTime", "Time", 100.0f, 2000.0f, 0.1f, 600.0f, "ms"),
    choice("revdMode", "Mode", "Normal|Reverse|Feedback", 1.0f),
    flt("revdFb", "Feedback", 0.0f, 100.0f, 0.1f, 45.0f, "%"),
    flt("revdMix", "Mix", 0.0f, 100.0f, 0.1f, 40.0f, "%"),
    flt("revdTone", "Tone", 0.0f, 100.0f, 0.1f, 50.0f, "%")};

constexpr ParamSpec kEns[] = {
    on("ensOn"),
    flt("ensRate", "Rate", 0.1f, 5.0f, 0.01f, 0.6f, "Hz"),
    flt("ensDepth", "Depth", 0.0f, 100.0f, 0.1f, 35.0f, "%"),
    flt("ensMix", "Mix", 0.0f, 100.0f, 0.1f, 40.0f, "%"),
    flt("ensWidth", "Width", 0.0f, 100.0f, 0.1f, 75.0f, "%")};

constexpr ParamSpec kCmod[] = {
    on("cmodOn"),
    flt("cmodRate", "Rate", 0.5f, 10.0f, 0.01f, 2.4f, "Hz"),
    flt("cmodDepth", "Depth", 0.0f, 100.0f, 0.1f, 55.0f, "%"),
    flt("cmodMix", "Mix", 0.0f, 100.0f, 0.1f, 45.0f, "%"),
    flt("cmodVibe", "Vibe", 0.0f, 100.0f, 0.1f, 30.0f, "%")};

constexpr ParamSpec kPh4[] = {
    on("ph4On"),
    flt("ph4Rate", "Rate", 0.1f, 5.0f, 0.01f, 0.4f, "Hz"),
    flt("ph4Depth", "Depth", 0.0f, 100.0f, 0.1f, 60.0f, "%"),
    flt("ph4Mix", "Mix", 0.0f, 100.0f, 0.1f, 50.0f, "%"),
    flt("ph4Center", "Center", 200.0f, 4000.0f, 1.0f, 900.0f, "Hz", 0.4f)};

constexpr ParamSpec kPh8[] = {
    on("ph8On"),
    flt("ph8Rate", "Rate", 0.1f, 10.0f, 0.01f, 1.2f, "Hz"),
    flt("ph8Depth", "Depth", 0.0f, 100.0f, 0.1f, 70.0f, "%"),
    flt("ph8Mix", "Mix", 0.0f, 100.0f, 0.1f, 55.0f, "%"),
    flt("ph8Q", "Q", 2.0f, 4.0f, 0.01f, 2.8f, ""),
    flt("ph8Sweep", "Sweep", 0.0f, 100.0f, 0.1f, 65.0f, "%")};

constexpr ParamSpec kFls[] = {
    on("flsOn"),
    flt("flsRate", "Rate", 0.1f, 5.0f, 0.01f, 0.35f, "Hz"),
    flt("flsDepth", "Depth", 0.0f, 100.0f, 0.1f, 45.0f, "%"),
    flt("flsMix", "Mix", 0.0f, 100.0f, 0.1f, 35.0f, "%"),
    flt("flsFb", "Feedback", 0.0f, 50.0f, 0.1f, 18.0f, "%")};

constexpr ParamSpec kFlh[] = {
    on("flhOn"),
    flt("flhRate", "Rate", 0.5f, 10.0f, 0.01f, 2.8f, "Hz"),
    flt("flhDepth", "Depth", 0.0f, 100.0f, 0.1f, 70.0f, "%"),
    flt("flhMix", "Mix", 0.0f, 100.0f, 0.1f, 45.0f, "%"),
    flt("flhFb", "Feedback", 0.0f, 90.0f, 0.1f, 62.0f, "%"),
    flt("flhRing", "Ring", 0.0f, 100.0f, 0.1f, 40.0f, "%")};

constexpr ParamSpec kCbm[] = {
    on("cbmOn"),
    flt("cbmThr", "Threshold", -40.0f, 0.0f, 0.1f, -18.0f, "dB"),
    flt("cbmAtk", "Attack", 1.0f, 15.0f, 0.1f, 3.0f, "ms"),
    flt("cbmRel", "Release", 50.0f, 200.0f, 0.1f, 90.0f, "ms"),
    flt("cbmRatio", "Ratio", 4.0f, 8.0f, 0.01f, 6.0f, ":1"),
    flt("cbmKnee", "Knee", 0.0f, 5.0f, 0.1f, 1.0f, "dB"),
    flt("cbmMake", "Makeup", 0.0f, 24.0f, 0.1f, 0.0f, "dB"),
    flt("cbmSc", "SC HPF", 20.0f, 400.0f, 1.0f, 100.0f, "Hz", 0.5f)};

constexpr ParamSpec kCbr[] = {
    on("cbrOn"),
    flt("cbrThr", "Threshold", -40.0f, 0.0f, 0.1f, -16.0f, "dB"),
    flt("cbrAtk", "Attack", 10.0f, 50.0f, 0.1f, 22.0f, "ms"),
    flt("cbrRel", "Release", 30.0f, 100.0f, 0.1f, 45.0f, "ms"),
    flt("cbrRatio", "Ratio", 6.0f, 20.0f, 0.01f, 12.0f, ":1"),
    flt("cbrMake", "Makeup", 0.0f, 24.0f, 0.1f, 0.0f, "dB")};

constexpr ParamSpec kCgn[] = {
    on("cgnOn"),
    flt("cgnThr", "Threshold", -40.0f, 0.0f, 0.1f, -20.0f, "dB"),
    flt("cgnAtk", "Attack", 5.0f, 50.0f, 0.1f, 15.0f, "ms"),
    flt("cgnRel", "Release", 50.0f, 500.0f, 0.1f, 160.0f, "ms"),
    flt("cgnRatio", "Ratio", 2.0f, 4.0f, 0.01f, 2.5f, ":1"),
    flt("cgnKnee", "Knee", 5.0f, 20.0f, 0.1f, 10.0f, "dB"),
    flt("cgnMake", "Makeup", 0.0f, 12.0f, 0.1f, 0.0f, "dB")};

constexpr ParamSpec kEqp[] = {
    on("eqpOn"),
    flt("eqp1F", "Low Freq", 20.0f, 20000.0f, 0.01f, 120.0f, "Hz", 0.25f),
    flt("eqp1G", "Low Gain", -24.0f, 24.0f, 0.1f, 0.0f, "dB"),
    flt("eqp1Q", "Low Q", 0.5f, 10.0f, 0.01f, 0.7f, ""),
    flt("eqp2F", "Low Mid F", 20.0f, 20000.0f, 0.01f, 400.0f, "Hz", 0.25f),
    flt("eqp2G", "Low Mid", -24.0f, 24.0f, 0.1f, 0.0f, "dB"),
    flt("eqp2Q", "Low Mid Q", 0.5f, 10.0f, 0.01f, 1.0f, ""),
    flt("eqp3F", "High Mid F", 20.0f, 20000.0f, 0.01f, 1800.0f, "Hz", 0.25f),
    flt("eqp3G", "High Mid", -24.0f, 24.0f, 0.1f, 0.0f, "dB"),
    flt("eqp3Q", "High Mid Q", 0.5f, 10.0f, 0.01f, 1.0f, ""),
    flt("eqp4F", "High Freq", 20.0f, 20000.0f, 0.01f, 6500.0f, "Hz", 0.25f),
    flt("eqp4G", "High Gain", -24.0f, 24.0f, 0.1f, 0.0f, "dB"),
    flt("eqp4Q", "High Q", 0.5f, 10.0f, 0.01f, 0.7f, "")};

constexpr ParamSpec kEqt[] = {
    on("eqtOn"),
    flt("eqtLow", "Bass", -20.0f, 20.0f, 0.1f, 0.0f, "dB"),
    flt("eqtMid", "Mid", -20.0f, 20.0f, 0.1f, 0.0f, "dB"),
    flt("eqtHigh", "Treble", -20.0f, 20.0f, 0.1f, 0.0f, "dB")};

constexpr ParamSpec kDyn[] = {
    on("dynOn"),
    flt("dyn1F", "Low Freq", 40.0f, 800.0f, 0.1f, 120.0f, "Hz", 0.5f),
    flt("dyn1Q", "Low Q", 0.5f, 10.0f, 0.01f, 0.7f, ""),
    flt("dyn1Thr", "Low Thr", -40.0f, 0.0f, 0.1f, -18.0f, "dB"),
    flt("dyn1Ratio", "Low Ratio", 1.0f, 10.0f, 0.01f, 2.0f, ":1"),
    flt("dyn2F", "L-Mid Freq", 80.0f, 4000.0f, 0.1f, 400.0f, "Hz", 0.4f),
    flt("dyn2Q", "L-Mid Q", 0.5f, 10.0f, 0.01f, 1.0f, ""),
    flt("dyn2Thr", "L-Mid Thr", -40.0f, 0.0f, 0.1f, -18.0f, "dB"),
    flt("dyn2Ratio", "L-Mid Ratio", 1.0f, 10.0f, 0.01f, 2.0f, ":1"),
    flt("dyn3F", "H-Mid Freq", 200.0f, 10000.0f, 0.1f, 2000.0f, "Hz", 0.4f),
    flt("dyn3Q", "H-Mid Q", 0.5f, 10.0f, 0.01f, 1.0f, ""),
    flt("dyn3Thr", "H-Mid Thr", -40.0f, 0.0f, 0.1f, -18.0f, "dB"),
    flt("dyn3Ratio", "H-Mid Ratio", 1.0f, 10.0f, 0.01f, 2.0f, ":1"),
    flt("dyn4F", "High Freq", 800.0f, 16000.0f, 0.1f, 6000.0f, "Hz", 0.4f),
    flt("dyn4Q", "High Q", 0.5f, 10.0f, 0.01f, 0.7f, ""),
    flt("dyn4Thr", "High Thr", -40.0f, 0.0f, 0.1f, -18.0f, "dB"),
    flt("dyn4Ratio", "High Ratio", 1.0f, 10.0f, 0.01f, 2.0f, ":1")};

constexpr ParamSpec kGate[] = {
    on("ngtOn"),
    flt("ngtThr", "Threshold", -80.0f, 0.0f, 0.1f, -45.0f, "dB"),
    flt("ngtAtk", "Attack", 1.0f, 20.0f, 0.1f, 2.0f, "ms"),
    flt("ngtRel", "Release", 20.0f, 500.0f, 0.1f, 80.0f, "ms"),
    flt("ngtRange", "Range", -60.0f, 0.0f, 0.1f, -60.0f, "dB"),
    flt("ngtHold", "Hold", 0.0f, 300.0f, 0.1f, 25.0f, "ms"),
    flt("ngtLook", "Lookahead", 5.0f, 20.0f, 0.1f, 8.0f, "ms"),
    flag("ngtScOn", "SC Filter", true),
    flt("ngtSc", "SC HPF", 40.0f, 300.0f, 1.0f, 100.0f, "Hz", 0.5f)};

constexpr ParamSpec kTun[] = {
    on("tunOn"),
    choice("tunMode", "Mode", "Mono|Poly", 0.0f)};

constexpr ModuleSpec kModules[] = {
    {SignalChain::Stage::PitchHarmonizer, "Harmonizer", "HARM",
     "TD-PSOLA dual-window pitch shifter with spectral-tilt formant lock", kHarm,
     (int)(sizeof(kHarm) / sizeof(kHarm[0]))},
    {SignalChain::Stage::PitchDrop, "Drop", "DROP",
     "Two-head time-domain transpose, every note moves together", kDrop, (int)(sizeof(kDrop) / sizeof(kDrop[0]))},
    {SignalChain::Stage::PitchOctaver, "Octaver", "OCT",
     "Fundamental-locked octave divider with onset trigger", kOct, (int)(sizeof(kOct) / sizeof(kOct[0]))},
    {SignalChain::Stage::ReverbPlate, "Plate", "PLATE",
     "Short 4-line Hadamard FDN plate", kPlate, (int)(sizeof(kPlate) / sizeof(kPlate[0]))},
    {SignalChain::Stage::ReverbHall, "Hall", "HALL",
     "Moorer hall: 4 lowpass-feedback combs + 2 allpass diffusers", kHall,
     (int)(sizeof(kHall) / sizeof(kHall[0]))},
    {SignalChain::Stage::ReverbShimmer, "Shimmer", "SHIM",
     "Hall tank with feedback pitch shift on the tail only", kShim, (int)(sizeof(kShim) / sizeof(kShim[0]))},
    {SignalChain::Stage::ReverbSpring, "Spring", "SPRNG",
     "Dispersive allpass spring with resonant boing", kSpring, (int)(sizeof(kSpring) / sizeof(kSpring[0]))},
    {SignalChain::Stage::ReverbGranular, "Granular", "GRAIN",
     "Hann-grain diffusion cloud, not a reflection reverb", kGrain, (int)(sizeof(kGrain) / sizeof(kGrain[0]))},
    {SignalChain::Stage::DelayTape, "Tape", "TAPE",
     "Short tape delay: wow/flutter, tanh saturation, loop degradation", kTape,
     (int)(sizeof(kTape) / sizeof(kTape[0]))},
    {SignalChain::Stage::DelayPingPong, "Ping-Pong", "PING",
     "Clean tempo-synced stereo ping-pong delay", kPing, (int)(sizeof(kPing) / sizeof(kPing[0]))},
    {SignalChain::Stage::DelayDark, "Dark Delay", "DARK",
     "Long delay with progressive lowpass darkening", kDark, (int)(sizeof(kDark) / sizeof(kDark[0]))},
    {SignalChain::Stage::DelayReverse, "Reverse", "REV",
     "Reverse buffer delay and infinite feedback freeze", kRevd, (int)(sizeof(kRevd) / sizeof(kRevd[0]))},
    {SignalChain::Stage::ChorusEnsemble, "Ensemble", "ENS",
     "3-voice sine chorus, 10-30 ms", kEns, (int)(sizeof(kEns) / sizeof(kEns[0]))},
    {SignalChain::Stage::ChorusLead, "Mod Lead", "MOD",
     "2-voice triangle chorus plus vibrato", kCmod, (int)(sizeof(kCmod) / sizeof(kCmod[0]))},
    {SignalChain::Stage::Phaser4, "Phaser 4", "PH4",
     "4 first-order allpass phaser", kPh4, (int)(sizeof(kPh4) / sizeof(kPh4[0]))},
    {SignalChain::Stage::Phaser8, "Phaser 8", "PH8",
     "8 second-order allpass phaser", kPh8, (int)(sizeof(kPh8) / sizeof(kPh8[0]))},
    {SignalChain::Stage::FlangerSubtle, "Flanger", "FL1",
     "Short sine flanger, light feedback", kFls, (int)(sizeof(kFls) / sizeof(kFls[0]))},
    {SignalChain::Stage::FlangerHard, "Flanger Hard", "FL2",
     "Squared-triangle flanger with resonant feedback", kFlh, (int)(sizeof(kFlh) / sizeof(kFlh[0]))},
    {SignalChain::Stage::CompBlack, "Black Comp", "TIGHT",
     "Fast feed-forward compressor, sidechain highpass", kCbm, (int)(sizeof(kCbm) / sizeof(kCbm[0]))},
    {SignalChain::Stage::CompBrutal, "Pump Comp", "PUMP",
     "Slow-attack fast-release pump compressor", kCbr, (int)(sizeof(kCbr) / sizeof(kCbr[0]))},
    {SignalChain::Stage::CompClear, "Comp", "COMP",
     "Soft-knee transparent compressor", kCgn, (int)(sizeof(kCgn) / sizeof(kCgn[0]))},
    {SignalChain::Stage::EqParametric, "Parametric EQ", "EQ4",
     "4-band RBJ parametric EQ", kEqp, (int)(sizeof(kEqp) / sizeof(kEqp[0]))},
    {SignalChain::Stage::EqTone, "Tone", "TONE",
     "Fixed 3-band tone stack", kEqt, (int)(sizeof(kEqt) / sizeof(kEqt[0]))},
    {SignalChain::Stage::EqDynamic, "Dynamic EQ", "DYN",
     "4-band dynamic EQ / multiband compressor", kDyn, (int)(sizeof(kDyn) / sizeof(kDyn[0]))},
    {SignalChain::Stage::NoiseGate, "Gate", "GATE",
     "Lookahead noise gate with hold and sidechain HPF", kGate, (int)(sizeof(kGate) / sizeof(kGate[0]))},
    {SignalChain::Stage::Tuner, "Tuner", "TUNE",
     "YIN mono tuner and harmonic-product polyphonic tuner", kTun, (int)(sizeof(kTun) / sizeof(kTun[0]))}};
}  // namespace

int moduleCount() noexcept {
    return (int)(sizeof(kModules) / sizeof(kModules[0]));
}

const ModuleSpec& moduleAt(int index) noexcept {
    return kModules[juce::jlimit(0, moduleCount() - 1, index)];
}

const ModuleSpec* moduleFor(SignalChain::Stage stage) noexcept {
    for (const auto& module : kModules) {
        if (module.stage == stage) {
            return &module;
        }
    }
    return nullptr;
}

int parameterCount() noexcept {
    int count = 0;
    for (const auto& module : kModules) {
        count += module.paramCount;
    }
    return count;
}

const char* captionFor(SignalChain::Stage stage) noexcept {
    switch (stage) {
        case SignalChain::Stage::Amp:
            return "AMP";
        case SignalChain::Stage::Cab:
            return "CAB";
        case SignalChain::Stage::Eq:
            return "EQ";
        case SignalChain::Stage::Pedal:
            return "DRIVE";
        case SignalChain::Stage::Empty:
            return "";
        default:
            if (const auto* module = moduleFor(stage)) {
                return module->caption;
            }
            return "";
    }
}

void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    for (const auto& module : kModules) {
        for (int index = 0; index < module.paramCount; ++index) {
            const auto& spec = module.params[index];
            const juce::ParameterID id{spec.id, 1};
            switch (spec.type) {
                case ParamType::Bool:
                    layout.add(std::make_unique<juce::AudioParameterBool>(id, spec.label, spec.defaultValue >= 0.5f));
                    break;
                case ParamType::Choice: {
                    juce::StringArray choices;
                    choices.addTokens(spec.choices != nullptr ? spec.choices : "", "|", "");
                    const auto defaultIndex = juce::jlimit(0, juce::jmax(0, choices.size() - 1), (int)spec.defaultValue);
                    layout.add(std::make_unique<juce::AudioParameterChoice>(id, spec.label, choices, defaultIndex));
                    break;
                }
                case ParamType::Float:
                default: {
                    juce::NormalisableRange<float> range{spec.minimum, spec.maximum, spec.step};
                    if (std::abs(spec.skew - 1.0f) > 0.001f) {
                        range = juce::NormalisableRange<float>(spec.minimum, spec.maximum, spec.step, spec.skew);
                    }
                    layout.add(std::make_unique<juce::AudioParameterFloat>(id, spec.label, range, spec.defaultValue));
                    break;
                }
            }
        }
    }
}
}  // namespace Fx
