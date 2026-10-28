#include "AmpCondition.h"

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int kHumTable = 1024;

const float* humCycle() noexcept {
    static const auto table = [] {
        std::array<float, kHumTable> values{};
        float peak = 0.0f;
        for (int index = 0; index < kHumTable; ++index) {
            const auto phase = juce::MathConstants<double>::twoPi * static_cast<double>(index) / kHumTable;
            const auto sample = static_cast<float>(std::sin(phase) + 0.35 * std::sin(2.0 * phase) +
                                                   0.16 * std::sin(3.0 * phase) + 0.05 * std::sin(5.0 * phase));
            values[static_cast<std::size_t>(index)] = sample;
            peak = std::max(peak, std::abs(sample));
        }
        if (peak > 0.0f) {
            for (auto& sample : values) {
                sample /= peak;
            }
        }
        return values;
    }();
    return table.data();
}

float pole(float hertz, double sampleRate) noexcept {
    const auto rate = sampleRate > 1.0 ? sampleRate : 1.0;
    return static_cast<float>(std::exp(-juce::MathConstants<double>::twoPi * static_cast<double>(hertz) / rate));
}

float timeConstant(float milliseconds, double sampleRate) noexcept {
    const auto seconds = std::max(0.001f, milliseconds) * 0.001f;
    return static_cast<float>(std::exp(-1.0 / (static_cast<double>(seconds) * sampleRate)));
}

float shape(float sample) noexcept {
    const auto limited = juce::jlimit(-1.5f, 1.5f, sample);
    return limited - 0.12f * limited * limited * limited;
}
}  // namespace

void AmpCondition::prepare(double sampleRate, std::uint32_t seed) noexcept {
    _sampleRate = sampleRate > 1.0 ? sampleRate : 48000.0;
    _rng = seed == 0 ? 0xA341316Cu : seed;
    _follow = 1.0f - timeConstant(40.0f, _sampleRate);
    _detectCoef = pole(140.0f, _sampleRate);
    _hissHpCoef = pole(2200.0f, _sampleRate);
    _hissLpCoef = pole(9000.0f, _sampleRate);
    _toneCoef = pole(3200.0f, _sampleRate);
    _hissLow = 0.0f;
    _hissHigh = 0.0f;
    _postLow = 0.0f;
    _postHigh = 0.0f;
    _detect = 0.0f;
    _envelope = 0.0f;
    _tone = 0.0f;
    _humPhase = 0.0f;
    _humLfo = static_cast<float>(seed & 1023u) / 1023.0f;
    _pre = _post = _hum = _sag = _darken = _bias = _driftDepth = _sputter = 0.0f;
    _driftGain = 1.0f;
    _driftTarget = 1.0f;
    _driftSamples = static_cast<int>(_sampleRate);
    _dipLeft = 0;
    _dipPos = 0;
    _dipDepth = 0.0f;
    _appliedProfile = -1;
    _ready = true;
    updateVoice();
    _gapLeft = static_cast<int>(_voice.gapMax * _sampleRate);
}

void AmpCondition::set(int profile, float amount) noexcept {
    _profile = juce::jlimit(kOff, kRaw, profile);
    _amount = juce::jlimit(0.0f, 1.0f, amount);
}

float AmpCondition::noise() noexcept {
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return static_cast<float>(static_cast<std::int32_t>(_rng)) * (1.0f / 2147483648.0f);
}

AmpCondition::Voice AmpCondition::voiceFor(int profile) noexcept {
    Voice voice;
    if (profile == kBlack) {
        voice = {0.00012f, 0.0032f, 0.006f, 0.12f, 0.18f, 0.018f, 0.6f, 0.22f, 7.0f, 20.0f, 28.0f, 340.0f};
    } else if (profile == kDeath) {
        voice = {0.00005f, 0.0014f, 0.0025f, 0.30f, 0.28f, 0.012f, 0.35f, 0.12f, 18.0f, 45.0f, 10.0f, 160.0f};
    } else if (profile == kRaw) {
        voice = {0.00022f, 0.0065f, 0.012f, 0.22f, 0.34f, 0.04f, 1.0f, 0.42f, 2.2f, 7.0f, 36.0f, 520.0f};
    }
    return voice;
}

void AmpCondition::updateVoice() noexcept {
    if (_appliedProfile == _profile) {
        return;
    }
    _appliedProfile = _profile;
    _voice = voiceFor(_profile);
    _sagAttack = timeConstant(_voice.attackMs, _sampleRate);
    _sagRelease = timeConstant(_voice.releaseMs, _sampleRate);
}

void AmpCondition::slew(int numSamples) noexcept {
    updateVoice();
    const float weight = _profile == kOff ? 0.0f : _amount;
    const float remain = std::pow(std::max(0.0f, 1.0f - _follow), static_cast<float>(std::max(1, numSamples)));
    const float blend = 1.0f - remain;
    auto step = [blend](float& current, float target) { current += blend * (target - current); };
    step(_pre, _voice.preHiss * weight);
    step(_post, _voice.postHiss * weight);
    step(_hum, _voice.hum * weight);
    step(_sag, _voice.sag * weight);
    step(_darken, _voice.darken * weight);
    step(_bias, _voice.bias * weight);
    step(_driftDepth, _voice.driftDb * weight);
    step(_sputter, _voice.sputter * weight);
}

