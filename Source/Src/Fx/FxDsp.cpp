#include "Fx/FxDsp.h"

#include <algorithm>
#include <cmath>

namespace FxDsp {
namespace {
float sine(float phase) noexcept {
    return std::sin(juce::MathConstants<float>::twoPi * phase);
}

float triangle(float phase) noexcept {
    const float x = phase - std::floor(phase);
    return x < 0.5f ? (4.0f * x - 1.0f) : (3.0f - 4.0f * x);
}

float wrapPhase(float phase) noexcept {
    phase -= std::floor(phase);
    return phase;
}

float feedbackForDecay(float delaySamples, float decaySeconds, double sampleRate) noexcept {
    const float delaySeconds = delaySamples / (float)juce::jmax(1.0, sampleRate);
    if (decaySeconds <= 0.05f) {
        return 0.0f;
    }
    return juce::jlimit(0.0f, 0.985f, std::pow(10.0f, -3.0f * delaySeconds / decaySeconds));
}

float reductionDb(float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept {
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f - 1.0f / juce::jmax(1.0f, ratio);
    if (kneeDb <= 0.05f) {
        return over > 0.0f ? over * slope : 0.0f;
    }
    const float half = kneeDb * 0.5f;
    if (over <= -half) {
        return 0.0f;
    }
    if (over >= half) {
        return over * slope;
    }
    const float x = over + half;
    return (x * x) / (2.0f * kneeDb) * slope;
}

void renderPitch(DelayLine& left,
                 DelayLine& right,
                 float inLeft,
                 float inRight,
                 float& distA,
                 float& distB,
                 bool& primed,
                 float ratio,
                 float window,
                 float& outLeft,
                 float& outRight) noexcept {
    left.push(inLeft);
    right.push(inRight);
    window = juce::jmax(8.0f, window);
    if (!primed) {
        distA = window * 0.75f;
        distB = window * 0.25f;
        primed = true;
    }

    if (std::abs(ratio - 1.0f) < 0.0008f) {
        outLeft = left.read(window * 0.5f);
        outRight = right.read(window * 0.5f);
        return;
    }

    distA += 1.0f - ratio;
    distB += 1.0f - ratio;
    const float span = juce::jmax(4.0f, window - 1.0f);
    auto contain = [&](float& distance) {
        while (distance < 1.0f) {
            distance += span;
        }
        while (distance > window) {
            distance -= span;
        }
        distance = juce::jlimit(1.0f, window, distance);
    };
    contain(distA);
    contain(distB);

    const float gainA = std::sin(juce::MathConstants<float>::pi * (distA / window));
    const float gainB = std::sin(juce::MathConstants<float>::pi * (distB / window));
    const float norm = juce::jmax(1.0e-4f, gainA + gainB);
    outLeft = (left.read(distA) * gainA + left.read(distB) * gainB) / norm;
    outRight = (right.read(distA) * gainA + right.read(distB) * gainB) / norm;
}

float renderPitchMono(DelayLine& line, float input, float& distA, float& distB, bool& primed, float ratio, float window) noexcept {
    line.push(input);
    window = juce::jmax(8.0f, window);
    if (!primed) {
        distA = window * 0.75f;
        distB = window * 0.25f;
        primed = true;
    }
    if (std::abs(ratio - 1.0f) < 0.0008f) {
        return line.read(window * 0.5f);
    }

    distA += 1.0f - ratio;
    distB += 1.0f - ratio;
    const float span = juce::jmax(4.0f, window - 1.0f);
    auto contain = [&](float& distance) {
        while (distance < 1.0f) {
            distance += span;
        }
        while (distance > window) {
            distance -= span;
        }
        distance = juce::jlimit(1.0f, window, distance);
    };
    contain(distA);
    contain(distB);
    const float gainA = std::sin(juce::MathConstants<float>::pi * (distA / window));
    const float gainB = std::sin(juce::MathConstants<float>::pi * (distB / window));
    const float norm = juce::jmax(1.0e-4f, gainA + gainB);
    return (line.read(distA) * gainA + line.read(distB) * gainB) / norm;
}

int checkedVoice(int voice) noexcept {
    return voice >= 0 && voice < kVoices ? voice : -1;
}
}  // namespace

void PitchTracker::prepare(double newSampleRate) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    ring.assign(4096, 0.0f);
    difference.assign(1200, 1.0f);
    write = 0;
    since = 0;
    hop = 2048;
}

void PitchTracker::clear() {
    std::fill(ring.begin(), ring.end(), 0.0f);
    write = 0;
    since = 0;
}

bool PitchTracker::push(float sample, float& periodSamples, float& confidence) {
    if (ring.empty()) {
        return false;
    }
    ring[(size_t)write] = sample;
    write = (write + 1) % (int)ring.size();
    if (++since < hop) {
        return false;
    }
    since = 0;

    const int window = 1024;
    const int maxLag = juce::jmin(1000, (int)(sampleRate / 40.0));
    const int minLag = juce::jmax(8, (int)(sampleRate / 1200.0));
    const int ringSize = (int)ring.size();
    auto at = [&](int index) {
        int wrapped = (write + index) % ringSize;
        if (wrapped < 0) {
            wrapped += ringSize;
        }
        return ring[(size_t)wrapped];
    };

    float peak = 0.0f;
    for (int index = 0; index < window; ++index) {
        peak = juce::jmax(peak, std::abs(at(index)));
    }
    if (peak < 0.0005f) {
        confidence = 0.0f;
        return true;
    }

    float running = 0.0f;
    int best = minLag;
    float bestValue = 1.0f;
    for (int tau = 1; tau <= maxLag; ++tau) {
        float sum = 0.0f;
        for (int index = 0; index < window; ++index) {
            const float delta = at(index) - at(index + tau);
            sum += delta * delta;
        }
        running += sum;
        const float value = running > 1.0e-9f ? sum * (float)tau / running : 1.0f;
        difference[(size_t)tau] = value;
        if (tau >= minLag && value < bestValue) {
            bestValue = value;
            best = tau;
        }
    }

    for (int tau = minLag; tau < maxLag; ++tau) {
        if (difference[(size_t)tau] < 0.15f && difference[(size_t)tau] <= difference[(size_t)tau + 1]) {
            best = tau;
            bestValue = difference[(size_t)tau];
            break;
        }
    }

    float refined = (float)best;
    if (best > 1 && best < maxLag) {
        const float s0 = difference[(size_t)(best - 1)];
        const float s1 = difference[(size_t)best];
        const float s2 = difference[(size_t)(best + 1)];
        const float denom = s0 - 2.0f * s1 + s2;
        if (std::abs(denom) > 1.0e-8f) {
            refined = (float)best + 0.5f * (s0 - s2) / denom;
        }
    }

    periodSamples = juce::jmax(2.0f, refined);
    confidence = juce::jlimit(0.0f, 1.0f, 1.0f - bestValue);
    return true;
}

void Harmonizer::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(90.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
        voice.dryLeft.prepare(length);
        voice.dryRight.prepare(length);
        voice.padLeft.prepare(length);
        voice.padRight.prepare(length);
        voice.tracker.prepare(sampleRate);
    }
    reset();
}

void Harmonizer::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        voice.dryLeft.clear();
        voice.dryRight.clear();
        voice.padLeft.clear();
        voice.padRight.clear();
        voice.tiltLeft.clear();
        voice.tiltRight.clear();
        voice.tracker.clear();
        voice.primed = false;
        voice.period = 256.0f;
        voice.lastSemitones = 100.0f;
    }
}

int Harmonizer::latencySamples(float latencyMs) const {
    if (sampleRate <= 0.0) {
        return 0;
    }
    return (int)std::round(juce::jlimit(2.0f, 40.0f, latencyMs) * 0.001f * (float)sampleRate);
}

void Harmonizer::process(int voiceIndex, float* left, float* right, int numSamples, const HarmonizerSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr || numSamples <= 0) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float bases[] = {-12.0f, 12.0f, 7.0f, 4.0f, 0.0f};
    const float semitones = juce::jlimit(-24.0f, 24.0f, bases[juce::jlimit(0, 4, settings.mode)] + settings.intervalSemitones);
    const float ratio = std::pow(2.0f, semitones / 12.0f);
    const int fixed = juce::jmax(1, latencySamples(settings.latencyMs));
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    if (settings.formantLock && std::abs(semitones - voice.lastSemitones) > 0.05f) {
        const float shelf = juce::jlimit(-12.0f, 12.0f, -0.45f * semitones);
        voice.tiltLeft.setHighShelf(sampleRate, 2800.0f, 0.7f, shelf);
        voice.tiltRight.setHighShelf(sampleRate, 2800.0f, 0.7f, shelf);
        voice.lastSemitones = semitones;
    }

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        float period = voice.period;
        float confidence = 0.0f;
        if (voice.tracker.push(0.5f * (inLeft + inRight), period, confidence) && confidence > 0.6f) {
            voice.period = period;
        }

        float grain = (float)fixed;
        if (settings.formantLock && voice.period > 16.0f) {
            grain = juce::jlimit(32.0f, (float)fixed, voice.period * 2.0f);
        }
        float pitchedLeft = 0.0f;
        float pitchedRight = 0.0f;
        renderPitch(voice.left, voice.right, inLeft, inRight, voice.readA, voice.readB, voice.primed, ratio, grain,
                    pitchedLeft, pitchedRight);
        if (settings.formantLock) {
            pitchedLeft = voice.tiltLeft.process(pitchedLeft);
            pitchedRight = voice.tiltRight.process(pitchedRight);
        }

        const float pad = juce::jmax(1.0f, (float)fixed - grain * 0.5f);
        voice.padLeft.push(pitchedLeft);
        voice.padRight.push(pitchedRight);
        voice.dryLeft.push(inLeft);
        voice.dryRight.push(inRight);
        const float wetLeft = voice.padLeft.read(pad);
        const float wetRight = voice.padRight.read(pad);
        const float dryLeft = voice.dryLeft.read((float)fixed);
        const float dryRight = voice.dryRight.read((float)fixed);
        left[sample] = safeSample(dryLeft * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(dryRight * dryGain + wetRight * wetGain);
    }
}

