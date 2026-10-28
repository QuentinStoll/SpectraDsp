#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>
#include <vector>

#include "Fx/TunerSnapshot.h"

namespace FxDsp {
inline constexpr int kVoices = 4;

inline float safeSample(float value) noexcept {
    return std::isfinite(value) ? juce::jlimit(-8.0f, 8.0f, value) : 0.0f;
}

inline void equalPower(float mixPercent, float& dry, float& wet) noexcept {
    const float angle = juce::jlimit(0.0f, 1.0f, mixPercent * 0.01f) * juce::MathConstants<float>::halfPi;
    dry = std::cos(angle);
    wet = std::sin(angle);
}

// First half of the knob adds the effect on top of a full dry signal.
// The second half fades the dry out, so mix 100% is fully wet.
inline void pedalMix(float mixPercent, float& dry, float& wet) noexcept {
    const float mix = juce::jlimit(0.0f, 1.0f, mixPercent * 0.01f);
    if (mix <= 0.5f) {
        dry = 1.0f;
        wet = mix * 2.0f;
    } else {
        dry = (1.0f - mix) * 2.0f;
        wet = 1.0f;
    }
}

inline float millisToSamples(float millis, double sampleRate) noexcept {
    return (float)(juce::jmax(0.0, millis * 0.001 * sampleRate));
}

inline float ballistics(float millis, double sampleRate) noexcept {
    const float seconds = juce::jmax(0.0001f, millis * 0.001f);
    return 1.0f - std::exp(-1.0f / (seconds * (float)sampleRate));
}

struct DelayLine {
    void prepare(int samples) {
        size = juce::jmax(8, samples);
        buffer.assign((size_t)size, 0.0f);
        write = 0;
    }

    void clear() noexcept {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        write = 0;
    }

    void push(float value) noexcept {
        if (buffer.empty()) {
            return;
        }
        buffer[(size_t)write] = value;
        if (++write >= size) {
            write = 0;
        }
    }

    float read(float delaySamples) const noexcept {
        if (size < 4) {
            return 0.0f;
        }
        const float delay = juce::jlimit(1.0f, (float)size - 3.0f, delaySamples);
        float position = (float)write - delay;
        while (position < 0.0f) {
            position += (float)size;
        }
        const int index = (int)position;
        const float fraction = position - (float)index;
        const float x0 = at(index - 1);
        const float x1 = at(index);
        const float x2 = at(index + 1);
        const float x3 = at(index + 2);
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * fraction + c2) * fraction + c1) * fraction + x1;
    }

    int length() const noexcept { return size; }

   private:
    float at(int index) const noexcept {
        index %= size;
        if (index < 0) {
            index += size;
        }
        return buffer[(size_t)index];
    }

    std::vector<float> buffer;
    int write = 0;
    int size = 0;
};

struct DcBlock {
    float process(float x) noexcept {
        const float y = x - x1 + 0.995f * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void clear() noexcept { x1 = y1 = 0.0f; }
    float x1 = 0.0f;
    float y1 = 0.0f;
};

struct OnePole {
    void setLowpass(float cutoffHz, double sampleRate) noexcept {
        const float hz = juce::jlimit(10.0f, (float)(sampleRate * 0.45), cutoffHz);
        coefficient = std::exp(-juce::MathConstants<float>::twoPi * hz / (float)sampleRate);
    }

    float process(float x) noexcept {
        state = (1.0f - coefficient) * x + coefficient * state;
        return state;
    }

    void clear() noexcept { state = 0.0f; }
    float state = 0.0f;
    float coefficient = 0.0f;
};

struct Biquad {
    void clear() noexcept { z1 = z2 = 0.0f; }