bool AmpCondition::atRest() const noexcept {
    return _pre < 1.0e-7f && _post < 1.0e-7f && _hum < 1.0e-7f && _sag < 1.0e-5f && _darken < 1.0e-5f &&
           _bias < 1.0e-5f && _driftDepth < 1.0e-4f && _sputter < 1.0e-5f && std::abs(_driftGain - 1.0f) < 1.0e-4f &&
           _dipLeft == 0;
}

void AmpCondition::processPre(float* mono, int numSamples) noexcept {
    juce::ScopedNoDenormals noDenormals;
    if (!_ready || mono == nullptr || numSamples <= 0) {
        return;
    }

    const bool silent = _profile == kOff || _amount <= 0.0001f;
    if (silent && atRest()) {
        return;
    }

    slew(numSamples);
    for (int index = 0; index < numSamples; ++index) {
        const float white = noise();
        _hissLow = _hissHpCoef * _hissLow + (1.0f - _hissHpCoef) * white;
        const float bright = white - _hissLow;
        _hissHigh = _hissLpCoef * _hissHigh + (1.0f - _hissLpCoef) * bright;
        mono[index] += _hissHigh * _pre;
    }
}

void AmpCondition::processPost(float* mono, int numSamples) noexcept {
    juce::ScopedNoDenormals noDenormals;
    if (!_ready || mono == nullptr || numSamples <= 0) {
        return;
    }

    const bool silent = _profile == kOff || _amount <= 0.0001f;
    if (silent && atRest()) {
        _envelope = 0.0f;
        return;
    }

    const auto* hum = humCycle();
    const auto humStep = static_cast<float>(50.0 * kHumTable / _sampleRate);
    const auto lfoStep = static_cast<float>(0.2 / _sampleRate);

    for (int index = 0; index < numSamples; ++index) {
        float sample = mono[index];
        if (!std::isfinite(sample)) {
            sample = 0.0f;
        }

        _detect = _detectCoef * _detect + (1.0f - _detectCoef) * sample;
        const float energy = std::min(1.0f, std::abs(_detect) * 1.5f);
        const float sagCoef = energy > _envelope ? _sagAttack : _sagRelease;
        _envelope += (1.0f - sagCoef) * (energy - _envelope);

        const float gain = 1.0f - _sag * _envelope;
        _tone = _toneCoef * _tone + (1.0f - _toneCoef) * sample;
        sample += (_tone - sample) * (_darken * _envelope);

        if (_bias > 1.0e-5f) {
            const float offset = _envelope * _bias;
            const float bent = shape(sample + offset) - shape(offset);
            sample += (bent - sample) * 0.35f;
        }

        _humLfo += lfoStep;
        if (_humLfo >= 1.0f) {
            _humLfo -= 1.0f;
        }
        const float lfo = _humLfo < 0.5f ? _humLfo * 4.0f - 1.0f : 3.0f - _humLfo * 4.0f;
        _humPhase += humStep * (1.0f + 0.003f * lfo);
        if (_humPhase >= static_cast<float>(kHumTable)) {
            _humPhase -= static_cast<float>(kHumTable);
        }
        const auto humIndex = static_cast<int>(_humPhase) & (kHumTable - 1);
        const float fraction = _humPhase - static_cast<float>(humIndex);
        const float humSample = hum[humIndex] * (1.0f - fraction) + hum[(humIndex + 1) & (kHumTable - 1)] * fraction;
        sample += humSample * _hum * (1.0f + 0.07f * lfo);

        const float floorNoise = noise();
        _postLow = _hissHpCoef * _postLow + (1.0f - _hissHpCoef) * floorNoise;
        const float bright = floorNoise - _postLow;
        _postHigh = _hissLpCoef * _postHigh + (1.0f - _hissLpCoef) * bright;
        sample += _postHigh * _post * (1.0f - 0.82f * _envelope);

        if (_dipLeft > 0) {
            const float position = static_cast<float>(_dipPos) / static_cast<float>(_dipLength);
            const float dip = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * position);
            sample *= 1.0f - _sputter * _dipDepth * dip;
            ++_dipPos;
            --_dipLeft;
            if (_dipLeft == 0 && _profile != kOff && _amount > 0.05f) {
                const float gap = _voice.gapMin + (0.5f + 0.5f * noise()) * (_voice.gapMax - _voice.gapMin);
                _gapLeft = std::max(1, static_cast<int>(gap * _sampleRate));
            }
        } else if (_profile != kOff && _amount > 0.05f && _sputter > 0.01f) {
            --_gapLeft;
            if (_gapLeft <= 0) {
                _dipLength = std::max(1, static_cast<int>((0.03f + 0.04f * (0.5f + 0.5f * noise())) * _sampleRate));
                _dipLeft = _dipLength;
                _dipPos = 0;
                _dipDepth = 0.55f + 0.3f * (0.5f + 0.5f * noise());
            }
        }

        if (--_driftSamples <= 0) {
            _driftSamples = std::max(1, static_cast<int>(0.5 * _sampleRate));
            const float wander = noise() * _driftDepth;
            const float targetDb = juce::jlimit(-_driftDepth, _driftDepth, wander);
            _driftTarget = juce::Decibels::decibelsToGain(targetDb);
        }
        _driftGain += _follow * (_driftTarget - _driftGain);
        sample *= gain * _driftGain;

        mono[index] = std::isfinite(sample) ? sample : 0.0f;
    }
}
