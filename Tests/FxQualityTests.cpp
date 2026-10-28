#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Fx/FxDsp.h"
#include "Fx/PedalDrive.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {

float goertzel(const float* data, int count, double sampleRate, double frequency) {
    const double omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const double coeff = 2.0 * std::cos(omega);
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;
    for (int index = 0; index < count; ++index) {
        q0 = coeff * q1 - q2 + data[index];
        q2 = q1;
        q1 = q0;
    }
    return (float)std::sqrt(q1 * q1 + q2 * q2 - coeff * q1 * q2);
}

class FxQualityUnitTests : public juce::UnitTest {
   public:
    FxQualityUnitTests()
        : juce::UnitTest("FxQuality", "Profiler") {}

    void runTest() override {
        runCase("drive screamer harmonics and alias floor", [this] { testScreamer(); });
        runCase("drive latency stays live", [this] { testDriveLatency(); });
        runCase("delay mix keeps dry unity", [this] { testDelayDry(); });
        runCase("plate holds the onset", [this] { testPlateOnset(); });
        runCase("reverse sine stays level", [this] { testReverseLevel(); });
        runCase("harmonizer octave accuracy", [this] { testHarmonizer(); });
        runCase("drop chord stays a chord", [this] { testDropChord(); });
        runCase("ping pong holds a whole note", [this] { testPingPong(); });
        runCase("hall tail is stereo", [this] { testHallStereo(); });
        runCase("dynamic eq leaves the highs", [this] { testDynamicEq(); });
    }

   private:
    template <typename Function>
    void runCase(const juce::String& name, Function&& function) {
        if (requestedTestCase.isNotEmpty() && requestedTestCase != name) {
            return;
        }
        beginTest(name);
        function();
    }

    void testScreamer() {
        constexpr double rate = 48000.0;
        constexpr int count = 48000;
        PedalDrive drive;
        drive.prepare(rate, 256);
        std::vector<float> audio((size_t)count);
        for (int index = 0; index < count; ++index) {
            audio[(size_t)index] = 0.25f * std::sin(juce::MathConstants<float>::twoPi * 1000.0f * (float)index / (float)rate);
        }
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            drive.process(audio.data() + offset, block, PedalDrive::kScreamer, 4.0f, 65.0f, 0.0f);
            offset += block;
        }