void DropPitch::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(120.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
        voice.dryLeft.prepare(length);
        voice.dryRight.prepare(length);
        voice.padLeft.prepare(length);
        voice.padRight.prepare(length);
    }
    reset();
}

void DropPitch::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        voice.dryLeft.clear();
        voice.dryRight.clear();
        voice.padLeft.clear();
        voice.padRight.clear();
        voice.primed = false;
    }
}

int DropPitch::latencySamples(float latencyMs) const {
    if (sampleRate <= 0.0) {
        return 0;
    }
    return (int)std::round(juce::jlimit(8.0f, 40.0f, latencyMs) * 0.001f * (float)sampleRate);
}

void DropPitch::process(int voiceIndex, float* left, float* right, int numSamples, const DropSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr || numSamples <= 0) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float semitones = juce::jlimit(-12.0f, 0.0f, settings.semitones);
    const float ratio = std::pow(2.0f, semitones / 12.0f);
    const int fixed = juce::jmax(1, latencySamples(settings.latencyMs));
    const float grain = (float)fixed;
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        float pitchedLeft = 0.0f;
        float pitchedRight = 0.0f;
        renderPitch(voice.left, voice.right, inLeft, inRight, voice.readA, voice.readB, voice.primed, ratio, grain,
                    pitchedLeft, pitchedRight);
        const float pad = juce::jmax(1.0f, (float)fixed - grain * 0.5f);
        voice.padLeft.push(pitchedLeft);
        voice.padRight.push(pitchedRight);
        voice.dryLeft.push(inLeft);
        voice.dryRight.push(inRight);
        left[sample] = safeSample(voice.dryLeft.read((float)fixed) * dryGain + voice.padLeft.read(pad) * wetGain);
        right[sample] = safeSample(voice.dryRight.read((float)fixed) * dryGain + voice.padRight.read(pad) * wetGain);
    }
}

void Octaver::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    for (auto& voice : voices) {
        for (auto& stage : voice.detector) {
            stage.setLowpass(210.0f, sampleRate);
        }
        voice.subLowpass[0].setLowpass(220.0f, sampleRate);
        voice.subLowpass[1].setLowpass(110.0f, sampleRate);
    }
    reset();
}

void Octaver::reset() {
    for (auto& voice : voices) {
        for (auto& stage : voice.detector) {
            stage.clear();
        }
        voice.subLowpass[0].clear();
        voice.subLowpass[1].clear();
        voice.highpass[0].clear();
        voice.highpass[1].clear();
        voice.envelope = 0.0f;
        voice.gate = 0.0f;
        voice.period = 400.0f;
        voice.sinceEdge = 0;
        voice.schmitt = false;
        voice.phase1 = 0.0f;
        voice.phase2 = 0.0f;
        voice.lastHighpass = -1.0f;
        voice.lastCutoff = -1.0f;
    }
}

void Octaver::process(int voiceIndex, float* left, float* right, int numSamples, const OctaverSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    if (std::abs(settings.highpassHz - voice.lastHighpass) > 0.5f) {
        voice.highpass[0].setHighpass(sampleRate, settings.highpassHz, 0.707f);
        voice.highpass[1].setHighpass(sampleRate, settings.highpassHz, 0.707f);
        voice.lastHighpass = settings.highpassHz;
    }

    const float threshold = juce::jmap(settings.trigger, 0.0f, 100.0f, 0.0008f, 0.18f);
    const float attack = ballistics(1.0f, sampleRate);
    const float release = ballistics(32.0f, sampleRate);
    const float openCoef = ballistics(3.0f, sampleRate);
    const float closeCoef = ballistics(10.0f, sampleRate);
    const float mix1 = settings.mode == 1 ? 0.0f : settings.mix1 * 0.01f;
    const float mix2 = settings.mode == 0 ? 0.0f : settings.mix2 * 0.01f;
    const int maxPeriod = (int)(sampleRate / 35.0);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float mid = 0.5f * (left[sample] + right[sample]);
        float low = mid;
        for (auto& stage : voice.detector) {
            low = stage.process(low);
        }

        const float rectified = std::abs(mid);
        const float previous = voice.envelope;
        voice.envelope += (rectified > voice.envelope ? attack : release) * (rectified - voice.envelope);
        const float edgeThreshold = juce::jmax(0.004f, voice.envelope * 0.22f);
        if (!voice.schmitt && low > edgeThreshold) {
            voice.schmitt = true;
            if (voice.sinceEdge > 24 && voice.sinceEdge < maxPeriod) {
                voice.period = voice.period * 0.45f + (float)voice.sinceEdge * 0.55f;
            }
            voice.sinceEdge = 0;
        } else if (voice.schmitt && low < -edgeThreshold) {
            voice.schmitt = false;
        }
        ++voice.sinceEdge;

        if (voice.envelope > previous * 1.8f && voice.envelope > threshold) {
            voice.phase1 = 0.0f;
            voice.phase2 = 0.0f;
        }

        const float period = juce::jmax(32.0f, voice.period);
        voice.phase1 += 1.0f / (period * 2.0f);
        voice.phase2 += 1.0f / (period * 4.0f);
        if (voice.phase1 >= 1.0f) {
            voice.phase1 -= 1.0f;
        }
        if (voice.phase2 >= 1.0f) {
            voice.phase2 -= 1.0f;
        }

        const float cutoff = juce::jlimit(50.0f, 500.0f, (float)sampleRate / (period * 2.0f) * 3.0f);
        if (std::abs(cutoff - voice.lastCutoff) > 8.0f) {
            voice.subLowpass[0].setLowpass(cutoff, sampleRate);
            voice.subLowpass[1].setLowpass(cutoff * 0.5f, sampleRate);
            voice.lastCutoff = cutoff;
        }

        float sub1 = voice.subLowpass[0].process(voice.phase1 < 0.5f ? 1.0f : -1.0f);
        float sub2 = voice.subLowpass[1].process(voice.phase2 < 0.5f ? 1.0f : -1.0f);
        const float target = voice.envelope > threshold ? 1.0f : 0.0f;
        voice.gate += (target > voice.gate ? openCoef : closeCoef) * (target - voice.gate);
        float added = (sub1 * mix1 + sub2 * mix2) * voice.gate * juce::jlimit(0.0f, 1.5f, voice.envelope * 2.2f);
        added = 0.5f * (voice.highpass[0].process(added) + voice.highpass[1].process(added));
        left[sample] = safeSample(left[sample] + added);
        right[sample] = safeSample(right[sample] + added);
    }
}

void PlateReverb::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int line = (int)millisToSamples(40.0f, sampleRate) + 8;
    const int pre = (int)millisToSamples(50.0f, sampleRate) + 4;
    for (auto& voice : voices) {
        for (auto& delay : voice.lines) {
            delay.prepare(line);
        }
        voice.predelay.prepare(pre);
    }
    reset();
}

void PlateReverb::reset() {
    for (auto& voice : voices) {
        for (auto& delay : voice.lines) {
            delay.clear();
        }
        voice.predelay.clear();
        for (auto& filter : voice.damp) {
            filter.clear();
        }
        for (auto& filter : voice.toneLp) {
            filter.clear();
        }
        for (auto& filter : voice.toneHp) {
            filter.clear();
        }
        voice.dc[0].clear();
        voice.dc[1].clear();
        voice.phase = 0.0f;
    }
}

void PlateReverb::process(int voiceIndex, float* left, float* right, int numSamples, const PlateSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float bases[] = {8.9f, 11.3f, 13.7f, 16.1f};
    const float pre = juce::jmax(1.0f, millisToSamples(settings.predelayMs, sampleRate));
    const float dampPole = juce::jmap(settings.damping, 0.0f, 100.0f, 0.12f, 0.9f);
    const float lpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 1800.0f, 12000.0f);
    const float hpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 40.0f, 280.0f);
    for (int channel = 0; channel < 2; ++channel) {
        voice.toneLp[channel].setLowpass(lpHz, sampleRate);
        voice.toneHp[channel].setLowpass(hpHz, sampleRate);
    }
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);
    const float phaseInc = 0.2f / (float)sampleRate;

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        const float mono = 0.5f * (inLeft + inRight);
        voice.predelay.push(mono);
        const float excited = voice.predelay.read(pre);
        voice.phase = wrapPhase(voice.phase + phaseInc);
        const float mod = sine(voice.phase) * millisToSamples(0.15f, sampleRate);
        float taps[4];
        for (int line = 0; line < 4; ++line) {
            const float target = millisToSamples(bases[line], sampleRate) + mod * (line % 2 == 0 ? 1.0f : -1.0f);
            voice.lengths[line] += (target - voice.lengths[line]) * 0.002f;
            taps[line] = voice.lines[line].read(juce::jmax(2.0f, voice.lengths[line]));
        }
        const float a = taps[0];
        const float b = taps[1];
        const float c = taps[2];
        const float d = taps[3];
        float mixed[4] = {0.5f * (a + b + c + d), 0.5f * (a - b + c - d), 0.5f * (a + b - c - d), 0.5f * (a - b - c + d)};
        for (int line = 0; line < 4; ++line) {
            const float damped = dampPole * voice.damp[line].state + (1.0f - dampPole) * mixed[line];
            voice.damp[line].state = damped;
            const float gain = feedbackForDecay(voice.lengths[line], settings.decay, sampleRate);
            voice.lines[line].push(excited * 0.35f + damped * gain);
        }
        float wetLeft = voice.dc[0].process(0.28f * (taps[0] + 0.7f * taps[2]));
        float wetRight = voice.dc[1].process(0.28f * (taps[1] + 0.7f * taps[3]));
        const float mid = 0.5f * (wetLeft + wetRight);
        const float side = 0.18f * (wetLeft - wetRight);
        wetLeft = mid + side;
        wetRight = mid - side;
        for (int channel = 0; channel < 2; ++channel) {
            float& wet = channel == 0 ? wetLeft : wetRight;
            const float high = wet - voice.toneHp[channel].process(wet);
            wet = voice.toneLp[channel].process(high);
        }
        left[sample] = safeSample(inLeft * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(inRight * dryGain + wetRight * wetGain);
    }
}

