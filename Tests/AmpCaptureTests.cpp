#include <JuceHeader.h>

#include <cmath>
#include <vector>

#include "AmpCapture.h"
#include "PluginProcessor.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {

std::vector<float> makeReamp(int frames) {
    juce::Random random(7);
    std::vector<float> signal(static_cast<std::size_t>(frames), 0.0f);
    for (int sample = 0; sample < AmpCapture::kValidationFrames; ++sample) {
        signal[static_cast<std::size_t>(sample)] = 0.3f * (random.nextFloat() * 2.0f - 1.0f);
    }
    for (const auto blip : AmpCapture::kBlips) {
        signal[static_cast<std::size_t>(blip)] = 1.0f;
    }
    for (int sample = AmpCapture::kBlips.back() + AmpCapture::kLookback; sample < frames; ++sample) {
        signal[static_cast<std::size_t>(sample)] = 0.3f * (random.nextFloat() * 2.0f - 1.0f);
    }
    if (frames == AmpCapture::kStandardFrames) {
        std::copy_n(signal.begin(), AmpCapture::kValidationFrames,
                    signal.end() - AmpCapture::kValidationFrames);
    }
    return signal;
}

// A filtered, saturated, noisy return that arrives `delay` samples late.
std::vector<float> makeReturn(const std::vector<float>& sent, int delay) {
    juce::Random random(11);
    std::vector<float> recorded(sent.size() + AmpCapture::kTailFrames, 0.0f);
    float state = 0.0f;
    const auto offset = static_cast<std::size_t>(delay);
    for (std::size_t sample = 0; sample < recorded.size(); ++sample) {
        const auto source = sample >= offset && sample - offset < sent.size() ? sent[sample - offset] : 0.0f;
        state = 0.7f * state + 0.3f * std::tanh(2.0f * source);
        recorded[sample] = 0.5f * state + 1.0e-4f * (random.nextFloat() * 2.0f - 1.0f);
    }
    return recorded;
}

class AmpCaptureUnitTests : public juce::UnitTest {
   public:
    AmpCaptureUnitTests()
        : juce::UnitTest("AmpCapture", "Profiler") {}

    void runTest() override {
        runCase("amp capture finds the blip latency", [this] {
            testFindsLatency();
        });

        runCase("amp capture rejects a silent return", [this] {
            testSilentReturn();
        });

        runCase("amp capture checks the replicate and writes a nam output", [this] {
            testReplicateAndOutput();
        });

        runCase("amp capture records a standalone loopback", [this] {
            testStandaloneLoopback();
        });
    }

   private:
    template <typename Function>
    void runCase(const juce::String& testCaseName, Function&& testCase) {
        if (requestedTestCase.isNotEmpty() && requestedTestCase != testCaseName) {
            return;
        }

        beginTest(testCaseName);
        testCase();
    }

    void testFindsLatency() {
        const auto sent = makeReamp(AmpCapture::kRoundTripFrames);
        const auto analysis = AmpCapture::analyse(sent, makeReturn(sent, 777));
        expect(analysis.detected, "The blips should trigger above the noise.");
        expectEquals(analysis.latency, 777);
        expectEquals(analysis.shift, 776);
        expectEquals(analysis.blipLatency[0], 777);
        expectEquals(analysis.blipLatency[1], 777);
        expect(!analysis.clipped, "A half-scale return should not clip.");
        expect(!analysis.replicateChecked, "The round trip test is too short for the replicate check.");
    }

    void testSilentReturn() {
        const auto sent = makeReamp(AmpCapture::kRoundTripFrames);
        const std::vector<float> recorded(sent.size() + AmpCapture::kTailFrames, 0.0f);
        const auto analysis = AmpCapture::analyse(sent, recorded);
        expect(!analysis.detected, "A silent return has no latency.");
        expectEquals(analysis.returnPeak, 0.0f);
    }