        const int skip = 8000;
        const float fundamental = goertzel(audio.data() + skip, count - skip, rate, 1000.0);
        const float second = goertzel(audio.data() + skip, count - skip, rate, 2000.0);
        const float seventh = goertzel(audio.data() + skip, count - skip, rate, 7000.0);
        const float alias = goertzel(audio.data() + skip, count - skip, rate, 1500.0);
        expect(fundamental > 0.01f, "Screamer should pass a 1 kHz tone. level " + juce::String(fundamental, 4));
        expect(second > fundamental * 0.01f, "Screamer should produce a second harmonic. H2/H1 " + juce::String(second / fundamental, 4));
        expect(alias < fundamental * 0.001f,
               "Inharmonic residue at 1.5 kHz should sit 60 dB under the fundamental. ratio " +
                   juce::String(alias / fundamental, 6));
        expect(seventh < fundamental * 0.2f, "Upper harmonics should fall, not scream.");
    }

    void testDriveLatency() {
        PedalDrive drive;
        drive.prepare(48000.0, 256);
        expect(drive.latencySamples() < 144, "4x drive latency should stay under 3 ms for live playing. samples " +
                                                  juce::String(drive.latencySamples()));
    }

    void testDelayDry() {
        FxDsp::TapeDelay delay;
        delay.prepare(48000.0, 512);
        std::vector<float> left(512, 0.0f);
        std::vector<float> right(512, 0.0f);
        left[0] = right[0] = 0.5f;
        FxDsp::TapeDelaySettings settings;
        settings.mix = 50.0f;
        settings.timeMs = 200.0f;
        settings.feedback = 0.0f;
        delay.process(0, left.data(), right.data(), 512, settings);
        expect(std::abs(left[0] - 0.5f) < 0.02f, "Mix 50% should leave the direct sample intact. got " + juce::String(left[0], 4));
    }

    void testPlateOnset() {
        FxDsp::PlateReverb plate;
        plate.prepare(48000.0, 256);
        constexpr int count = 48000;
        std::vector<float> left((size_t)count, 0.0f);
        std::vector<float> right((size_t)count, 0.0f);
        left[0] = right[0] = 1.0f;
        FxDsp::PlateSettings settings;
        settings.predelayMs = 0.0f;
        settings.mix = 100.0f;
        settings.decay = 1.4f;
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            plate.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        float early = 0.0f;
        for (int index = 0; index < 800; ++index) {
            early = std::max(early, std::abs(left[(size_t)index]));
        }
        float later = 0.0f;
        for (int index = 2000; index < 8000; ++index) {
            later = std::max(later, std::abs(left[(size_t)index]));
        }
        expect(later > 0.001f, "Plate should produce a tail.");
        expect(early < later * 0.25f, "Plate onset before 17 ms should stay quieter than the body. early " +
                                           juce::String(early, 5) + " later " + juce::String(later, 5));
    }

    void testReverseLevel() {
        FxDsp::ReverseDelay reverse;
        reverse.prepare(48000.0, 256);
        constexpr int count = 48000;
        std::vector<float> left((size_t)count);
        std::vector<float> right((size_t)count);
        for (int index = 0; index < count; ++index) {
            const float sample = 0.4f * std::sin(juce::MathConstants<float>::twoPi * 440.0f * (float)index / 48000.0f);
            left[(size_t)index] = right[(size_t)index] = sample;
        }
        FxDsp::ReverseDelaySettings settings;
        settings.mode = 1;
        settings.mix = 100.0f;
        settings.timeMs = 300.0f;
        settings.feedback = 0.0f;
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            reverse.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        float minRms = 10.0f;
        float maxRms = 0.0f;
        for (int start = 20000; start < count - 480; start += 480) {
            double energy = 0.0;
            for (int index = 0; index < 480; ++index) {
                const float sample = left[(size_t)(start + index)];
                energy += (double)sample * sample;
            }
            const float rms = (float)std::sqrt(energy / 480.0);
            minRms = std::min(minRms, rms);
            maxRms = std::max(maxRms, rms);
        }
        expect(maxRms > 0.02f, "Reverse should produce a signal.");
        expect(minRms > maxRms * 0.5f, "Reverse envelope should stay within 6 dB. min " + juce::String(minRms, 4) +
                                            " max " + juce::String(maxRms, 4));
    }

    void testHarmonizer() {
        FxDsp::Harmonizer harmonizer;
        harmonizer.prepare(48000.0, 256);
        constexpr int count = 72000;
        std::vector<float> left((size_t)count);
        std::vector<float> right((size_t)count);
        for (int index = 0; index < count; ++index) {
            const float sample = 0.4f * std::sin(juce::MathConstants<float>::twoPi * 220.0f * (float)index / 48000.0f);
            left[(size_t)index] = right[(size_t)index] = sample;
        }
        FxDsp::HarmonizerSettings settings;
        settings.mode = 1;
        settings.mix = 100.0f;
        settings.latencyMs = 22.0f;
        settings.formantLock = true;
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            harmonizer.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        const int skip = 24000;
        const int length = count - skip;
        std::vector<float> windowed((size_t)length);
        for (int index = 0; index < length; ++index) {
            const float hann = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float)index / (float)(length - 1)));
            windowed[(size_t)index] = left[(size_t)(skip + index)] * hann;
        }
        const float octave = goertzel(windowed.data(), length, 48000.0, 440.0);
        const float source = goertzel(windowed.data(), length, 48000.0, 220.0);
        float peakHz = 440.0f;
        float peakMag = octave;
        for (int hz = 300; hz <= 600; ++hz) {
            const float mag = goertzel(windowed.data(), length, 48000.0, (double)hz);
            if (mag > peakMag) {
                peakMag = mag;
                peakHz = (float)hz;
            }
        }
        float side = 0.0f;
        int sideHz = 0;
        for (int hz = 100; hz <= 1200; ++hz) {
            if (std::abs(hz - 440) <= 25) {
                continue;
            }
            const float mag = goertzel(windowed.data(), length, 48000.0, (double)hz);
            if (mag > side) {
                side = mag;
                sideHz = hz;
            }
        }
        expect(octave > source, "Octave up at full mix should sit above the dry fundamental.");
        expect(std::abs(peakHz - 440.0f) <= 4.0f,
               "Octave should land within 15 cents of 440 Hz. peak " + juce::String(peakHz, 0));
        expect(side < octave * 0.01f, "Grain sidebands should stay 40 dB down. ratio " +
                                           juce::String(side / juce::jmax(1.0e-8f, octave), 5) + " at " +
                                           juce::String(sideHz));
    }

    struct DropWindowScore {
        int latency = 0;
        float worstCents = 0.0f;
        float leftoverRatio = 0.0f;
        float weakNote = 1.0f;
        float envRatio = 0.0f;
        float maxRms = 0.0f;
        float peakHz[3] = {};
        float expectedHz[3] = {};
        bool finite = true;

        bool passed() const {
            return finite && worstCents <= 15.0f && leftoverRatio < 0.2f && weakNote > 0.25f && envRatio > 0.5f && maxRms > 0.02f;
        }
    };

    DropWindowScore measureDrop(const double notes[3], float semitones) const {
        constexpr double rate = 48000.0;
        constexpr int count = 96000;
        DropWindowScore score;
        FxDsp::DropPitch drop;
        drop.prepare(rate, 256);
        score.latency = drop.latencySamples(20.0f);
        const double shift = std::pow(2.0, (double)semitones / 12.0);
        std::vector<float> left((size_t)count);
        std::vector<float> right((size_t)count);
        for (int index = 0; index < count; ++index) {
            float sample = 0.0f;
            for (int note = 0; note < 3; ++note) {
                sample += 0.2f * std::sin(juce::MathConstants<float>::twoPi * (float)notes[note] * (float)index / (float)rate);
            }
            left[(size_t)index] = right[(size_t)index] = sample;
        }
        FxDsp::DropSettings settings;
        settings.semitones = semitones;
        settings.mix = 100.0f;
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            drop.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        const int skip = std::min(count / 2, score.latency + 12000);
        const int length = count - skip;
        if (length < 960) {
            score.finite = false;
            return score;
        }
        float loudest = 0.0f;
        float peaks[3] = {};
        for (int note = 0; note < 3; ++note) {
            const double expected = notes[note] * shift;
            float peakHz = (float)expected;
            float peakMag = 0.0f;
            const double radius = expected < 100.0 ? 4.0 : 20.0;
            const int low = std::max(1, (int)std::floor(expected - radius));
            const int high = (int)std::ceil(expected + radius);
            for (int hz = low; hz <= high; ++hz) {
                const float mag = goertzel(left.data() + skip, length, rate, (double)hz);
                if (mag > peakMag) {
                    peakMag = mag;
                    peakHz = (float)hz;
                }
            }
            peaks[note] = peakMag;
            score.peakHz[note] = peakHz;
            score.expectedHz[note] = (float)expected;
            loudest = std::max(loudest, peakMag);
            if (!(peakMag > 0.0f) || !std::isfinite(peakMag) || peakHz <= 0.0f) {
                score.finite = false;
                continue;
            }
            const float cents = (float)(1200.0 * std::log2(peakHz / expected));
            score.worstCents = std::max(score.worstCents, std::abs(cents));
            const float leftover = goertzel(left.data() + skip, length, rate, notes[note]);
            score.leftoverRatio = std::max(score.leftoverRatio, leftover / peakMag);
        }
        for (int note = 0; note < 3; ++note) {
            if (loudest > 1.0e-8f) {
                score.weakNote = std::min(score.weakNote, peaks[note] / loudest);
            }
        }
        float minRms = 10.0f;
        for (int start = skip; start < count - 960; start += 960) {
            double energy = 0.0;
            for (int index = 0; index < 960; ++index) {
                const float sample = left[(size_t)(start + index)];
                if (!std::isfinite(sample)) {
                    score.finite = false;
                }
                energy += (double)sample * sample;
            }
            const float rms = (float)std::sqrt(energy / 960.0);
            minRms = std::min(minRms, rms);
            score.maxRms = std::max(score.maxRms, rms);
        }
        score.envRatio = score.maxRms > 1.0e-8f ? minRms / score.maxRms : 0.0f;
        return score;
    }

    void testDropChord() {
        constexpr double rate = 48000.0;
        const double notes[3] = {220.0, 220.0 * std::pow(2.0, 4.0 / 12.0), 220.0 * std::pow(2.0, 7.0 / 12.0)};
        const auto tuned = measureDrop(notes, -2.0f);
        const int cap = (int)std::lround(rate * 0.020);
        expect(tuned.latency == cap,
               "Drop latency should sit on the 20 ms ceiling. samples " + juce::String(tuned.latency));
        expect(tuned.passed(),
               "A major chord dropped 2 semitones should stay in tune and level. cents " +
                   juce::String(tuned.worstCents, 1) + " weak " + juce::String(tuned.weakNote, 3) + " env " +
                   juce::String(tuned.envRatio, 3));

        const auto octave = measureDrop(notes, -12.0f);
        expect(octave.finite && octave.maxRms > 0.02f && octave.leftoverRatio < 0.2f && octave.weakNote > 0.4f,
               "An octave drop should keep every note of the chord. weak " + juce::String(octave.weakNote, 3));
        for (int note = 0; note < 3; ++note) {
            expect(std::abs(octave.peakHz[note] - octave.expectedHz[note]) <= 2.0f,
                   "Octave partial should stay within 2 Hz. note " + juce::String(note) + " peak " +
                       juce::String(octave.peakHz[note], 1) + " expected " + juce::String(octave.expectedHz[note], 1));
        }
    }

    void testPingPong() {
        FxDsp::PingPongDelay delay;
        delay.prepare(48000.0, 256);
        constexpr int count = 110000;
        std::vector<float> left((size_t)count, 0.0f);
        std::vector<float> right((size_t)count, 0.0f);
        left[0] = right[0] = 1.0f;
        FxDsp::PingPongSettings settings;
        settings.timeMs = 2000.0f;
        settings.mix = 100.0f;
        settings.feedback = 0.0f;
        settings.sync = false;
        std::vector<float> warmL(48000, 0.0f);
        std::vector<float> warmR(48000, 0.0f);
        for (int offset = 0; offset < 48000;) {
            const int block = std::min(256, 48000 - offset);
            delay.process(0, warmL.data() + offset, warmR.data() + offset, block, settings);
            offset += block;
        }
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            delay.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        int peak = 0;
        float level = 0.0f;
        for (int index = 1000; index < count; ++index) {
            const float sample = std::abs(left[(size_t)index]);
            if (sample > level) {
                level = sample;
                peak = index;
            }
        }
        expect(std::abs(peak - 96000) < 1500, "A 2 s ping-pong should echo near 96000 samples, not clamp to 1 s. peak " +
                                                   juce::String(peak));
    }

    void testHallStereo() {
        FxDsp::HallReverb hall;
        hall.prepare(48000.0, 256);
        constexpr int count = 36000;
        std::vector<float> left((size_t)count, 0.0f);
        std::vector<float> right((size_t)count, 0.0f);
        left[0] = 1.0f;
        FxDsp::HallSettings settings;
        settings.mix = 100.0f;
        settings.width = 100.0f;
        settings.decay = 2.4f;
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            hall.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        double same = 0.0;
        double differ = 0.0;
        for (int index = 4000; index < count; ++index) {
            const float mid = 0.5f * (left[(size_t)index] + right[(size_t)index]);
            const float side = 0.5f * (left[(size_t)index] - right[(size_t)index]);
            same += (double)mid * mid;
            differ += (double)side * side;
        }
        expect(differ > same * 0.02, "Hall tail should keep a stereo difference.");
    }

    void testDynamicEq() {
        FxDsp::DynamicEq eq;
        eq.prepare(48000.0, 256);
        constexpr int count = 24000;
        std::vector<float> left((size_t)count);
        std::vector<float> right((size_t)count);
        for (int index = 0; index < count; ++index) {
            const float low = 0.4f * std::sin(juce::MathConstants<float>::twoPi * 100.0f * (float)index / 48000.0f);
            const float high = 0.2f * std::sin(juce::MathConstants<float>::twoPi * 8000.0f * (float)index / 48000.0f);
            left[(size_t)index] = right[(size_t)index] = low + high;
        }
        std::vector<float> dry = left;
        FxDsp::DynamicEqSettings settings;
        settings.frequency[0] = 120.0f;
        settings.threshold[0] = -50.0f;
        settings.ratio[0] = 8.0f;
        settings.threshold[1] = 0.0f;
        settings.threshold[2] = 0.0f;
        settings.threshold[3] = 0.0f;
        settings.ratio[1] = settings.ratio[2] = settings.ratio[3] = 1.0f;
        for (int offset = 0; offset < count;) {
            const int block = std::min(256, count - offset);
            eq.process(0, left.data() + offset, right.data() + offset, block, settings);
            offset += block;
        }
        const float before = goertzel(dry.data() + 4000, count - 4000, 48000.0, 8000.0);
        const float after = goertzel(left.data() + 4000, count - 4000, 48000.0, 8000.0);
        const float ratio = after / juce::jmax(1.0e-6f, before);
        expect(ratio > 0.944f && ratio < 1.06f, "Low-band compression should leave 8 kHz within 0.5 dB. ratio " +
                                                     juce::String(ratio, 4));
    }
};

FxQualityUnitTests fxQualityUnitTests;

}  // namespace
}  // namespace profiler_tests
