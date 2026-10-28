#include "Fx/PedalDrive.h"

#include <cmath>

PedalDrive::PedalDrive()
    : oversampler(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false) {}

void PedalDrive::OnePole::setLowpass(float hz, double rate) noexcept {
    const int quantized = (int)std::lround(hz);
    if (quantized == lastHz) {
        return;
    }
    lastHz = quantized;
    const float limited = juce::jlimit(20.0f, (float)(rate * 0.45), hz);
    coefficient = std::exp(-juce::MathConstants<float>::twoPi * limited / (float)rate);
}

float PedalDrive::OnePole::lowpass(float x) noexcept {
    state = (1.0f - coefficient) * x + coefficient * state;
    return state;
}

void PedalDrive::prepare(double newSampleRate, int maxBlock) {
    sampleRate = juce::jmax(8000.0, newSampleRate);
    upRate = sampleRate * 4.0;
    preparedBlock = juce::jmax(1, maxBlock);
    oversampler.initProcessing((size_t)preparedBlock);
    latency = (int)std::lround(oversampler.getLatencyInSamples());
    reset();
    ready = true;
}

void PedalDrive::reset() {
    oversampler.reset();
    highpass.clear();
    toneFilter.clear();
    highpass.lastHz = -1;
    toneFilter.lastHz = -1;
    dcX = dcY = 0.0f;
    smoothDrive = 4.0f;
    smoothTone = 65.0f;
    smoothLevel = 1.0f;
}

float PedalDrive::clipAsym(float x) const noexcept {
    // Mismatched diode slopes: same unity small-signal gain, even harmonics once it bends.
    const float shaped = x >= 0.0f ? std::tanh(x) : std::tanh(1.7f * x) / 1.7f;
    return shaped + 0.07f * shaped * std::abs(shaped);
}

void PedalDrive::process(float* mono, int numSamples, int model, float drive, float tone, float levelDb) {
    if (!ready || mono == nullptr || numSamples <= 0) {
        return;
    }
    int offset = 0;
    while (offset < numSamples) {
        const int count = juce::jmin(preparedBlock, numSamples - offset);
        processChunk(mono + offset, count, model, drive, tone, levelDb);
        offset += count;
    }
}

void PedalDrive::processChunk(float* mono, int numSamples, int model, float drive, float tone, float levelDb) {
    const int count = numSamples;
    const float driveTarget = juce::jlimit(0.0f, 10.0f, drive);
    const float toneTarget = juce::jlimit(0.0f, 100.0f, tone);
    const float levelTarget = juce::Decibels::decibelsToGain(juce::jlimit(-12.0f, 12.0f, levelDb));

    float* channel = mono;
    juce::dsp::AudioBlock<float> block(&channel, 1, (size_t)count);
    auto up = oversampler.processSamplesUp(block);
    auto* data = up.getChannelPointer(0);
    const int upSamples = (int)up.getNumSamples();
    const float driveStep = (driveTarget - smoothDrive) / (float)juce::jmax(1, upSamples);
    const float toneStep = (toneTarget - smoothTone) / (float)juce::jmax(1, upSamples);
    const float levelStep = (levelTarget - smoothLevel) / (float)juce::jmax(1, upSamples);

    const int kind = juce::jlimit(kScreamer, kBoost, model);
    const float highpassHz = kind == kScreamer ? 720.0f : (kind == kDistortion ? 160.0f : 45.0f);
    highpass.setLowpass(highpassHz, upRate);

    for (int sample = 0; sample < upSamples; ++sample) {
        smoothDrive += driveStep;
        smoothTone += toneStep;
        smoothLevel += levelStep;
        const float knob = smoothDrive;
        const float toneNorm = smoothTone * 0.01f;
        float gain = 1.0f;
        float toneHz = 4000.0f;
        float trim = 1.0f;
        if (kind == kScreamer) {
            gain = 1.6f * std::pow(28.0f, knob * 0.1f);
            toneHz = 420.0f * std::pow(14.0f, toneNorm);
            trim = 0.42f;
        } else if (kind == kDistortion) {
            gain = 2.4f * std::pow(36.0f, knob * 0.1f);
            toneHz = 320.0f * std::pow(12.0f, toneNorm);
            trim = 0.28f;
        } else {
            gain = 1.0f + knob * 0.55f;
            toneHz = 1800.0f * std::pow(8.0f, toneNorm);
            trim = 0.85f;
        }

        const float driven = highpass.highpass(data[sample]);
        float shaped = kind == kBoost ? std::tanh(driven * gain) : clipAsym(driven * gain);
        if (kind == kDistortion) {
            shaped = clipAsym(shaped * 2.1f);
        }
        toneFilter.setLowpass(toneHz, upRate);
        const float colored = toneFilter.lowpass(shaped) * trim * smoothLevel;
        const float blocked = colored - dcX + 0.995f * dcY;
        dcX = colored;
        dcY = blocked;
        data[sample] = std::isfinite(blocked) ? juce::jlimit(-4.0f, 4.0f, blocked) : 0.0f;
    }
    oversampler.processSamplesDown(block);
}