    float process(float x) noexcept {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass(double sampleRate, float frequency, float q) noexcept { setCommon(sampleRate, frequency, q, Kind::Lowpass); }
    void setHighpass(double sampleRate, float frequency, float q) noexcept { setCommon(sampleRate, frequency, q, Kind::Highpass); }
    void setBandpass(double sampleRate, float frequency, float q) noexcept { setCommon(sampleRate, frequency, q, Kind::Bandpass); }
    void setPeak(double sampleRate, float frequency, float q, float gainDb) noexcept {
        setShelfOrPeak(sampleRate, frequency, q, gainDb, Kind::Peak);
    }
    void setLowShelf(double sampleRate, float frequency, float q, float gainDb) noexcept {
        setShelfOrPeak(sampleRate, frequency, q, gainDb, Kind::LowShelf);
    }
    void setHighShelf(double sampleRate, float frequency, float q, float gainDb) noexcept {
        setShelfOrPeak(sampleRate, frequency, q, gainDb, Kind::HighShelf);
    }
    void setAllpass(double sampleRate, float frequency, float q) noexcept { setCommon(sampleRate, frequency, q, Kind::Allpass); }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

   private:
    enum class Kind { Lowpass,
                      Highpass,
                      Bandpass,
                      Peak,
                      LowShelf,
                      HighShelf,
                      Allpass };

    void setCommon(double sampleRate, float frequency, float q, Kind kind) noexcept {
        const float hz = juce::jlimit(20.0f, (float)(sampleRate * 0.45), frequency);
        const float quality = juce::jlimit(0.05f, 20.0f, q);
        const float w0 = juce::MathConstants<float>::twoPi * hz / (float)sampleRate;
        const float cosine = std::cos(w0);
        const float sine = std::sin(w0);
        const float alpha = sine / (2.0f * quality);
        const float a0 = 1.0f + alpha;
        if (kind == Kind::Lowpass) {
            b0 = ((1.0f - cosine) * 0.5f) / a0;
            b1 = (1.0f - cosine) / a0;
            b2 = b0;
        } else if (kind == Kind::Highpass) {
            b0 = ((1.0f + cosine) * 0.5f) / a0;
            b1 = -(1.0f + cosine) / a0;
            b2 = b0;
        } else if (kind == Kind::Bandpass) {
            b0 = alpha / a0;
            b1 = 0.0f;
            b2 = -alpha / a0;
        } else {
            b0 = (1.0f - alpha) / a0;
            b1 = (-2.0f * cosine) / a0;
            b2 = (1.0f + alpha) / a0;
            a1 = b1;
            a2 = b0;
            return;
        }
        a1 = (-2.0f * cosine) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setShelfOrPeak(double sampleRate, float frequency, float q, float gainDb, Kind kind) noexcept {
        const float hz = juce::jlimit(20.0f, (float)(sampleRate * 0.45), frequency);
        const float quality = juce::jlimit(0.05f, 20.0f, q);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = juce::MathConstants<float>::twoPi * hz / (float)sampleRate;
        const float cosine = std::cos(w0);
        const float sine = std::sin(w0);
        const float alpha = sine / (2.0f * quality);
        float a0 = 1.0f;
        if (kind == Kind::Peak) {
            a0 = 1.0f + alpha / A;
            b0 = (1.0f + alpha * A) / a0;
            b1 = (-2.0f * cosine) / a0;
            b2 = (1.0f - alpha * A) / a0;
            a1 = b1;
            a2 = (1.0f - alpha / A) / a0;
            return;
        }
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        if (kind == Kind::LowShelf) {
            a0 = (A + 1.0f) + (A - 1.0f) * cosine + twoSqrtAAlpha;
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosine + twoSqrtAAlpha) / a0;
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosine) / a0;
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosine - twoSqrtAAlpha) / a0;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosine) / a0;
            a2 = ((A + 1.0f) + (A - 1.0f) * cosine - twoSqrtAAlpha) / a0;
            return;
        }
        a0 = (A + 1.0f) - (A - 1.0f) * cosine + twoSqrtAAlpha;
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cosine + twoSqrtAAlpha) / a0;
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosine) / a0;
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cosine - twoSqrtAAlpha) / a0;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosine) / a0;
        a2 = ((A + 1.0f) - (A - 1.0f) * cosine - twoSqrtAAlpha) / a0;
    }

    float z1 = 0.0f;
    float z2 = 0.0f;
};

struct AllpassDelay {
    void prepare(int samples, float feedbackGain) {
        size = juce::jmax(1, samples);
        buffer.assign((size_t)size, 0.0f);
        index = 0;
        gain = juce::jlimit(-0.95f, 0.95f, feedbackGain);
    }

    void clear() noexcept {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        index = 0;
    }

    float process(float x) noexcept {
        if (buffer.empty()) {
            return x;
        }
        const float delayed = buffer[(size_t)index];
        // Unity-gain Schroeder allpass. A direct coefficient of -1 (instead of -gain)
        // stacks to a loud high-frequency scream when several stages are cascaded.
        const float y = delayed - gain * x;
        buffer[(size_t)index] = x + delayed * gain;
        if (++index >= size) {
            index = 0;
        }
        return y;
    }

    std::vector<float> buffer;
    int index = 0;
    int size = 0;
    float gain = 0.6f;
};