void HallReverb::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int comb = (int)millisToSamples(160.0f, sampleRate) + 8;
    const int pre = (int)millisToSamples(90.0f, sampleRate) + 4;
    for (auto& voice : voices) {
        for (int line = 0; line < 4; ++line) {
            voice.combL[line].prepare(comb);
            voice.combR[line].prepare(comb);
        }
        voice.predelayL.prepare(pre);
        voice.predelayR.prepare(pre);
        voice.diffuseL[0].prepare((int)millisToSamples(4.7f, sampleRate), 0.72f);
        voice.diffuseL[1].prepare((int)millisToSamples(1.6f, sampleRate), 0.62f);
        voice.diffuseR[0].prepare((int)millisToSamples(5.9f, sampleRate), 0.72f);
        voice.diffuseR[1].prepare((int)millisToSamples(2.1f, sampleRate), 0.62f);
    }
    reset();
}

void HallReverb::reset() {
    for (auto& voice : voices) {
        for (int line = 0; line < 4; ++line) {
            voice.combL[line].clear();
            voice.combR[line].clear();
            voice.dampL[line] = 0.0f;
            voice.dampR[line] = 0.0f;
            voice.lengthL[line] = 1000.0f;
            voice.lengthR[line] = 1000.0f;
        }
        voice.predelayL.clear();
        voice.predelayR.clear();
        for (auto& filter : voice.diffuseL) {
            filter.clear();
        }
        for (auto& filter : voice.diffuseR) {
            filter.clear();
        }
        for (auto& filter : voice.toneLp) {
            filter.clear();
        }
        for (auto& filter : voice.toneHp) {
            filter.clear();
        }
        voice.dc[0].clear();
        voice.dc[1].clear();
        voice.phase = 0.0f;
    }
}

void HallReverb::process(int voiceIndex, float* left, float* right, int numSamples, const HallSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float baseL[] = {29.7f, 37.1f, 41.1f, 43.7f};
    const float baseR[] = {31.1f, 35.3f, 39.7f, 45.1f};
    const float size = juce::jmap(settings.size, 0.0f, 100.0f, 0.65f, 1.5f);
    const float damp = juce::jmap(settings.tone, 0.0f, 100.0f, 0.72f, 0.12f);
    const float width = juce::jlimit(0.0f, 1.0f, settings.width * 0.01f);
    const float pre = juce::jmax(1.0f, millisToSamples(settings.predelayMs, sampleRate));
    const float lpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 2200.0f, 14000.0f);
    const float hpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 30.0f, 240.0f);
    for (int channel = 0; channel < 2; ++channel) {
        voice.toneLp[channel].setLowpass(lpHz, sampleRate);
        voice.toneHp[channel].setLowpass(hpHz, sampleRate);
    }
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        voice.predelayL.push(inLeft);
        voice.predelayR.push(inRight);
        float diffL = voice.diffuseL[1].process(voice.diffuseL[0].process(voice.predelayL.read(pre)));
        float diffR = voice.diffuseR[1].process(voice.diffuseR[0].process(voice.predelayR.read(pre)));
        voice.phase = wrapPhase(voice.phase + 0.13f / (float)sampleRate);
        const float mod = sine(voice.phase) * millisToSamples(0.35f, sampleRate);
        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int line = 0; line < 4; ++line) {
            const float targetL = millisToSamples(baseL[line] * size, sampleRate) + mod;
            const float targetR = millisToSamples(baseR[line] * size, sampleRate) - mod;
            voice.lengthL[line] += (targetL - voice.lengthL[line]) * 0.0015f;
            voice.lengthR[line] += (targetR - voice.lengthR[line]) * 0.0015f;
            const float yL = voice.combL[line].read(juce::jmax(2.0f, voice.lengthL[line]));
            const float yR = voice.combR[line].read(juce::jmax(2.0f, voice.lengthR[line]));
            voice.dampL[line] = yL * (1.0f - damp) + voice.dampL[line] * damp;
            voice.dampR[line] = yR * (1.0f - damp) + voice.dampR[line] * damp;
            voice.combL[line].push(diffL + voice.dampL[line] * feedbackForDecay(voice.lengthL[line], settings.decay, sampleRate));
            voice.combR[line].push(diffR + voice.dampR[line] * feedbackForDecay(voice.lengthR[line], settings.decay, sampleRate));
            sumL += yL;
            sumR += yR;
        }
        sumL = voice.dc[0].process(sumL * 0.28f);
        sumR = voice.dc[1].process(sumR * 0.28f);
        const float mid = 0.5f * (sumL + sumR);
        const float side = 0.5f * (sumL - sumR) * width;
        float wetLeft = mid + side;
        float wetRight = mid - side;
        wetLeft = voice.toneLp[0].process(wetLeft - voice.toneHp[0].process(wetLeft));
        wetRight = voice.toneLp[1].process(wetRight - voice.toneHp[1].process(wetRight));
        left[sample] = safeSample(inLeft * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(inRight * dryGain + wetRight * wetGain);
    }
}

void ShimmerReverb::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int comb = (int)millisToSamples(180.0f, sampleRate) + 8;
    const int pitch = (int)millisToSamples(80.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        for (int line = 0; line < 4; ++line) {
            voice.combL[line].prepare(comb);
            voice.combR[line].prepare(comb);
        }
        voice.pitch.prepare(pitch);
        voice.diffuseL[0].prepare((int)millisToSamples(7.1f, sampleRate), 0.68f);
        voice.diffuseL[1].prepare((int)millisToSamples(2.7f, sampleRate), 0.6f);
        voice.diffuseR[0].prepare((int)millisToSamples(8.3f, sampleRate), 0.68f);
        voice.diffuseR[1].prepare((int)millisToSamples(3.1f, sampleRate), 0.6f);
    }
    reset();
}

void ShimmerReverb::reset() {
    for (auto& voice : voices) {
        for (int line = 0; line < 4; ++line) {
            voice.combL[line].clear();
            voice.combR[line].clear();
            voice.dampL[line] = 0.0f;
            voice.dampR[line] = 0.0f;
        }
        voice.pitch.clear();
        voice.primed = false;
        for (auto& filter : voice.diffuseL) {
            filter.clear();
        }
        for (auto& filter : voice.diffuseR) {
            filter.clear();
        }
        voice.tone[0].clear();
        voice.tone[1].clear();
        voice.dc[0].clear();
        voice.dc[1].clear();
        voice.feedbackSample = 0.0f;
    }
}

void ShimmerReverb::process(int voiceIndex, float* left, float* right, int numSamples, const ShimmerSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float baseL[] = {33.1f, 40.9f, 47.3f, 52.7f};
    const float baseR[] = {34.7f, 39.1f, 46.1f, 54.3f};
    const float ratio = settings.pitchMode == 0 ? std::pow(2.0f, 7.0f / 12.0f) : 2.0f;
    const float damp = juce::jmap(settings.damping, 0.0f, 100.0f, 0.15f, 0.88f);
    const float shimmer = juce::jlimit(0.0f, 1.0f, settings.shimmer * 0.01f);
    const float window = millisToSamples(32.0f, sampleRate);
    const float lpHz = juce::jmap(settings.damping, 0.0f, 100.0f, 9000.0f, 2500.0f);
    voice.tone[0].setLowpass(lpHz, sampleRate);
    voice.tone[1].setLowpass(lpHz, sampleRate);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        const float mono = 0.5f * (inLeft + inRight) + voice.feedbackSample;
        float diffL = voice.diffuseL[1].process(voice.diffuseL[0].process(mono));
        float diffR = voice.diffuseR[1].process(voice.diffuseR[0].process(mono));
        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int line = 0; line < 4; ++line) {
            const float delayL = millisToSamples(baseL[line], sampleRate);
            const float delayR = millisToSamples(baseR[line], sampleRate);
            const float yL = voice.combL[line].read(delayL);
            const float yR = voice.combR[line].read(delayR);
            voice.dampL[line] = yL * (1.0f - damp) + voice.dampL[line] * damp;
            voice.dampR[line] = yR * (1.0f - damp) + voice.dampR[line] * damp;
            voice.combL[line].push(diffL + voice.dampL[line] * feedbackForDecay(delayL, settings.decay, sampleRate));
            voice.combR[line].push(diffR + voice.dampR[line] * feedbackForDecay(delayR, settings.decay, sampleRate));
            sumL += yL;
            sumR += yR;
        }
        sumL = voice.dc[0].process(sumL * 0.25f);
        sumR = voice.dc[1].process(sumR * 0.25f);
        const float pitched = renderPitchMono(voice.pitch, 0.5f * (sumL + sumR), voice.readA, voice.readB, voice.primed, ratio, window);
        voice.feedbackSample = pitched * shimmer * 0.32f;
        float wetLeft = voice.tone[0].process(sumL * (1.0f - shimmer) + pitched * shimmer);
        float wetRight = voice.tone[1].process(sumR * (1.0f - shimmer) + pitched * shimmer);
        left[sample] = safeSample(inLeft * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(inRight * dryGain + wetRight * wetGain);
    }
}

