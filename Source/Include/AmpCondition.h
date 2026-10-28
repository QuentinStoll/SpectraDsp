#pragma once

#include <cstdint>

// Real-time wear for a WaveNet amp: hiss into the model, then hum, sag and
// slow drift on the way out. One instance per amp copy, so doubled guitars
// do not share the same sputter.
class AmpCondition {
   public:
    static constexpr int kOff = 0;
    static constexpr int kBlack = 1;
    static constexpr int kDeath = 2;
    static constexpr int kRaw = 3;

    void prepare(double sampleRate, std::uint32_t seed) noexcept;
    void set(int profile, float amount) noexcept;

    // Floor injected into the signal the amp model is about to see.
    void processPre(float* mono, int numSamples) noexcept;
    // Supply sag, hum and drift on the model output, before the cabinet.
    void processPost(float* mono, int numSamples) noexcept;

   private:
    struct Voice {
        float preHiss = 0.0f;
        float postHiss = 0.0f;
        float hum = 0.0f;
        float sag = 0.0f;
        float darken = 0.0f;
        float bias = 0.0f;
        float driftDb = 0.0f;
        float sputter = 0.0f;
        float gapMin = 8.0f;
        float gapMax = 20.0f;
        float attackMs = 30.0f;
        float releaseMs = 300.0f;
    };

    static Voice voiceFor(int profile) noexcept;
    void updateVoice() noexcept;
    void slew(int numSamples) noexcept;
    float noise() noexcept;
    bool atRest() const noexcept;

    Voice _voice{};
    int _profile = kOff;
    int _appliedProfile = kOff;
    float _amount = 0.0f;
    float _pre = 0.0f;
    float _post = 0.0f;
    float _hum = 0.0f;
    float _sag = 0.0f;
    float _darken = 0.0f;
    float _bias = 0.0f;
    float _driftDepth = 0.0f;
    float _sputter = 0.0f;
    float _follow = 0.0f;
    float _sagAttack = 0.0f;
    float _sagRelease = 0.0f;
    float _hissLow = 0.0f;
    float _hissHigh = 0.0f;
    float _postLow = 0.0f;
    float _postHigh = 0.0f;
    float _detect = 0.0f;
    float _envelope = 0.0f;
    float _tone = 0.0f;
    float _detectCoef = 0.0f;
    float _hissHpCoef = 0.0f;
    float _hissLpCoef = 0.0f;
    float _toneCoef = 0.0f;
    float _humPhase = 0.0f;
    float _humLfo = 0.0f;
    float _driftGain = 1.0f;
    float _driftTarget = 1.0f;
    int _driftSamples = 1;
    int _gapLeft = 1;
    int _dipLeft = 0;
    int _dipPos = 0;
    int _dipLength = 1;
    float _dipDepth = 0.0f;
    double _sampleRate = 48000.0;
    std::uint32_t _rng = 0xA341316Cu;
    bool _ready = false;
};