struct FirstAllpass {
    void setFrequency(float frequency, double sampleRate) noexcept {
        const float hz = juce::jlimit(20.0f, (float)(sampleRate * 0.45), frequency);
        const float t = std::tan(juce::MathConstants<float>::pi * hz / (float)sampleRate);
        coefficient = (1.0f - t) / (1.0f + t);
    }

    float process(float x) noexcept {
        const float y = coefficient * x + state;
        state = x - coefficient * y;
        return y;
    }

    void clear() noexcept { state = 0.0f; }
    float coefficient = 0.0f;
    float state = 0.0f;
};

class PitchTracker {
   public:
    void prepare(double sampleRate);
    void clear();
    bool push(float sample, float& periodSamples, float& confidence);

   private:
    double sampleRate = 48000.0;
    std::vector<float> ring;
    std::vector<float> difference;
    int write = 0;
    int since = 0;
    int filled = 0;
    int hop = 512;
};

struct HarmonizerSettings {
    int mode = 1;
    float intervalSemitones = 0.0f;
    float mix = 50.0f;
    float latencyMs = 22.0f;
    bool formantLock = true;
};

class Harmonizer {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const HarmonizerSettings& settings);
    int latencySamples(float latencyMs) const;

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        DelayLine dryLeft;
        DelayLine dryRight;
        DelayLine padLeft;
        DelayLine padRight;
        Biquad tiltLeft;
        Biquad tiltRight;
        PitchTracker tracker;
        float readHead[4]{};
        float period = 256.0f;
        float confidence = 0.0f;
        float lastSemitones = 100.0f;
        bool primed = false;
    };

    float readAt(const DelayLine& line, float position) const;
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct DropSettings {
    float semitones = -2.0f;
    float mix = 100.0f;
    float latencyMs = 20.0f;
};

// Polyphonic transpose down. Two read heads play the input at one ratio, so
// every partial of a chord moves together. The delay stays inside 8–20 ms.
class DropPitch {
   public:
    DropPitch();
    ~DropPitch();
    DropPitch(const DropPitch&) = delete;
    DropPitch& operator=(const DropPitch&) = delete;
    DropPitch(DropPitch&&) noexcept;
    DropPitch& operator=(DropPitch&&) noexcept;

    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const DropSettings& settings);
    int latencySamples(float latencyMs) const;

   private:
    struct State;
    std::unique_ptr<State> state;
};

struct OctaverSettings {
    int mode = 2;
    float mix1 = 55.0f;
    float mix2 = 30.0f;
    float highpassHz = 90.0f;
    float trigger = 35.0f;
};