void SpringReverb::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const float stages[] = {0.7f, 1.15f, 1.9f, 3.0f, 4.7f, 7.2f};
    for (auto& voice : voices) {
        for (int stage = 0; stage < 6; ++stage) {
            voice.dispersion[stage].prepare((int)millisToSamples(stages[stage], sampleRate), 0.62f);
        }
        voice.taps.prepare((int)millisToSamples(80.0f, sampleRate) + 8);
    }
    reset();
}

void SpringReverb::reset() {
    for (auto& voice : voices) {
        for (auto& stage : voice.dispersion) {
            stage.clear();
        }
        voice.taps.clear();
        voice.boing[0].clear();
        voice.boing[1].clear();
        voice.damp.clear();
        voice.tone.clear();
        voice.dc.clear();
        voice.lastBoing = -1.0f;
    }
}

void SpringReverb::process(int voiceIndex, float* left, float* right, int numSamples, const SpringSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float boing = juce::jlimit(0.0f, 1.0f, settings.boing * 0.01f);
    if (std::abs(boing - voice.lastBoing) > 0.01f) {
        const float q = juce::jmap(boing, 1.4f, 14.0f);
        voice.boing[0].setBandpass(sampleRate, 95.0f, q);
        voice.boing[1].setBandpass(sampleRate, 155.0f, q * 0.8f);
        for (auto& stage : voice.dispersion) {
            stage.gain = juce::jlimit(0.2f, 0.7f, 0.28f + boing * 0.32f);
        }
        voice.lastBoing = boing;
    }
    const float damp = juce::jmap(settings.damping, 0.0f, 100.0f, 0.15f, 0.9f);
    voice.tone.setLowpass(juce::jmap(settings.damping, 0.0f, 100.0f, 3800.0f, 1200.0f), sampleRate);
    const float taps[] = {19.0f, 27.0f, 36.0f, 48.0f};
    const float tapGain[] = {0.42f, 0.26f, 0.15f, 0.08f};
    const float feedback = feedbackForDecay(millisToSamples(48.0f, sampleRate), settings.decay, sampleRate);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        const float mono = 0.5f * (inLeft + inRight);
        float dispersed = mono;
        for (auto& stage : voice.dispersion) {
            dispersed = stage.process(dispersed);
        }
        float echo = 0.0f;
        for (int tap = 0; tap < 4; ++tap) {
            echo += voice.taps.read(millisToSamples(taps[tap], sampleRate)) * tapGain[tap];
        }
        const float damped = echo * (1.0f - damp) + voice.damp.state * damp;
        voice.damp.state = damped;
        const float ring = (voice.boing[0].process(mono) + voice.boing[1].process(mono)) * (0.04f + boing * 0.12f);
        voice.taps.push(voice.dc.process(dispersed * 0.45f + damped * feedback));
        const float wet = voice.tone.process(dispersed * 0.4f + echo + ring);
        left[sample] = safeSample(inLeft * dryGain + wet * wetGain);
        right[sample] = safeSample(inRight * dryGain + wet * wetGain);
    }
}

void GranularReverb::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(2500.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
    }
    reset();
}

void GranularReverb::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        for (auto& grain : voice.grains) {
            grain.active = false;
        }
        voice.tone[0].clear();
        voice.tone[1].clear();
        voice.spawn = 0.0f;
    }
}

void GranularReverb::process(int voiceIndex, float* left, float* right, int numSamples, const GranularSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float grainSamples = juce::jlimit(32.0f, (float)voice.left.length() * 0.5f, millisToSamples(settings.grainMs, sampleRate));
    const float density = juce::jlimit(0.5f, 40.0f, settings.density);
    const float feedback = juce::jlimit(0.0f, 0.92f, settings.feedback * 0.01f * 0.96f);
    const float diffusion = juce::jlimit(0.0f, 1.0f, settings.diffusion * 0.01f);
    const float lpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 900.0f, 12000.0f);
    voice.tone[0].setLowpass(lpHz, sampleRate);
    voice.tone[1].setLowpass(lpHz, sampleRate);
    const float overlap = juce::jmax(1.0f, density * (grainSamples / (float)sampleRate) * 0.5f);
    const float norm = 1.0f / overlap;
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);
    float previousLeft = 0.0f;
    float previousRight = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        voice.left.push(inLeft + previousLeft * feedback);
        voice.right.push(inRight + previousRight * feedback);
        voice.spawn = juce::jmin(3.0f, voice.spawn + density / (float)sampleRate);
        if (voice.spawn >= 1.0f) {
            voice.spawn -= 1.0f;
            for (auto& grain : voice.grains) {
                if (grain.active) {
                    continue;
                }
                const float near = grainSamples;
                const float far = (float)voice.left.length() * juce::jmap(diffusion, 0.12f, 0.8f);
                grain.position = near + voice.random.nextFloat() * juce::jmax(1.0f, far - near);
                grain.age = 0;
                grain.length = juce::jmax(8, (int)grainSamples);
                grain.pan = (voice.random.nextFloat() * 2.0f - 1.0f) * diffusion;
                grain.swap = voice.random.nextFloat() < diffusion;
                grain.active = true;
                break;
            }
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (auto& grain : voice.grains) {
            if (!grain.active) {
                continue;
            }
            const float window = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float)grain.age / (float)grain.length));
            float sampleL = voice.left.read(grain.position);
            float sampleR = voice.right.read(grain.position);
            if (grain.swap) {
                std::swap(sampleL, sampleR);
            }
            const float angle = (grain.pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            wetLeft += sampleL * window * std::cos(angle);
            wetRight += sampleR * window * std::sin(angle);
            grain.position -= 1.0f;
            if (++grain.age >= grain.length || grain.position < 2.0f) {
                grain.active = false;
            }
        }
        wetLeft = voice.tone[0].process(wetLeft * norm);
        wetRight = voice.tone[1].process(wetRight * norm);
        previousLeft = wetLeft;
        previousRight = wetRight;
        left[sample] = safeSample(inLeft * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(inRight * dryGain + wetRight * wetGain);
    }
}

void TapeDelay::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(360.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
    }
    reset();
}

void TapeDelay::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        for (auto& filter : voice.lowpass) {
            filter.clear();
        }
        for (auto& filter : voice.highpass) {
            filter.clear();
        }
        voice.phase = 0.0f;
        voice.time = 1000.0f;
    }
}

void TapeDelay::process(int voiceIndex, float* left, float* right, int numSamples, const TapeDelaySettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float feedback = juce::jlimit(0.0f, 0.85f, settings.feedback * 0.01f);
    const float drive = 1.0f + settings.saturation * 0.03f;
    const float wow = millisToSamples(settings.wow * 0.05f, sampleRate);
    const float lpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 900.0f, 7000.0f);
    const float hpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 180.0f, 50.0f);
    for (int channel = 0; channel < 2; ++channel) {
        voice.lowpass[channel].setLowpass(lpHz, sampleRate);
        voice.highpass[channel].setLowpass(hpHz, sampleRate);
    }
    const float target = millisToSamples(juce::jlimit(50.0f, 300.0f, settings.timeMs), sampleRate);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);
    const float norm = 1.0f / std::tanh(drive);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.time += (target - voice.time) * 0.0015f;
        voice.phase = wrapPhase(voice.phase + juce::jmap(settings.wow, 0.0f, 100.0f, 0.5f, 3.0f) / (float)sampleRate);
        const float flutter = sine(voice.phase * 5.3f) * wow * 0.25f;
        const float delay = juce::jmax(2.0f, voice.time + sine(voice.phase) * wow + flutter);
        auto saturate = [&](float value, int channel) {
            const float shaped = std::tanh(value * drive) * norm;
            const float low = voice.lowpass[channel].process(shaped);
            return low - voice.highpass[channel].process(low);
        };
        const float echoL = saturate(voice.left.read(delay), 0);
        const float echoR = saturate(voice.right.read(delay), 1);
        voice.left.push(left[sample] + echoL * feedback);
        voice.right.push(right[sample] + echoR * feedback);
        left[sample] = safeSample(left[sample] * dryGain + echoL * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + echoR * wetGain);
    }
}

void PingPongDelay::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(1200.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
    }
    reset();
}

void PingPongDelay::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        for (auto& filter : voice.hp) {
            filter.clear();
        }
        for (auto& filter : voice.lp) {
            filter.clear();
        }
        voice.time = 1000.0f;
        voice.lastHp = -1.0f;
        voice.lastLp = -1.0f;
    }
}