    void testReplicateAndOutput() {
        const auto sent = makeReamp(AmpCapture::kStandardFrames);
        auto recorded = makeReturn(sent, 300);
        auto analysis = AmpCapture::analyse(sent, recorded);
        expect(analysis.detected, "The full capture should find the blips.");
        expectEquals(analysis.latency, 300);
        expect(analysis.replicateChecked, "The standard signal has two validation passes.");
        expect(analysis.replicateEsr < 1.0e-4, "Identical passes should match, got " + juce::String(analysis.replicateEsr));

        const auto aligned = AmpCapture::align(recorded, analysis.shift, AmpCapture::kStandardFrames);
        expectEquals(static_cast<int>(aligned.size()), AmpCapture::kStandardFrames);
        expect(std::abs(aligned[AmpCapture::kBlips[0]]) < 1.0e-3f, "One sample of margin stays before the response.");
        expect(aligned[AmpCapture::kBlips[0] + 1] > 0.05f, "The response starts right after the margin.");

        const auto file = juce::File(PROFILER_TEST_TEMP_ROOT).getChildFile("amp-capture-output.wav");
        file.getParentDirectory().createDirectory();
        juce::String error;
        expect(AmpCapture::writeWav(file, aligned, &error), error);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        expect(reader != nullptr, "The output should read back.");
        if (reader != nullptr) {
            expectEquals(static_cast<int>(reader->lengthInSamples), AmpCapture::kStandardFrames);
            expectEquals(static_cast<int>(reader->numChannels), 1);
            expectEquals(static_cast<int>(reader->bitsPerSample), 24);
            expectEquals(reader->sampleRate, AmpCapture::kSampleRate);
        }
        reader.reset();
        file.deleteFile();

        const auto tailStart = recorded.size() - AmpCapture::kTailFrames - AmpCapture::kValidationFrames;
        for (auto sample = tailStart; sample < recorded.size(); ++sample) {
            recorded[sample] *= 0.8f;
        }
        analysis = AmpCapture::analyse(sent, recorded);
        expect(analysis.replicateEsr > AmpCapture::kReplicateEsrLimit,
               "A level change between passes should fail, got " + juce::String(analysis.replicateEsr));
    }

    void testStandaloneLoopback() {
        {
            ProfilerAudioProcessor plugin;
            juce::String error;
            expect(!plugin.beginAmpCapture(makeReamp(AmpCapture::kRoundTripFrames), &error),
                   "Hosts own the device, so capture stays in the standalone.");
        }

        juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
        ProfilerAudioProcessor processor;
        juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
        constexpr int blockSize = 512;
        constexpr std::size_t loopDelay = 1500;

        juce::String error;
        processor.setRateAndBufferSizeDetails(44100.0, blockSize);
        processor.prepareToPlay(44100.0, blockSize);
        expect(!processor.beginAmpCapture(makeReamp(AmpCapture::kRoundTripFrames), &error),
               "The NAM signal needs a 48 kHz device.");

        processor.setRateAndBufferSizeDetails(AmpCapture::kSampleRate, blockSize);
        processor.prepareToPlay(AmpCapture::kSampleRate, blockSize);
        const auto reamp = makeReamp(AmpCapture::kRoundTripFrames);
        expect(processor.beginAmpCapture(reamp, &error), error);

        std::vector<float> played;
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block < 2000 && !processor.ampCaptureFinished(); ++block) {
            for (int sample = 0; sample < blockSize; ++sample) {
                const auto time = played.size() + static_cast<std::size_t>(sample);
                const auto value = time >= loopDelay ? 0.5f * played[time - loopDelay] : 0.0f;
                buffer.setSample(0, sample, value);
                buffer.setSample(1, sample, value);
            }
            processor.processBlock(buffer, midi);
            played.insert(played.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + blockSize);
        }
        expect(processor.ampCaptureFinished(), "The capture should end after the reamp and its tail.");

        std::vector<float> sent;
        std::vector<float> recorded;
        expect(processor.takeAmpCapture(sent, recorded), "The finished capture should hand over its audio.");
        expectEquals(static_cast<int>(recorded.size()), AmpCapture::kRoundTripFrames + AmpCapture::kTailFrames);
        const auto analysis = AmpCapture::analyse(sent, recorded);
        expect(analysis.detected, "The loopback blips should come back.");
        expectEquals(analysis.latency, static_cast<int>(loopDelay));

        buffer.clear();
        for (int sample = 0; sample < blockSize; ++sample) {
            buffer.setSample(0, sample, 0.25f);
        }
        processor.processBlock(buffer, midi);
        expect(!processor.isAmpCaptureRunning(), "The processor should return to normal playback.");

        expect(processor.beginAmpCapture(reamp, &error), error);
        processor.stopAmpCapture();
        processor.processBlock(buffer, midi);
        expect(processor.releaseStoppedAmpCapture(), "Stop is acknowledged by the audio thread.");
        expect(!processor.isAmpCaptureRunning(), "A stopped capture frees the session.");
    }
};

static AmpCaptureUnitTests ampCaptureUnitTests;

}  // namespace
}  // namespace profiler_tests