class Octaver {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const OctaverSettings& settings);

   private:
    struct Voice {
        OnePole detector[3];
        OnePole subLowpass[2];
        Biquad highpass[2];
        float envelope = 0.0f;
        float gate = 0.0f;
        float period = 400.0f;
        int sinceEdge = 0;
        bool schmitt = false;
        float phase1 = 0.0f;
        float phase2 = 0.0f;
        float lastHighpass = -1.0f;
        float lastCutoff = -1.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct PlateSettings {
    float decay = 1.4f;
    float predelayMs = 8.0f;
    float mix = 22.0f;
    float damping = 35.0f;
    float tone = 62.0f;
};

class PlateReverb {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const PlateSettings& settings);

   private:
    struct Voice {
        DelayLine lines[8];
        DelayLine predelay;
        OnePole damp[8];
        OnePole toneLp[2];
        OnePole toneHp[2];
        DcBlock dc[2];
        float lengths[8]{};
        float phase = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct HallSettings {
    float decay = 3.6f;
    float predelayMs = 18.0f;
    float width = 70.0f;
    float size = 55.0f;
    float tone = 48.0f;
    float mix = 24.0f;
};

class HallReverb {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const HallSettings& settings);

   private:
    struct Voice {
        DelayLine combL[4];
        DelayLine combR[4];
        DelayLine predelayL;
        DelayLine predelayR;
        AllpassDelay diffuseL[4];
        AllpassDelay diffuseR[4];
        float dampL[4]{};
        float dampR[4]{};
        float lengthL[4]{};
        float lengthR[4]{};
        OnePole toneLp[2];
        OnePole toneHp[2];
        DcBlock dc[2];
        float phase = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct ShimmerSettings {
    float decay = 4.5f;
    float mix = 28.0f;
    int pitchMode = 0;
    float shimmer = 45.0f;
    float damping = 40.0f;
};

class ShimmerReverb {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const ShimmerSettings& settings);

   private:
    struct Voice {
        DelayLine combL[4];
        DelayLine combR[4];
        AllpassDelay diffuseL[2];
        AllpassDelay diffuseR[2];
        DelayLine pitch;
        float dampL[4]{};
        float dampR[4]{};
        float readHead[4]{};
        bool primed = false;
        OnePole tone[2];
        DcBlock dc[2];
        float feedbackSample = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
    float readPitch(const DelayLine& line, float position) const;
};

struct SpringSettings {
    float decay = 1.1f;
    float mix = 28.0f;
    float boing = 45.0f;
    float damping = 55.0f;
};

class SpringReverb {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const SpringSettings& settings);

   private:
    struct Voice {
        AllpassDelay dispersion[6];
        DelayLine taps;
        DelayLine chirp;
        Biquad boing[2];
        OnePole damp;
        OnePole split;
        OnePole tone;
        DcBlock dc;
        float lastBoing = -1.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct GranularSettings {
    float grainMs = 140.0f;
    float density = 12.0f;
    float feedback = 62.0f;
    float diffusion = 70.0f;
    float tone = 40.0f;
    float mix = 30.0f;
};

class GranularReverb {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const GranularSettings& settings);

   private:
    struct Grain {
        float position = 0.0f;
        int age = 0;
        int length = 1000;
        float pan = 0.0f;
        bool swap = false;
        bool active = false;
    };
    struct Voice {
        DelayLine left;
        DelayLine right;
        std::array<Grain, 24> grains;
        OnePole tone[2];
        float spawn = 0.0f;
        juce::Random random;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct TapeDelaySettings {
    float timeMs = 120.0f;
    float feedback = 35.0f;
    float mix = 28.0f;
    float wow = 30.0f;
    float saturation = 28.0f;
    float tone = 45.0f;
};

class TapeDelay {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const TapeDelaySettings& settings);

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        OnePole lowpass[2];
        OnePole highpass[2];
        float phase = 0.0f;
        float time = 1000.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct PingPongSettings {
    float timeMs = 375.0f;
    bool sync = false;
    int division = 3;
    float feedback = 35.0f;
    float mix = 28.0f;
    float depth = 100.0f;
    float highpassHz = 80.0f;
    float lowpassHz = 9000.0f;
    double bpm = 120.0;
};

class PingPongDelay {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const PingPongSettings& settings);

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        Biquad hp[2];
        Biquad lp[2];
        float time = 1000.0f;
        float lastHp = -1.0f;
        float lastLp = -1.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct DarkDelaySettings {
    float timeMs = 480.0f;
    float feedback = 72.0f;
    float darkness = 65.0f;
    float mix = 30.0f;
    float tone = 35.0f;
};

class DarkDelay {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const DarkDelaySettings& settings);

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        OnePole dark[2];
        OnePole highpass[2];
        OnePole tone[2];
        float time = 2000.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct TapeExtremeSettings {
    float timeMs = 280.0f;
    float feedback = 48.0f;
    float saturation = 70.0f;
    float wow = 55.0f;
    float cents = 12.0f;
    float mix = 32.0f;
};

class TapeExtremeDelay {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const TapeExtremeSettings& settings);

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        DelayLine pitch;
        float readHead[4]{};
        bool primed = false;
        float phase = 0.0f;
        float time = 2000.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
    float readPitch(const DelayLine& line, float position) const;
};

struct ReverseDelaySettings {
    float timeMs = 600.0f;
    int mode = 1;
    float feedback = 45.0f;
    float mix = 40.0f;
    float tone = 50.0f;
};

class ReverseDelay {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const ReverseDelaySettings& settings);

   private:
    struct Voice {
        std::array<std::vector<float>, 2> loopL;
        std::array<std::vector<float>, 2> loopR;
        DelayLine forwardL;
        DelayLine forwardR;
        OnePole tone[2];
        int record = 0;
        int bank = 0;
        int length = 1000;
        float fade = 1.0f;
        float time = 2000.0f;
        int lastMode = -1;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct EnsembleSettings {
    float rate = 0.6f;
    float depth = 35.0f;
    float mix = 40.0f;
    float width = 75.0f;
};

class EnsembleChorus {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const EnsembleSettings& settings);

   private:
    struct Voice {
        DelayLine delay;
        OnePole tone;
        float phase = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct LeadChorusSettings {
    float rate = 2.4f;
    float depth = 55.0f;
    float mix = 45.0f;
    float vibe = 30.0f;
};

class LeadChorus {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const LeadChorusSettings& settings);

   private:
    struct Voice {
        DelayLine delay;
        float phase = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct Phaser4Settings {
    float rate = 0.4f;
    float depth = 60.0f;
    float mix = 50.0f;
    float centerHz = 900.0f;
};

class Phaser4 {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const Phaser4Settings& settings);

   private:
    struct Voice {
        FirstAllpass stages[2][4];
        float feedback[2]{};
        float phase = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct Phaser8Settings {
    float rate = 1.2f;
    float depth = 70.0f;
    float mix = 55.0f;
    float q = 2.8f;
    float sweep = 65.0f;
};

class Phaser8 {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const Phaser8Settings& settings);

   private:
    struct Voice {
        FirstAllpass stages[2][8];
        float feedback[2]{};
        float phase = 0.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct FlangerSettings {
    float rate = 0.35f;
    float depth = 45.0f;
    float mix = 35.0f;
    float feedback = 18.0f;
    float ring = 0.0f;
    bool hard = false;
};

class Flanger {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const FlangerSettings& settings);

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        Biquad ring[2];
        DcBlock dc[2];
        float phase = 0.0f;
        float lastRingHz = -1.0f;
        int ringCountdown = 0;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct CompressorSettings {
    float thresholdDb = -18.0f;
    float attackMs = 5.0f;
    float releaseMs = 80.0f;
    float ratio = 4.0f;
    float kneeDb = 0.0f;
    float makeupDb = 0.0f;
    float sidechainHz = 100.0f;
    bool sidechain = false;
    bool peak = true;
};

class Compressor {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const CompressorSettings& settings);

   private:
    struct Voice {
        Biquad sidechain[2];
        float envelope = 0.0f;
        float rms = 0.0f;
        float lastSidechain = -1.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct ParametricEqSettings {
    float frequency[4]{120.0f, 400.0f, 1800.0f, 6500.0f};
    float gain[4]{0.0f, 0.0f, 0.0f, 0.0f};
    float q[4]{0.7f, 1.0f, 1.0f, 0.7f};
};

class ParametricEq {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const ParametricEqSettings& settings);

   private:
    struct Voice {
        Biquad bands[2][4];
        float lastF[4]{-1, -1, -1, -1};
        float lastG[4]{};
        float lastQ[4]{};
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct ToneEqSettings {
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
};

class ToneEq {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const ToneEqSettings& settings);

   private:
    struct Voice {
        Biquad low[2];
        Biquad mid[2];
        Biquad high[2];
        float lastLow = 100.0f;
        float lastMid = 100.0f;
        float lastHigh = 100.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct DynamicEqSettings {
    float frequency[4]{120.0f, 400.0f, 2000.0f, 6000.0f};
    float q[4]{0.7f, 1.0f, 1.0f, 0.7f};
    float threshold[4]{-18.0f, -18.0f, -18.0f, -18.0f};
    float ratio[4]{2.0f, 2.0f, 2.0f, 2.0f};
};

class DynamicEq {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const DynamicEqSettings& settings);

   private:
    struct Voice {
        Biquad filters[2][4];
        float envelope[4]{};
        float lastF[4]{-1, -1, -1, -1};
        float lastQ[4]{};
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct GateSettings {
    float thresholdDb = -45.0f;
    float attackMs = 2.0f;
    float releaseMs = 80.0f;
    float rangeDb = -60.0f;
    float holdMs = 25.0f;
    float lookaheadMs = 8.0f;
    bool sidechain = false;
    float sidechainHz = 100.0f;
};

class NoiseGate {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, float* left, float* right, int numSamples, const GateSettings& settings);
    int latencySamples(float lookaheadMs) const;

   private:
    struct Voice {
        DelayLine left;
        DelayLine right;
        Biquad sidechain[2];
        float envelope = 0.0f;
        float gain = 1.0f;
        int hold = 0;
        bool open = false;
        float lastSidechain = -1.0f;
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
};

struct TunerSettings {
    int mode = 0;
    bool enabled = true;
};

class Tuner {
   public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(int voice, const float* left, const float* right, int numSamples, const TunerSettings& settings, Fx::TunerSnapshot& snapshot);

   private:
    struct Voice {
        PitchTracker tracker;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> fftData;
        std::vector<float> window;
        int filled = 0;
        float strobe = 0.0f;
        Fx::TunerSnapshot held{};
    };
    double sampleRate = 48000.0;
    std::array<Voice, kVoices> voices;
    void analyseMono(Voice& voice, float period, float confidence, float rms, Fx::TunerSnapshot& snapshot);
    void analysePoly(Voice& voice, Fx::TunerSnapshot& snapshot);
};
}  // namespace FxDsp