void PingPongDelay::process(int voiceIndex, float* left, float* right, int numSamples, const PingPongSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    float timeMs = juce::jlimit(50.0f, 1000.0f, settings.timeMs);
    if (settings.sync) {
        const float beats[] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
        const float beat = beats[juce::jlimit(0, 7, settings.division)];
        timeMs = juce::jlimit(50.0f, 1000.0f, (float)(60000.0 * beat / juce::jmax(20.0, settings.bpm)));
    }
    const float target = millisToSamples(timeMs, sampleRate);
    const float feedback = juce::jlimit(0.0f, 0.9f, settings.feedback * 0.01f);
    const float depth = juce::jlimit(0.0f, 1.0f, settings.depth * 0.01f);
    if (std::abs(settings.highpassHz - voice.lastHp) > 1.0f || std::abs(settings.lowpassHz - voice.lastLp) > 1.0f) {
        for (int channel = 0; channel < 2; ++channel) {
            voice.hp[channel].setHighpass(sampleRate, settings.highpassHz, 0.707f);
            voice.lp[channel].setLowpass(sampleRate, settings.lowpassHz, 0.707f);
        }
        voice.lastHp = settings.highpassHz;
        voice.lastLp = settings.lowpassHz;
    }
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.time += (target - voice.time) * 0.002f;
        const float delay = juce::jmax(2.0f, voice.time);
        const float echoL = voice.lp[0].process(voice.hp[0].process(voice.left.read(delay)));
        const float echoR = voice.lp[1].process(voice.hp[1].process(voice.right.read(delay)));
        const float dry = 0.5f * (left[sample] + right[sample]);
        const float pingL = dry + echoR * feedback;
        const float pingR = echoL;
        const float monoL = dry + echoL * feedback;
        const float monoR = dry + echoR * feedback;
        voice.left.push(pingL * depth + monoL * (1.0f - depth));
        voice.right.push(pingR * depth + monoR * (1.0f - depth));
        left[sample] = safeSample(left[sample] * dryGain + echoL * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + echoR * wetGain);
    }
}

void DarkDelay::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(1400.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
    }
    reset();
}

void DarkDelay::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        for (auto& filter : voice.dark) {
            filter.clear();
        }
        for (auto& filter : voice.highpass) {
            filter.clear();
        }
        for (auto& filter : voice.tone) {
            filter.clear();
        }
        voice.time = 4000.0f;
    }
}

void DarkDelay::process(int voiceIndex, float* left, float* right, int numSamples, const DarkDelaySettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float target = millisToSamples(juce::jlimit(300.0f, 1200.0f, settings.timeMs), sampleRate);
    const float feedback = juce::jlimit(0.3f, 0.95f, settings.feedback * 0.01f);
    const float darkHzL = juce::jmap(settings.darkness, 0.0f, 100.0f, 7000.0f, 450.0f);
    const float toneHz = juce::jmap(settings.tone, 0.0f, 100.0f, 1200.0f, 10000.0f);
    voice.dark[0].setLowpass(darkHzL, sampleRate);
    voice.dark[1].setLowpass(darkHzL * 0.92f, sampleRate);
    voice.highpass[0].setLowpass(110.0f, sampleRate);
    voice.highpass[1].setLowpass(110.0f, sampleRate);
    voice.tone[0].setLowpass(toneHz, sampleRate);
    voice.tone[1].setLowpass(toneHz, sampleRate);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.time += (target - voice.time) * 0.0012f;
        const float delayL = juce::jmax(2.0f, voice.time);
        const float delayR = juce::jmax(2.0f, voice.time * 1.015f);
        auto colour = [&](float value, int channel) {
            const float low = voice.dark[channel].process(value);
            return low - voice.highpass[channel].process(low);
        };
        const float echoL = voice.tone[0].process(colour(voice.left.read(delayL), 0));
        const float echoR = voice.tone[1].process(colour(voice.right.read(delayR), 1));
        voice.left.push(left[sample] + echoL * feedback);
        voice.right.push(right[sample] + echoR * feedback);
        left[sample] = safeSample(left[sample] * dryGain + echoL * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + echoR * wetGain);
    }
}

void TapeExtremeDelay::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(760.0f, sampleRate) + 8;
    const int pitch = (int)millisToSamples(80.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
        voice.pitch.prepare(pitch);
    }
    reset();
}

void TapeExtremeDelay::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        voice.pitch.clear();
        voice.primed = false;
        voice.phase = 0.0f;
        voice.time = 2000.0f;
    }
}

void TapeExtremeDelay::process(int voiceIndex, float* left, float* right, int numSamples, const TapeExtremeSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float target = millisToSamples(juce::jlimit(100.0f, 600.0f, settings.timeMs), sampleRate);
    const float feedback = juce::jlimit(0.0f, 0.9f, settings.feedback * 0.01f);
    const float drive = 1.0f + settings.saturation * 0.08f;
    const float wow = millisToSamples(settings.wow * 0.12f, sampleRate);
    const float ratio = std::pow(2.0f, juce::jlimit(-50.0f, 50.0f, settings.cents) / 1200.0f);
    const float window = millisToSamples(28.0f, sampleRate);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.time += (target - voice.time) * 0.0015f;
        voice.phase = wrapPhase(voice.phase + juce::jmap(settings.wow, 0.0f, 100.0f, 0.4f, 4.5f) / (float)sampleRate);
        const float delay = juce::jmax(2.0f, voice.time + sine(voice.phase) * wow);
        const float echoL = voice.left.read(delay);
        const float echoR = voice.right.read(delay);
        const float mid = 0.5f * (echoL + echoR);
        float pitched = mid;
        if (std::abs(settings.cents) > 0.4f) {
            pitched = renderPitchMono(voice.pitch, mid, voice.readA, voice.readB, voice.primed, ratio, window);
        }
        auto crush = [&](float value) {
            const float x = value * drive;
            return std::tanh(x + 0.18f * x * std::abs(x));
        };
        const float shapedL = crush(echoL * 0.35f + pitched * 0.65f);
        const float shapedR = crush(echoR * 0.35f + pitched * 0.65f);
        voice.left.push(left[sample] + shapedL * feedback);
        voice.right.push(right[sample] + shapedR * feedback);
        left[sample] = safeSample(left[sample] * dryGain + shapedL * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + shapedR * wetGain);
    }
}

void ReverseDelay::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(2100.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        for (int bank = 0; bank < 2; ++bank) {
            voice.loopL[(size_t)bank].assign((size_t)length, 0.0f);
            voice.loopR[(size_t)bank].assign((size_t)length, 0.0f);
        }
        voice.forwardL.prepare(length);
        voice.forwardR.prepare(length);
    }
    reset();
}

void ReverseDelay::reset() {
    for (auto& voice : voices) {
        for (int bank = 0; bank < 2; ++bank) {
            std::fill(voice.loopL[(size_t)bank].begin(), voice.loopL[(size_t)bank].end(), 0.0f);
            std::fill(voice.loopR[(size_t)bank].begin(), voice.loopR[(size_t)bank].end(), 0.0f);
        }
        voice.forwardL.clear();
        voice.forwardR.clear();
        voice.tone[0].clear();
        voice.tone[1].clear();
        voice.record = 0;
        voice.bank = 0;
        voice.fade = 1.0f;
        voice.lastMode = -1;
    }
}

void ReverseDelay::process(int voiceIndex, float* left, float* right, int numSamples, const ReverseDelaySettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    if (voice.loopL[0].empty()) {
        return;
    }
    if (voice.lastMode != settings.mode) {
        voice.record = 0;
        voice.fade = 0.0f;
        voice.lastMode = settings.mode;
    }
    const int maxLength = (int)voice.loopL[0].size() - 4;
    const int length = juce::jlimit(32, maxLength, (int)millisToSamples(juce::jlimit(100.0f, 2000.0f, settings.timeMs), sampleRate));
    voice.length = length;
    if (voice.record >= length) {
        voice.record = 0;
    }
    const float feedback = juce::jlimit(0.0f, 1.0f, settings.feedback * 0.01f);
    const float lpHz = juce::jmap(settings.tone, 0.0f, 100.0f, 1000.0f, 12000.0f);
    voice.tone[0].setLowpass(lpHz, sampleRate);
    voice.tone[1].setLowpass(lpHz, sampleRate);
    const float fadeStep = 1.0f / juce::jmax(1.0f, 0.008f * (float)sampleRate);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        if (settings.mode == 1) {
            auto& recordL = voice.loopL[(size_t)voice.bank];
            auto& recordR = voice.loopR[(size_t)voice.bank];
            auto& playL = voice.loopL[(size_t)(1 - voice.bank)];
            auto& playR = voice.loopR[(size_t)(1 - voice.bank)];
            recordL[(size_t)voice.record] = left[sample];
            recordR[(size_t)voice.record] = right[sample];
            const int playIndex = juce::jlimit(0, length - 1, length - 1 - voice.record);
            const float revL = voice.tone[0].process(playL[(size_t)playIndex]);
            const float revR = voice.tone[1].process(playR[(size_t)playIndex]);
            voice.fade = juce::jmin(1.0f, voice.fade + fadeStep);
            wetLeft = revL * voice.fade;
            wetRight = revR * voice.fade;
            if (++voice.record >= length) {
                voice.record = 0;
                voice.bank = 1 - voice.bank;
                voice.fade = 0.0f;
            }
        } else if (settings.mode == 2) {
            const float delay = (float)length;
            const float loopL = voice.tone[0].process(voice.forwardL.read(delay));
            const float loopR = voice.tone[1].process(voice.forwardR.read(delay));
            voice.forwardL.push(loopL * feedback + left[sample] * (1.0f - feedback));
            voice.forwardR.push(loopR * feedback + right[sample] * (1.0f - feedback));
            wetLeft = loopL;
            wetRight = loopR;
        } else {
            voice.time += ((float)length - voice.time) * 0.002f;
            const float echoL = voice.tone[0].process(voice.forwardL.read(juce::jmax(2.0f, voice.time)));
            const float echoR = voice.tone[1].process(voice.forwardR.read(juce::jmax(2.0f, voice.time)));
            voice.forwardL.push(left[sample] + echoL * feedback * 0.95f);
            voice.forwardR.push(right[sample] + echoR * feedback * 0.95f);
            wetLeft = echoL;
            wetRight = echoR;
        }
        left[sample] = safeSample(left[sample] * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + wetRight * wetGain);
    }
}

