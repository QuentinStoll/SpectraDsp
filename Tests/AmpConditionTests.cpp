#include <JuceHeader.h>

#include <cmath>
#include <vector>

#include "AmpCondition.h"
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
    return static_cast<float>(std::sqrt(q1 * q1 + q2 * q2 - coeff * q1 * q2));
}

void render(AmpCondition& condition, float* audio, int count, int block) {
    for (int offset = 0; offset < count;) {
        const int frames = std::min(block, count - offset);
        condition.processPre(audio + offset, frames);
        condition.processPost(audio + offset, frames);
        offset += frames;
    }
}

class AmpConditionUnitTests : public juce::UnitTest {
   public:
    AmpConditionUnitTests()
        : juce::UnitTest("AmpCondition", "Profiler") {}

    void runTest() override {
        runCase("condition off stays transparent", [this] { testOff(); });
        runCase("raw floor is hum plus hiss", [this] { testRawFloor(); });
        runCase("death sag yields on a loud note", [this] { testDeathSag(); });
        runCase("condition copies do not share a seed", [this] { testSeeds(); });
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

    void testOff() {
        AmpCondition condition;
        condition.prepare(48000.0, 1);
        condition.set(AmpCondition::kOff, 1.0f);
        std::vector<float> audio(2048);
        std::vector<float> dry(2048);
        for (int index = 0; index < 2048; ++index) {
            audio[static_cast<std::size_t>(index)] = 0.2f * std::sin(0.07f * static_cast<float>(index));
        }
        dry = audio;
        render(condition, audio.data(), 2048, 128);
        float error = 0.0f;
        for (int index = 0; index < 2048; ++index) {
            error = std::max(error, std::abs(audio[static_cast<std::size_t>(index)] - dry[static_cast<std::size_t>(index)]));
        }
        expect(error == 0.0f, "An off profile should not touch the amp signal.");

        condition.set(AmpCondition::kRaw, 0.0f);
        audio = dry;
        render(condition, audio.data(), 2048, 128);
        error = 0.0f;
        for (int index = 0; index < 2048; ++index) {
            error = std::max(error, std::abs(audio[static_cast<std::size_t>(index)] - dry[static_cast<std::size_t>(index)]));
        }
        expect(error == 0.0f, "A zero amount should not touch the amp signal.");
    }

    void testRawFloor() {
        constexpr double rate = 48000.0;
        constexpr int count = 48000;
        AmpCondition condition;
        condition.prepare(rate, 7);
        condition.set(AmpCondition::kRaw, 1.0f);
        std::vector<float> audio(static_cast<std::size_t>(count), 0.0f);
        render(condition, audio.data(), count, 256);

        double energy = 0.0;
        int finite = 0;
        for (int index = 8000; index < count; ++index) {
            const float sample = audio[static_cast<std::size_t>(index)];
            expect(std::isfinite(sample), "Raw floor should stay finite.");
            if (!std::isfinite(sample)) {
                return;
            }
            energy += static_cast<double>(sample) * sample;
            ++finite;
            expect(std::abs(sample) < 0.2f, "Raw floor should stay under the note.");
        }
        const float rms = static_cast<float>(std::sqrt(energy / std::max(1, finite)));
        const float hum = goertzel(audio.data() + 8000, count - 8000, rate, 50.0);
        const float elsewhere = goertzel(audio.data() + 8000, count - 8000, rate, 400.0);
        expect(rms > 0.001f, "Silence should pick up the raw floor. rms " + juce::String(rms, 5));
        expect(hum > elsewhere * 4.0f, "The floor should carry 50 Hz hum. hum/400 " + juce::String(hum / std::max(elsewhere, 1.0e-6f), 2));
    }

    void testDeathSag() {
        constexpr int count = 24000;
        AmpCondition condition;
        condition.prepare(48000.0, 3);
        condition.set(AmpCondition::kDeath, 1.0f);
        std::vector<float> audio(static_cast<std::size_t>(count), 0.6f);
        render(condition, audio.data(), count, 256);
        double sum = 0.0;
        constexpr int tail = 2000;
        for (int index = count - tail; index < count; ++index) {
            const float sample = audio[static_cast<std::size_t>(index)];
            expect(std::isfinite(sample), "Sag should stay finite.");
            sum += sample;
        }
        const float mean = static_cast<float>(sum / tail);
        expect(mean < 0.52f, "A loud note should sag on the Death profile. mean " + juce::String(mean, 3));
        expect(mean > 0.25f, "Sag should leave the note in place. mean " + juce::String(mean, 3));
    }

    void testSeeds() {
        AmpCondition left;
        AmpCondition right;
        left.prepare(48000.0, 11);
        right.prepare(48000.0, 29);
        left.set(AmpCondition::kRaw, 1.0f);
        right.set(AmpCondition::kRaw, 1.0f);
        std::vector<float> a(4096, 0.0f);
        std::vector<float> b(4096, 0.0f);
        render(left, a.data(), 4096, 256);
        render(right, b.data(), 4096, 256);
        float delta = 0.0f;
        for (int index = 0; index < 4096; ++index) {
            delta = std::max(delta, std::abs(a[static_cast<std::size_t>(index)] - b[static_cast<std::size_t>(index)]));
        }
        expect(delta > 1.0e-5f, "Two amp copies should not share the same floor.");
    }
};

AmpConditionUnitTests ampConditionUnitTests;

}  // namespace
}  // namespace profiler_tests
