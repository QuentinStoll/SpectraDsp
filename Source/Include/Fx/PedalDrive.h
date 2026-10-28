#pragma once

#include <JuceHeader.h>

// Real-time guitar drives. The nonlinear section runs at 4x through a polyphase
// IIR half-band so the added latency stays in the range of a live pedal.
class PedalDrive {
   public:
    static constexpr int kScreamer = 0;
    static constexpr int kDistortion = 1;
    static constexpr int kBoost = 2;

    PedalDrive();

    void prepare(double sampleRate, int maxBlock);
    void reset();
    int latencySamples() const noexcept { return latency; }

    void process(float* mono, int numSamples, int model, float drive, float tone, float levelDb);

   private:
    struct OnePole {
        void setLowpass(float hz, double sampleRate) noexcept;
        float lowpass(float x) noexcept;
        float highpass(float x) noexcept { return x - lowpass(x); }
        void clear() noexcept { state = 0.0f; }
        float state = 0.0f;
        float coefficient = 0.0f;
        int lastHz = -1;
    };

    float clipAsym(float x) const noexcept;
    void processChunk(float* mono, int numSamples, int model, float drive, float tone, float levelDb);

    juce::dsp::Oversampling<float> oversampler;
    OnePole highpass;
    OnePole toneFilter;
    float dcX = 0.0f;
    float dcY = 0.0f;
    float smoothDrive = 4.0f;
    float smoothTone = 65.0f;
    float smoothLevel = 1.0f;
    double sampleRate = 48000.0;
    double upRate = 192000.0;
    int latency = 0;
    int preparedBlock = 0;
    bool ready = false;
};