void EnsembleChorus::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(50.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.delay.prepare(length);
    }
    reset();
}

void EnsembleChorus::reset() {
    for (auto& voice : voices) {
        voice.delay.clear();
        voice.phase = 0.0f;
    }
}

void EnsembleChorus::process(int voiceIndex, float* left, float* right, int numSamples, const EnsembleSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float bases[] = {12.0f, 19.0f, 27.0f};
    const float offsets[] = {0.0f, 0.33f, 0.67f};
    const float depth = millisToSamples(juce::jmap(settings.depth, 0.0f, 100.0f, 0.0f, 6.0f), sampleRate);
    const float width = juce::jlimit(0.0f, 1.0f, settings.width * 0.01f);
    const float rate = juce::jlimit(0.1f, 5.0f, settings.rate) / (float)sampleRate;
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        voice.delay.push(0.5f * (inLeft + inRight));
        voice.phase = wrapPhase(voice.phase + rate);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (int oscillator = 0; oscillator < 3; ++oscillator) {
            const float lfo = sine(voice.phase + offsets[oscillator]);
            const float delay = millisToSamples(bases[oscillator], sampleRate) + lfo * depth;
            const float voiceSample = voice.delay.read(juce::jmax(1.0f, delay));
            const float pan = (float)(oscillator - 1) * width;
            const float angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            wetLeft += voiceSample * std::cos(angle);
            wetRight += voiceSample * std::sin(angle);
        }
        wetLeft *= 0.45f;
        wetRight *= 0.45f;
        left[sample] = safeSample(inLeft * dryGain + wetLeft * wetGain);
        right[sample] = safeSample(inRight * dryGain + wetRight * wetGain);
    }
}

void LeadChorus::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(70.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.delay.prepare(length);
    }
    reset();
}

void LeadChorus::reset() {
    for (auto& voice : voices) {
        voice.delay.clear();
        voice.phase = 0.0f;
    }
}

void LeadChorus::process(int voiceIndex, float* left, float* right, int numSamples, const LeadChorusSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float depth = millisToSamples(juce::jmap(settings.depth, 0.0f, 100.0f, 0.0f, 14.0f), sampleRate);
    const float vibe = juce::jlimit(0.0f, 1.0f, settings.vibe * 0.01f);
    const float rate = juce::jlimit(0.5f, 10.0f, settings.rate) / (float)sampleRate;
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inLeft = left[sample];
        const float inRight = right[sample];
        voice.delay.push(0.5f * (inLeft + inRight));
        voice.phase = wrapPhase(voice.phase + rate);
        const float lfoA = triangle(voice.phase);
        const float lfoB = triangle(voice.phase + 0.5f);
        const float a = voice.delay.read(millisToSamples(16.0f, sampleRate) + lfoA * depth);
        const float b = voice.delay.read(millisToSamples(34.0f, sampleRate) + lfoB * depth);
        const float vibrato = voice.delay.read(millisToSamples(5.0f, sampleRate) + triangle(voice.phase * 1.7f) * depth * 0.35f);
        const float wet = (a + b) * 0.35f + vibrato * vibe * 0.5f;
        left[sample] = safeSample(inLeft * dryGain + (wet + a * 0.15f) * wetGain);
        right[sample] = safeSample(inRight * dryGain + (wet + b * 0.15f) * wetGain);
    }
}

void Phaser4::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    reset();
}

void Phaser4::reset() {
    for (auto& voice : voices) {
        for (auto& channel : voice.stages) {
            for (auto& stage : channel) {
                stage.clear();
            }
        }
        voice.phase = 0.0f;
    }
}

void Phaser4::process(int voiceIndex, float* left, float* right, int numSamples, const Phaser4Settings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float rate = juce::jlimit(0.1f, 5.0f, settings.rate) / (float)sampleRate;
    const float depth = juce::jmap(settings.depth, 0.0f, 100.0f, 0.15f, 2.4f);
    const float center = juce::jlimit(80.0f, 6000.0f, settings.centerHz);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.phase = wrapPhase(voice.phase + rate);
        const float hz = juce::jlimit(40.0f, (float)(sampleRate * 0.45), center * std::pow(2.0f, sine(voice.phase) * depth));
        float wetLeft = left[sample];
        float wetRight = right[sample];
        for (int stage = 0; stage < 4; ++stage) {
            voice.stages[0][stage].setFrequency(hz, sampleRate);
            voice.stages[1][stage].setFrequency(hz, sampleRate);
            wetLeft = voice.stages[0][stage].process(wetLeft);
            wetRight = voice.stages[1][stage].process(wetRight);
        }
        const float phasedLeft = 0.5f * (left[sample] + wetLeft);
        const float phasedRight = 0.5f * (right[sample] + wetRight);
        left[sample] = safeSample(left[sample] * dryGain + phasedLeft * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + phasedRight * wetGain);
    }
}

void Phaser8::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    reset();
}

void Phaser8::reset() {
    for (auto& voice : voices) {
        for (auto& channel : voice.stages) {
            for (auto& stage : channel) {
                stage.clear();
            }
        }
        voice.phase = 0.0f;
        voice.lastHz[0] = voice.lastHz[1] = -1.0f;
        voice.lastQ = -1.0f;
    }
}

void Phaser8::process(int voiceIndex, float* left, float* right, int numSamples, const Phaser8Settings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float rate = juce::jlimit(0.1f, 10.0f, settings.rate) / (float)sampleRate;
    const float depth = juce::jmap(settings.sweep, 0.0f, 100.0f, 0.4f, 3.2f) * juce::jmap(settings.depth, 0.0f, 100.0f, 0.25f, 1.0f);
    const float q = juce::jlimit(2.0f, 4.0f, settings.q);
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.phase = wrapPhase(voice.phase + rate);
        const float hzL = juce::jlimit(50.0f, (float)(sampleRate * 0.42), 900.0f * std::pow(2.0f, sine(voice.phase) * depth));
        const float hzR = juce::jlimit(50.0f, (float)(sampleRate * 0.42), 900.0f * std::pow(2.0f, sine(voice.phase + 0.08f) * depth));
        if (hzL != voice.lastHz[0] || hzR != voice.lastHz[1] || q != voice.lastQ) {
            for (int stage = 0; stage < 8; ++stage) {
                const float spread = 1.0f + 0.04f * (float)(stage - 4);
                voice.stages[0][stage].setAllpass(sampleRate, hzL * spread, q);
                voice.stages[1][stage].setAllpass(sampleRate, hzR * spread, q);
            }
            voice.lastHz[0] = hzL;
            voice.lastHz[1] = hzR;
            voice.lastQ = q;
        }
        float wetLeft = left[sample];
        float wetRight = right[sample];
        for (int stage = 0; stage < 8; ++stage) {
            wetLeft = voice.stages[0][stage].process(wetLeft);
            wetRight = voice.stages[1][stage].process(wetRight);
        }
        left[sample] = safeSample(left[sample] * dryGain + 0.5f * (left[sample] + wetLeft) * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + 0.5f * (right[sample] + wetRight) * wetGain);
    }
}

void Flanger::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(20.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
    }
    reset();
}

void Flanger::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        voice.ring[0].clear();
        voice.ring[1].clear();
        voice.dc[0].clear();
        voice.dc[1].clear();
        voice.phase = 0.0f;
        voice.lastRingHz = -1.0f;
        voice.ringCountdown = 0;
    }
}

void Flanger::process(int voiceIndex, float* left, float* right, int numSamples, const FlangerSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    const float minMs = settings.hard ? 1.0f : 0.15f;
    const float maxMs = settings.hard ? 8.0f : 3.0f;
    const float depth = juce::jlimit(0.0f, 1.0f, settings.depth * 0.01f);
    const float centre = millisToSamples((minMs + maxMs) * 0.5f, sampleRate);
    const float amount = millisToSamples((maxMs - minMs) * 0.5f, sampleRate) * depth;
    const float maxFeedback = settings.hard ? 0.9f : 0.5f;
    const float feedback = juce::jlimit(0.0f, maxFeedback, settings.feedback * 0.01f);
    const float rateMin = settings.hard ? 0.5f : 0.1f;
    const float rateMax = settings.hard ? 10.0f : 5.0f;
    const float rate = juce::jlimit(rateMin, rateMax, settings.rate) / (float)sampleRate;
    float dryGain = 1.0f;
    float wetGain = 0.0f;
    equalPower(settings.mix, dryGain, wetGain);

    for (int sample = 0; sample < numSamples; ++sample) {
        voice.phase = wrapPhase(voice.phase + rate);
        const float lfoSine = sine(voice.phase);
        const float lfoHard = std::tanh(triangle(voice.phase) * 5.0f);
        const float lfo = settings.hard ? lfoHard : lfoSine;
        const float lfoR = settings.hard ? std::tanh(triangle(voice.phase + 0.13f) * 5.0f) : lfo;
        const float delayL = juce::jlimit(1.0f, millisToSamples(12.0f, sampleRate), centre + lfo * amount);
        const float delayR = juce::jlimit(1.0f, millisToSamples(12.0f, sampleRate), centre + lfoR * amount);
        if (settings.hard && --voice.ringCountdown <= 0) {
            const float hz = juce::jlimit(200.0f, 6000.0f, (float)sampleRate / juce::jmax(2.0f, delayL));
            const float gain = juce::jmap(settings.ring, 0.0f, 100.0f, 0.0f, 10.0f);
            voice.ring[0].setPeak(sampleRate, hz, 3.5f, gain);
            voice.ring[1].setPeak(sampleRate, hz * 0.97f, 3.5f, gain);
            voice.ringCountdown = 16;
        }
        float echoL = voice.left.read(delayL);
        float echoR = voice.right.read(delayR);
        if (settings.hard && settings.ring > 0.5f) {
            echoL = voice.ring[0].process(echoL);
            echoR = voice.ring[1].process(echoR);
        }
        echoL = voice.dc[0].process(echoL);
        echoR = voice.dc[1].process(echoR);
        voice.left.push(left[sample] - echoL * feedback);
        voice.right.push(right[sample] - echoR * feedback);
        left[sample] = safeSample(left[sample] * dryGain + echoL * wetGain);
        right[sample] = safeSample(right[sample] * dryGain + echoR * wetGain);
    }
}

void Compressor::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    reset();
}

void Compressor::reset() {
    for (auto& voice : voices) {
        voice.sidechain[0].clear();
        voice.sidechain[1].clear();
        voice.envelope = 0.0f;
        voice.rms = 0.0f;
        voice.lastSidechain = -1.0f;
    }
}

void Compressor::process(int voiceIndex, float* left, float* right, int numSamples, const CompressorSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    if (settings.sidechain && std::abs(settings.sidechainHz - voice.lastSidechain) > 0.5f) {
        voice.sidechain[0].setHighpass(sampleRate, settings.sidechainHz, 0.707f);
        voice.sidechain[1].setHighpass(sampleRate, settings.sidechainHz, 0.707f);
        voice.lastSidechain = settings.sidechainHz;
    }
    const float attack = ballistics(settings.attackMs, sampleRate);
    const float release = ballistics(settings.releaseMs, sampleRate);
    const float rmsCoef = ballistics(5.0f, sampleRate);
    const float makeup = juce::Decibels::decibelsToGain(settings.makeupDb);

    for (int sample = 0; sample < numSamples; ++sample) {
        float detectL = left[sample];
        float detectR = right[sample];
        if (settings.sidechain) {
            detectL = voice.sidechain[0].process(detectL);
            detectR = voice.sidechain[1].process(detectR);
        }
        float detect = 0.0f;
        if (settings.peak) {
            detect = juce::jmax(std::abs(detectL), std::abs(detectR));
        } else {
            const float power = juce::jmax(detectL * detectL, detectR * detectR);
            voice.rms += rmsCoef * (power - voice.rms);
            detect = std::sqrt(juce::jmax(0.0f, voice.rms));
        }
        const float coef = detect > voice.envelope ? attack : release;
        voice.envelope += coef * (detect - voice.envelope);
        const float levelDb = juce::Decibels::gainToDecibels(voice.envelope, -80.0f);
        const float gain = juce::Decibels::decibelsToGain(-reductionDb(levelDb, settings.thresholdDb, settings.ratio, settings.kneeDb)) * makeup;
        left[sample] = safeSample(left[sample] * gain);
        right[sample] = safeSample(right[sample] * gain);
    }
}

void ParametricEq::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    reset();
}

void ParametricEq::reset() {
    for (auto& voice : voices) {
        for (auto& channel : voice.bands) {
            for (auto& band : channel) {
                band.clear();
            }
        }
        for (float& value : voice.lastF) {
            value = -1.0f;
        }
    }
}

void ParametricEq::process(int voiceIndex, float* left, float* right, int numSamples, const ParametricEqSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    bool dirty = false;
    for (int band = 0; band < 4; ++band) {
        if (std::abs(settings.frequency[band] - voice.lastF[band]) > 0.2f ||
            std::abs(settings.gain[band] - voice.lastG[band]) > 0.02f ||
            std::abs(settings.q[band] - voice.lastQ[band]) > 0.01f) {
            dirty = true;
        }
    }
    if (dirty) {
        for (int channel = 0; channel < 2; ++channel) {
            voice.bands[channel][0].setLowShelf(sampleRate, settings.frequency[0], settings.q[0], settings.gain[0]);
            voice.bands[channel][1].setPeak(sampleRate, settings.frequency[1], settings.q[1], settings.gain[1]);
            voice.bands[channel][2].setPeak(sampleRate, settings.frequency[2], settings.q[2], settings.gain[2]);
            voice.bands[channel][3].setHighShelf(sampleRate, settings.frequency[3], settings.q[3], settings.gain[3]);
        }
        for (int band = 0; band < 4; ++band) {
            voice.lastF[band] = settings.frequency[band];
            voice.lastG[band] = settings.gain[band];
            voice.lastQ[band] = settings.q[band];
        }
    }
    for (int sample = 0; sample < numSamples; ++sample) {
        float l = left[sample];
        float r = right[sample];
        for (int band = 0; band < 4; ++band) {
            l = voice.bands[0][band].process(l);
            r = voice.bands[1][band].process(r);
        }
        left[sample] = safeSample(l);
        right[sample] = safeSample(r);
    }
}

void ToneEq::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    reset();
}

void ToneEq::reset() {
    for (auto& voice : voices) {
        for (int channel = 0; channel < 2; ++channel) {
            voice.low[channel].clear();
            voice.mid[channel].clear();
            voice.high[channel].clear();
        }
        voice.lastLow = voice.lastMid = voice.lastHigh = 100.0f;
    }
}

void ToneEq::process(int voiceIndex, float* left, float* right, int numSamples, const ToneEqSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    if (std::abs(settings.low - voice.lastLow) > 0.02f || std::abs(settings.mid - voice.lastMid) > 0.02f ||
        std::abs(settings.high - voice.lastHigh) > 0.02f) {
        for (int channel = 0; channel < 2; ++channel) {
            voice.low[channel].setLowShelf(sampleRate, 100.0f, 0.707f, settings.low);
            voice.mid[channel].setPeak(sampleRate, 800.0f, 0.7f, settings.mid);
            voice.high[channel].setHighShelf(sampleRate, 3000.0f, 0.707f, settings.high);
        }
        voice.lastLow = settings.low;
        voice.lastMid = settings.mid;
        voice.lastHigh = settings.high;
    }
    for (int sample = 0; sample < numSamples; ++sample) {
        float l = voice.high[0].process(voice.mid[0].process(voice.low[0].process(left[sample])));
        float r = voice.high[1].process(voice.mid[1].process(voice.low[1].process(right[sample])));
        left[sample] = safeSample(l);
        right[sample] = safeSample(r);
    }
}

void DynamicEq::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    reset();
}

void DynamicEq::reset() {
    for (auto& voice : voices) {
        for (auto& channel : voice.filters) {
            for (auto& band : channel) {
                band.clear();
            }
        }
        for (float& env : voice.envelope) {
            env = 0.0f;
        }
        for (float& frequency : voice.lastF) {
            frequency = -1.0f;
        }
    }
}

void DynamicEq::process(int voiceIndex, float* left, float* right, int numSamples, const DynamicEqSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    bool dirty = false;
    for (int band = 0; band < 4; ++band) {
        if (std::abs(settings.frequency[band] - voice.lastF[band]) > 0.4f || std::abs(settings.q[band] - voice.lastQ[band]) > 0.01f) {
            dirty = true;
        }
    }
    if (dirty) {
        for (int channel = 0; channel < 2; ++channel) {
            voice.filters[channel][0].setLowpass(sampleRate, settings.frequency[0], settings.q[0]);
            voice.filters[channel][1].setBandpass(sampleRate, settings.frequency[1], settings.q[1]);
            voice.filters[channel][2].setBandpass(sampleRate, settings.frequency[2], settings.q[2]);
            voice.filters[channel][3].setHighpass(sampleRate, settings.frequency[3], settings.q[3]);
        }
        for (int band = 0; band < 4; ++band) {
            voice.lastF[band] = settings.frequency[band];
            voice.lastQ[band] = settings.q[band];
        }
    }
    const float attack = ballistics(8.0f, sampleRate);
    const float release = ballistics(80.0f, sampleRate);

    for (int sample = 0; sample < numSamples; ++sample) {
        float l = left[sample];
        float r = right[sample];
        for (int band = 0; band < 4; ++band) {
            const float bandL = voice.filters[0][band].process(l);
            const float bandR = voice.filters[1][band].process(r);
            const float detect = juce::jmax(std::abs(bandL), std::abs(bandR));
            const float coef = detect > voice.envelope[band] ? attack : release;
            voice.envelope[band] += coef * (detect - voice.envelope[band]);
            const float levelDb = juce::Decibels::gainToDecibels(voice.envelope[band], -80.0f);
            const float gain = juce::Decibels::decibelsToGain(-reductionDb(levelDb, settings.threshold[band], settings.ratio[band], 0.0f));
            l += bandL * (gain - 1.0f);
            r += bandR * (gain - 1.0f);
        }
        left[sample] = safeSample(l);
        right[sample] = safeSample(r);
    }
}

void NoiseGate::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    const int length = (int)millisToSamples(30.0f, sampleRate) + 8;
    for (auto& voice : voices) {
        voice.left.prepare(length);
        voice.right.prepare(length);
    }
    reset();
}

void NoiseGate::reset() {
    for (auto& voice : voices) {
        voice.left.clear();
        voice.right.clear();
        voice.sidechain[0].clear();
        voice.sidechain[1].clear();
        voice.envelope = 0.0f;
        voice.gain = 1.0f;
        voice.hold = 0;
        voice.open = false;
        voice.lastSidechain = -1.0f;
    }
}

int NoiseGate::latencySamples(float lookaheadMs) const {
    if (sampleRate <= 0.0) {
        return 0;
    }
    return (int)std::round(juce::jlimit(5.0f, 20.0f, lookaheadMs) * 0.001f * (float)sampleRate);
}

void NoiseGate::process(int voiceIndex, float* left, float* right, int numSamples, const GateSettings& settings) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr) {
        return;
    }
    auto& voice = voices[(size_t)index];
    if (settings.sidechain && std::abs(settings.sidechainHz - voice.lastSidechain) > 0.5f) {
        voice.sidechain[0].setHighpass(sampleRate, settings.sidechainHz, 0.707f);
        voice.sidechain[1].setHighpass(sampleRate, settings.sidechainHz, 0.707f);
        voice.lastSidechain = settings.sidechainHz;
    }
    const float openThreshold = juce::Decibels::decibelsToGain(settings.thresholdDb);
    const float closeThreshold = openThreshold * juce::Decibels::decibelsToGain(-4.0f);
    const float range = juce::Decibels::decibelsToGain(settings.rangeDb);
    const int holdSamples = (int)millisToSamples(settings.holdMs, sampleRate);
    const float lookahead = (float)juce::jmax(1, latencySamples(settings.lookaheadMs));
    const float attack = ballistics(juce::jmax(1.0f, settings.attackMs), sampleRate);
    const float release = ballistics(juce::jmax(1.0f, settings.releaseMs), sampleRate);
    const float follow = ballistics(1.0f, sampleRate);

    for (int sample = 0; sample < numSamples; ++sample) {
        float detectL = left[sample];
        float detectR = right[sample];
        if (settings.sidechain) {
            detectL = voice.sidechain[0].process(detectL);
            detectR = voice.sidechain[1].process(detectR);
        }
        const float peak = juce::jmax(std::abs(detectL), std::abs(detectR));
        voice.envelope += follow * (peak - voice.envelope);
        if (voice.envelope > openThreshold) {
            voice.open = true;
            voice.hold = holdSamples;
        } else if (voice.envelope < closeThreshold) {
            if (voice.hold > 0) {
                --voice.hold;
            } else {
                voice.open = false;
            }
        }
        const float target = voice.open ? 1.0f : range;
        const float coef = target > voice.gain ? attack : release;
        voice.gain += coef * (target - voice.gain);
        voice.left.push(left[sample]);
        voice.right.push(right[sample]);
        left[sample] = safeSample(voice.left.read(lookahead) * voice.gain);
        right[sample] = safeSample(voice.right.read(lookahead) * voice.gain);
    }
}

void Tuner::prepare(double newSampleRate, int) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    constexpr int fftSize = 4096;
    for (auto& voice : voices) {
        voice.tracker.prepare(sampleRate);
        voice.fft = std::make_unique<juce::dsp::FFT>(12);
        voice.fftData.assign((size_t)fftSize * 2, 0.0f);
        voice.window.assign(fftSize, 0.0f);
        for (int index = 0; index < fftSize; ++index) {
            voice.window[(size_t)index] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float)index / (float)(fftSize - 1)));
        }
        voice.filled = 0;
    }
    reset();
}

void Tuner::reset() {
    for (auto& voice : voices) {
        voice.tracker.clear();
        std::fill(voice.fftData.begin(), voice.fftData.end(), 0.0f);
        voice.filled = 0;
        voice.strobe = 0.0f;
    }
}

void Tuner::analyseMono(Voice& voice, float period, float confidence, float rms, Fx::TunerSnapshot& snapshot) {
    snapshot.mode = 0;
    snapshot.count = 0;
    snapshot.confidence = confidence;
    if (rms < 0.0008f || confidence < 0.55f || period < 2.0f) {
        voice.strobe *= 0.9f;
        snapshot.strobe = voice.strobe;
        return;
    }
    const float frequency = (float)sampleRate / period;
    const float midiFloat = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    const int midi = (int)std::round(midiFloat);
    const float cents = (midiFloat - (float)midi) * 100.0f;
    snapshot.count = 1;
    snapshot.midi[0] = midi;
    snapshot.cents[0] = cents;
    if (std::abs(cents) > 1.2f) {
        voice.strobe += cents * 0.00035f;
    } else {
        voice.strobe *= 0.85f;
    }
    snapshot.strobe = voice.strobe;
}

void Tuner::analysePoly(Voice& voice, Fx::TunerSnapshot& snapshot) {
    snapshot.mode = 1;
    snapshot.count = 0;
    if (voice.fft == nullptr) {
        return;
    }
    std::fill(voice.fftData.begin() + 4096, voice.fftData.end(), 0.0f);
    voice.fft->performFrequencyOnlyForwardTransform(voice.fftData.data());
    float scores[100];
    int notes[100];
    int candidates = 0;
    float maxScore = 0.0f;
    for (int midi = 40; midi <= 88 && candidates < 100; ++midi) {
        const float frequency = 440.0f * std::pow(2.0f, ((float)midi - 69.0f) / 12.0f);
        auto magnitude = [&](float freq) {
            const float bin = freq * 4096.0f / (float)sampleRate;
            const int k = (int)std::round(bin);
            if (k < 1 || k >= 2046) {
                return 0.0f;
            }
            return voice.fftData[(size_t)k];
        };
        const float score = magnitude(frequency) + 0.55f * magnitude(frequency * 2.0f) + 0.3f * magnitude(frequency * 3.0f);
        scores[candidates] = score;
        notes[candidates] = midi;
        maxScore = juce::jmax(maxScore, score);
        ++candidates;
    }
    if (maxScore < 1.0e-4f) {
        voice.strobe *= 0.9f;
        snapshot.strobe = voice.strobe;
        return;
    }
    for (int index = 1; index < candidates - 1 && snapshot.count < 6; ++index) {
        if (scores[index] < maxScore * 0.18f || scores[index] < scores[index - 1] || scores[index] < scores[index + 1]) {
            continue;
        }
        const int midi = notes[index];
        const float frequency = 440.0f * std::pow(2.0f, ((float)midi - 69.0f) / 12.0f);
        const float bin = frequency * 4096.0f / (float)sampleRate;
        const int k = juce::jlimit(1, 2045, (int)std::round(bin));
        const float s0 = voice.fftData[(size_t)(k - 1)];
        const float s1 = voice.fftData[(size_t)k];
        const float s2 = voice.fftData[(size_t)(k + 1)];
        const float denom = s0 - 2.0f * s1 + s2;
        float delta = 0.0f;
        if (std::abs(denom) > 1.0e-8f) {
            delta = 0.5f * (s0 - s2) / denom;
        }
        const float refined = ((float)k + delta) * (float)sampleRate / 4096.0f;
        const float cents = 1200.0f * std::log2(juce::jmax(1.0f, refined) / frequency);
        snapshot.midi[snapshot.count] = midi;
        snapshot.cents[snapshot.count] = juce::jlimit(-50.0f, 50.0f, cents);
        ++snapshot.count;
    }
    snapshot.confidence = juce::jlimit(0.0f, 1.0f, maxScore > 0.0f ? 0.8f : 0.0f);
    const float cents = snapshot.count > 0 ? snapshot.cents[0] : 0.0f;
    if (snapshot.count > 0 && std::abs(cents) > 1.2f) {
        voice.strobe += cents * 0.00035f;
    } else {
        voice.strobe *= 0.85f;
    }
    snapshot.strobe = voice.strobe;
}

void Tuner::process(int voiceIndex, const float* left, const float* right, int numSamples, const TunerSettings& settings, Fx::TunerSnapshot& snapshot) {
    const int index = checkedVoice(voiceIndex);
    if (index < 0 || left == nullptr || right == nullptr || !settings.enabled) {
        return;
    }
    auto& voice = voices[(size_t)index];
    float sum = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample) {
        const float mid = 0.5f * (left[sample] + right[sample]);
        sum += mid * mid;
        if (settings.mode == 0) {
            float period = 0.0f;
            float confidence = 0.0f;
            if (voice.tracker.push(mid, period, confidence)) {
                analyseMono(voice, period, confidence, std::sqrt(sum / (float)(sample + 1)), snapshot);
            }
        } else if (voice.filled < 4096) {
            voice.fftData[(size_t)voice.filled] = mid * voice.window[(size_t)voice.filled];
            if (++voice.filled >= 4096) {
                analysePoly(voice, snapshot);
                voice.filled = 0;
            }
        }
    }
}
}  // namespace FxDsp
