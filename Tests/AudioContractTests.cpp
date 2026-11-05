#include <JuceHeader.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "PluginProcessor.h"
#include "SignalChainLayout.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {

constexpr double kSampleRate = 48000.0;

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

class AudioContractUnitTests : public juce::UnitTest {
   public:
    AudioContractUnitTests()
        : juce::UnitTest("AudioContract", "Profiler") {}

    void runTest() override {
        runCase("audio contract dry latency is zero", [this] { testDryLatency(); });
        runCase("audio contract pedal latency matches the drive", [this] { testPedalLatency(); });
        runCase("audio contract harmonizer latency matches the parameter", [this] { testHarmonizerLatency(); });
        runCase("audio contract drop latency matches the parameter", [this] { testDropLatency(); });
        runCase("audio contract gate lookahead matches the parameter", [this] { testGateLookahead(); });
        runCase("audio contract block boundaries stay continuous", [this] { testBlockBoundaries(); });
        runCase("audio contract bypass settles without a click", [this] { testBypassSettles(); });
        runCase("audio contract oversized block stays finite", [this] { testOversizedBlock(); });
        runCase("audio contract silence stays silent", [this] { testSilence(); });
        runCase("audio contract stages stay realtime", [this] { testRealtime(); });
        runCase("audio contract harmonizer octave stays in tune", [this] { testHarmonizerTune(); });
        runCase("audio contract drop stays in tune", [this] { testDropTune(); });

        if (requestedTestCase.isEmpty()) {
            writeMetrics();
        }
    }

   private:
    struct Metric {
        juce::String name;
        juce::String unit;
        double value = 0.0;
    };

    template <typename Function>
    void runCase(const juce::String& name, Function&& function) {
        if (requestedTestCase.isNotEmpty() && requestedTestCase != name) {
            return;
        }
        beginTest(name);
        function();
    }

    void setParameter(ProfilerAudioProcessor& processor, const juce::String& id, float value) {
        auto* parameter = processor._apvts.getParameter(id);
        expect(parameter != nullptr, "Missing parameter: " + id);
        if (parameter != nullptr) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }

    void silenceUnusedStages(ProfilerAudioProcessor& processor) {
        setParameter(processor, "isEqEnabled", 0.0f);
        setParameter(processor, "isGateEnabled", 0.0f);
        setParameter(processor, "isPedalEnabled", 0.0f);
        setParameter(processor, "isCabEnabled", 0.0f);
        setParameter(processor, "isAmpEnabled", 0.0f);
        setParameter(processor, "gain", 0.0f);
        setParameter(processor, "master", 50.0f);
    }

    void clearMovableSlots(SignalChain::Layout& layout) {
        for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
            layout.clear(SignalChain::chainSlotForMovableIndex(index));
        }
    }

    void setEmptyChain(ProfilerAudioProcessor& processor) {
        SignalChain::Layout layout;
        clearMovableSlots(layout);
        processor.setChainLayout(layout);
        silenceUnusedStages(processor);
    }

    void setSoloStage(ProfilerAudioProcessor& processor, SignalChain::Stage stage) {
        SignalChain::Layout layout;
        clearMovableSlots(layout);
        layout.place(stage, 1);
        processor.setChainLayout(layout);
        silenceUnusedStages(processor);
    }

    void prepareProcessor(ProfilerAudioProcessor& processor, int blockSize) {
        processor.setRateAndBufferSizeDetails(kSampleRate, blockSize);
        processor.prepareToPlay(kSampleRate, blockSize);
    }

    void enableLowDrivePedal(ProfilerAudioProcessor& processor) {
        setParameter(processor, "isPedalEnabled", 1.0f);
        setParameter(processor, "pedalDrive", 0.0f);
        setParameter(processor, "pedalModel", 2.0f);
        setParameter(processor, "pedalTone", 100.0f);
        setParameter(processor, "pedalLevel", 0.0f);
    }

    void fillSine(juce::AudioBuffer<float>& buffer, int globalStart, float frequency, float amplitude) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            const auto phase = juce::MathConstants<double>::twoPi * frequency * (double)(globalStart + sample) / kSampleRate;
            const auto value = amplitude * (float)std::sin(phase);
            buffer.setSample(0, sample, value);
            if (buffer.getNumChannels() > 1) {
                buffer.setSample(1, sample, value);
            }
        }
    }

    bool samplesFinite(const juce::AudioBuffer<float>& buffer, float bound = 8.0f) const {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            const auto* samples = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                if (!std::isfinite(samples[sample]) || !(std::abs(samples[sample]) < bound)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool samplesFinite(const std::vector<float>& samples, float bound = 8.0f) const {
        for (float sample : samples) {
            if (!std::isfinite(sample) || !(std::abs(sample) < bound)) {
                return false;
            }
        }
        return true;
    }

    float maxAbs(const std::vector<float>& samples, int from, int to) const {
        float peak = 0.0f;
        const int begin = std::max(0, from);
        const int end = std::min((int)samples.size(), to);
        for (int index = begin; index < end; ++index) {
            peak = std::max(peak, std::abs(samples[(size_t)index]));
        }
        return peak;
    }

    double correlationAtLag(const std::vector<float>& samples, int lag, float frequency, int start) const {
        double dot = 0.0;
        double energyOut = 0.0;
        double energyRef = 0.0;
        const int begin = std::max(start, 0);
        for (int index = begin; index < (int)samples.size(); ++index) {
            const double ref = std::sin(juce::MathConstants<double>::twoPi * frequency * (double)(index - lag) / kSampleRate);
            const double sample = (double)samples[(size_t)index];
            dot += sample * ref;
            energyOut += sample * sample;
            energyRef += ref * ref;
        }
        if (!(energyOut > 0.0) || !(energyRef > 0.0)) {
            return 0.0;
        }
        return dot / std::sqrt(energyOut * energyRef);
    }

    std::vector<float> renderSine(ProfilerAudioProcessor& processor,
                                  int blockSize,
                                  int count,
                                  float frequency,
                                  float amplitude,
                                  const juce::String& label) {
        std::vector<float> output((size_t)count, 0.0f);
        juce::MidiBuffer midi;
        int written = 0;
        while (written < count) {
            const int block = std::min(blockSize, count - written);
            juce::AudioBuffer<float> buffer(2, block);
            fillSine(buffer, written, frequency, amplitude);
            processor.processBlock(buffer, midi);
            expect(samplesFinite(buffer), label + " should stay finite. peak " + juce::String(buffer.getMagnitude(0, block), 4));
            juce::FloatVectorOperations::copy(output.data() + written, buffer.getReadPointer(0), block);
            written += block;
        }
        return output;
    }

    void processOne(ProfilerAudioProcessor& processor, juce::AudioBuffer<float>& buffer) {
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
    }

    void storeMetric(const juce::String& metricName, const juce::String& unit, double value) {
        if (!(value > 0.0) || !std::isfinite(value)) {
            return;
        }
        for (auto& metric : metrics) {
            if (metric.name == metricName) {
                metric.unit = unit;
                metric.value = value;
                return;
            }
        }
        metrics.add({metricName, unit, value});
    }

    void writeMetrics() {
        juce::Array<juce::var> rows;
        for (const auto& metric : metrics) {
            auto* object = new juce::DynamicObject();
            object->setProperty("name", metric.name);
            object->setProperty("unit", metric.unit);
            object->setProperty("value", metric.value);
            object->setProperty("extra", "48 kHz");
            rows.add(juce::var(object));
        }

        const auto env = juce::SystemStats::getEnvironmentVariable("PROFILER_AUDIO_METRICS", {});
        const juce::File file = env.isNotEmpty()
                                    ? juce::File(env)
                                    : juce::File::getCurrentWorkingDirectory().getChildFile("build/audio-metrics.json");
        file.getParentDirectory().createDirectory();
        file.replaceWithText(juce::JSON::toString(juce::var(rows), true));
    }

    int msToSamples(float milliseconds) const {
        return (int)std::lround((double)milliseconds * 1.0e-3 * kSampleRate);
    }

    void expectEnergyAroundDelay(const std::vector<float>& samples, int latency, const juce::String& label) {
        const float before = maxAbs(samples, 0, latency);
        const float after = maxAbs(samples, latency, latency + 2048);
        expect(before < 0.08f, label + " energy before delay " + juce::String(before, 5) + " latency " + juce::String(latency));
        expect(after > 0.02f, label + " energy after delay " + juce::String(after, 5) + " latency " + juce::String(latency));
    }

    void testDryLatency() {
        ProfilerAudioProcessor processor;
        setEmptyChain(processor);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        juce::AudioBuffer<float> warm(2, block);
        for (int n = 0; n < 16; ++n) {
            warm.clear();
            processOne(processor, warm);
        }

        juce::AudioBuffer<float> buffer(2, block);
        buffer.clear();
        buffer.setSample(0, 0, 0.8f);
        buffer.setSample(1, 0, 0.8f);
        processOne(processor, buffer);

        const int latency = processor.getLatencySamples();
        expect(latency == 0, "Dry latency should be 0. samples " + juce::String(latency));
        expect(samplesFinite(buffer), "Dry impulse should stay finite.");
        const float sample0 = buffer.getSample(0, 0);
        expect(std::abs(sample0 - 0.8f) <= 0.08f, "Dry impulse channel 0 should stay within 0.08. sample " + juce::String(sample0, 4));
        processor.releaseResources();
    }

    void testPedalLatency() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::Pedal);
        enableLowDrivePedal(processor);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        juce::AudioBuffer<float> warm(2, block);
        fillSine(warm, 0, 1000.0f, 0.05f);
        processOne(processor, warm);
        const int latency = processor.getLatencySamples();
        storeMetric("Pedal latency", "samples", (double)latency);
        expect(latency > 0 && latency < 144, "Pedal latency should stay under 144 samples. samples " + juce::String(latency));

        constexpr int count = 24000;
        const auto output = renderSine(processor, block, count, 1000.0f, 0.05f, "Pedal");
        const int start = latency + 2048;
        const double corr = correlationAtLag(output, latency, 1000.0f, start);
        expect(corr > 0.95, "Pedal correlation against delayed dry sine " + juce::String(corr, 4) + " latency " + juce::String(latency));
        processor.releaseResources();
    }

    void testHarmonizerLatency() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::PitchHarmonizer);
        setParameter(processor, "harmOn", 1.0f);
        setParameter(processor, "harmMix", 100.0f);
        setParameter(processor, "harmLatency", 22.0f);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        constexpr int count = 16000;
        const auto output = renderSine(processor, block, count, 220.0f, 0.4f, "Harmonizer");
        const int latency = processor.getLatencySamples();
        storeMetric("Harmonizer latency", "samples", (double)latency);
        expect(latency == 1056, "Harmonizer latency at 22 ms should be 1056. samples " + juce::String(latency));
        expectEnergyAroundDelay(output, latency, "Harmonizer");

        setParameter(processor, "harmLatency", 10.0f);
        juce::AudioBuffer<float> follow(2, block);
        fillSine(follow, count, 220.0f, 0.4f);
        processOne(processor, follow);
        const int followed = processor.getLatencySamples();
        expect(followed == msToSamples(10.0f),
               "Harmonizer latency should follow 10 ms. samples " + juce::String(followed));
        processor.releaseResources();
    }

    void testDropLatency() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::PitchDrop);
        setParameter(processor, "dropOn", 1.0f);
        setParameter(processor, "dropMix", 100.0f);
        setParameter(processor, "dropSemi", -2.0f);
        setParameter(processor, "dropLatency", 20.0f);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        constexpr int count = 16000;
        const auto output = renderSine(processor, block, count, 220.0f, 0.4f, "Drop");
        const int latency = processor.getLatencySamples();
        storeMetric("Drop latency", "samples", (double)latency);
        expect(latency == 960, "Drop latency at 20 ms should be 960. samples " + juce::String(latency));
        expectEnergyAroundDelay(output, latency, "Drop");

        setParameter(processor, "dropLatency", 12.0f);
        juce::AudioBuffer<float> follow(2, block);
        fillSine(follow, count, 220.0f, 0.4f);
        processOne(processor, follow);
        const int followed = processor.getLatencySamples();
        expect(followed == msToSamples(12.0f), "Drop latency should follow 12 ms. samples " + juce::String(followed));
        processor.releaseResources();
    }

    void testGateLookahead() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::NoiseGate);
        setParameter(processor, "ngtOn", 1.0f);
        setParameter(processor, "ngtLook", 8.0f);
        setParameter(processor, "ngtThr", -80.0f);
        setParameter(processor, "isGateEnabled", 0.0f);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        constexpr int count = 24000;
        auto output = renderSine(processor, block, count, 1000.0f, 0.5f, "Gate 8 ms");
        const int latency8 = processor.getLatencySamples();
        storeMetric("Gate lookahead", "samples", (double)latency8);
        expect(latency8 == msToSamples(8.0f), "Gate lookahead at 8 ms. samples " + juce::String(latency8));
        const double corr8 = correlationAtLag(output, latency8, 1000.0f, latency8 + 2048);
        expect(corr8 > 0.95, "Gate correlation at 8 ms " + juce::String(corr8, 4) + " latency " + juce::String(latency8));

        setParameter(processor, "ngtLook", 15.0f);
        output = renderSine(processor, block, count, 1000.0f, 0.5f, "Gate 15 ms");
        const int latency15 = processor.getLatencySamples();
        expect(latency15 == msToSamples(15.0f), "Gate lookahead at 15 ms. samples " + juce::String(latency15));
        const double corr15 = correlationAtLag(output, latency15, 1000.0f, latency15 + 2048);
        expect(corr15 > 0.95, "Gate correlation at 15 ms " + juce::String(corr15, 4) + " latency " + juce::String(latency15));
        processor.releaseResources();
    }

    void expectContinuous(const std::vector<float>& samples, float amplitude, const juce::String& label) {
        expect(samplesFinite(samples), label + " should stay finite.");
        const double maxDelta = 4.0 * (double)amplitude * juce::MathConstants<double>::twoPi * 1000.0 / kSampleRate;
        float worst = 0.0f;
        for (int index = 1; index < (int)samples.size(); ++index) {
            const float delta = std::abs(samples[(size_t)index] - samples[(size_t)index - 1]);
            worst = std::max(worst, delta);
        }
        expect((double)worst <= maxDelta,
               label + " block boundary jump " + juce::String(worst, 5) + " cap " + juce::String(maxDelta, 5));
    }

    std::vector<float> renderBlockSizes(ProfilerAudioProcessor& processor, float amplitude) {
        const int sizes[] = {32, 64, 128, 256, 512};
        std::vector<float> output;
        juce::MidiBuffer midi;
        int global = 0;
        for (int cycle = 0; cycle < 4; ++cycle) {
            for (int size : sizes) {
                juce::AudioBuffer<float> buffer(2, size);
                fillSine(buffer, global, 1000.0f, amplitude);
                processor.processBlock(buffer, midi);
                expect(samplesFinite(buffer), "Variable-block render should stay finite. peak " +
                                                  juce::String(buffer.getMagnitude(0, size), 4));
                const auto* data = buffer.getReadPointer(0);
                output.insert(output.end(), data, data + size);
                global += size;
            }
        }
        return output;
    }

    void testBlockBoundaries() {
        constexpr float amplitude = 0.25f;
        {
            ProfilerAudioProcessor processor;
            setEmptyChain(processor);
            prepareProcessor(processor, 512);
            expectContinuous(renderBlockSizes(processor, amplitude), amplitude, "Dry");
            processor.releaseResources();
        }
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::Pedal);
            enableLowDrivePedal(processor);
            prepareProcessor(processor, 512);
            expectContinuous(renderBlockSizes(processor, amplitude), amplitude, "Pedal");
            processor.releaseResources();
        }
    }

    void testBypassSettles() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::Pedal);
        enableLowDrivePedal(processor);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        constexpr float amplitude = 0.25f;
        constexpr float frequency = 1000.0f;
        juce::MidiBuffer midi;
        int global = 0;
        for (int n = 0; n < 16; ++n) {
            juce::AudioBuffer<float> buffer(2, block);
            fillSine(buffer, global, frequency, amplitude);
            processor.processBlock(buffer, midi);
            expect(samplesFinite(buffer), "Pedal warmup should stay finite.");
            global += block;
        }

        setParameter(processor, "isPedalEnabled", 0.0f);
        std::vector<float> after;
        for (int n = 0; n < 32; ++n) {
            juce::AudioBuffer<float> buffer(2, block);
            fillSine(buffer, global, frequency, amplitude);
            processor.processBlock(buffer, midi);
            expect(samplesFinite(buffer), "Pedal bypass transition should stay finite. peak " +
                                              juce::String(buffer.getMagnitude(0, block), 4));
            const auto* data = buffer.getReadPointer(0);
            after.insert(after.end(), data, data + block);
            global += block;
        }

        const int settle = (int)std::lround(0.05 * kSampleRate);
        const int start = std::min(settle, std::max(0, (int)after.size() - block));
        double dot = 0.0;
        double energyOut = 0.0;
        double energyRef = 0.0;
        const int afterOrigin = global - (int)after.size();
        for (int index = start; index < (int)after.size(); ++index) {
            const double ref = amplitude * std::sin(juce::MathConstants<double>::twoPi * frequency *
                                                    (double)(afterOrigin + index) / kSampleRate);
            const double sample = (double)after[(size_t)index];
            dot += sample * ref;
            energyOut += sample * sample;
            energyRef += ref * ref;
        }
        const double corr = (energyOut > 0.0 && energyRef > 0.0) ? dot / std::sqrt(energyOut * energyRef) : 0.0;
        expect(corr > 0.98, "Bypassed pedal correlation " + juce::String(corr, 4));
        expect(processor.getLatencySamples() == 0,
               "Bypassed pedal latency should be 0. samples " + juce::String(processor.getLatencySamples()));
        processor.releaseResources();
    }

    void expectOversizedFinite(bool pedal) {
        ProfilerAudioProcessor processor;
        if (pedal) {
            setSoloStage(processor, SignalChain::Stage::Pedal);
            enableLowDrivePedal(processor);
        } else {
            setEmptyChain(processor);
        }
        prepareProcessor(processor, 256);
        juce::AudioBuffer<float> buffer(2, 1024);
        fillSine(buffer, 0, 1000.0f, 0.25f);
        processOne(processor, buffer);
        expect(samplesFinite(buffer), (pedal ? juce::String("Pedal") : juce::String("Dry")) +
                                          " oversized block should stay finite. peak " +
                                          juce::String(buffer.getMagnitude(0, 1024), 4));
        processor.releaseResources();
    }

    void testOversizedBlock() {
        expectOversizedFinite(false);
        expectOversizedFinite(true);
    }

    void expectSilentStage(ProfilerAudioProcessor& processor, int block, const juce::String& label) {
        juce::AudioBuffer<float> silent(2, block);
        silent.clear();
        processOne(processor, silent);
        expect(samplesFinite(silent), label + " silence should stay finite.");
        const float magnitude = silent.getMagnitude(0, block);
        expect(magnitude < 1.0e-4f, label + " silent input magnitude " + juce::String(magnitude, 6));

        const int latency = processor.getLatencySamples();
        constexpr int burst = 2048;
        int global = 0;
        juce::MidiBuffer midi;
        while (global < burst) {
            const int count = std::min(block, burst - global);
            juce::AudioBuffer<float> buffer(2, count);
            for (int sample = 0; sample < count; ++sample) {
                buffer.setSample(0, sample, 0.5f);
                buffer.setSample(1, sample, 0.5f);
            }
            processor.processBlock(buffer, midi);
            expect(samplesFinite(buffer), label + " burst should stay finite.");
            global += count;
        }

        const int wait = latency + 4096;
        int produced = 0;
        float tail = 0.0f;
        while (produced < wait) {
            const int count = std::min(block, wait - produced);
            juce::AudioBuffer<float> buffer(2, count);
            buffer.clear();
            processor.processBlock(buffer, midi);
            expect(samplesFinite(buffer), label + " tail should stay finite.");
            produced += count;
            if (produced >= wait) {
                tail = buffer.getMagnitude(0, count);
            }
        }
        expect(tail < 1.0e-3f, label + " magnitude after latency+4096 " + juce::String(tail, 6) + " latency " +
                                   juce::String(latency));
    }

    void testSilence() {
        constexpr int block = 256;
        {
            ProfilerAudioProcessor processor;
            setEmptyChain(processor);
            prepareProcessor(processor, block);
            expectSilentStage(processor, block, "Dry");
            processor.releaseResources();
        }
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::Pedal);
            enableLowDrivePedal(processor);
            prepareProcessor(processor, block);
            expectSilentStage(processor, block, "Pedal");
            processor.releaseResources();
        }
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::PitchDrop);
            setParameter(processor, "dropOn", 1.0f);
            setParameter(processor, "dropMix", 100.0f);
            setParameter(processor, "dropSemi", -2.0f);
            setParameter(processor, "dropLatency", 20.0f);
            prepareProcessor(processor, block);
            expectSilentStage(processor, block, "Drop");
            processor.releaseResources();
        }
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::PitchHarmonizer);
            setParameter(processor, "harmOn", 1.0f);
            setParameter(processor, "harmMix", 100.0f);
            setParameter(processor, "harmLatency", 22.0f);
            prepareProcessor(processor, block);
            expectSilentStage(processor, block, "Harmonizer");
            processor.releaseResources();
        }
    }

    double measureRealtime(ProfilerAudioProcessor& processor, const juce::String& label) {
        constexpr int block = 256;
        constexpr int count = 96000;
        juce::AudioBuffer<float> buffer(2, block);
        juce::MidiBuffer midi;
        for (int warm = 0; warm < 8; ++warm) {
            fillSine(buffer, warm * block, 1000.0f, 0.25f);
            processor.processBlock(buffer, midi);
        }

        const auto started = std::chrono::steady_clock::now();
        int global = 0;
        while (global < count) {
            fillSine(buffer, global, 1000.0f, 0.25f);
            processor.processBlock(buffer, midi);
            global += block;
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        const double audio = (double)count / kSampleRate;
        const double ratio = elapsed / audio;
        expect(samplesFinite(buffer), label + " realtime render should stay finite.");
        expect(elapsed < 2.0 && ratio < 1.0,
               label + " elapsed/audio " + juce::String(ratio, 4) + " elapsed " + juce::String(elapsed, 4));
        return ratio;
    }

    void testRealtime() {
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::Pedal);
            enableLowDrivePedal(processor);
            prepareProcessor(processor, 256);
            storeMetric("Pedal realtime ratio", "x realtime", measureRealtime(processor, "Pedal"));
            processor.releaseResources();
        }
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::PitchHarmonizer);
            setParameter(processor, "harmOn", 1.0f);
            setParameter(processor, "harmMix", 100.0f);
            setParameter(processor, "harmLatency", 22.0f);
            prepareProcessor(processor, 256);
            storeMetric("Harmonizer realtime ratio", "x realtime", measureRealtime(processor, "Harmonizer"));
            processor.releaseResources();
        }
        {
            ProfilerAudioProcessor processor;
            setSoloStage(processor, SignalChain::Stage::PitchDrop);
            setParameter(processor, "dropOn", 1.0f);
            setParameter(processor, "dropMix", 100.0f);
            setParameter(processor, "dropLatency", 20.0f);
            prepareProcessor(processor, 256);
            storeMetric("Drop realtime ratio", "x realtime", measureRealtime(processor, "Drop"));
            processor.releaseResources();
        }
    }

    void testHarmonizerTune() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::PitchHarmonizer);
        setParameter(processor, "harmOn", 1.0f);
        setParameter(processor, "harmMode", 1.0f);
        setParameter(processor, "harmMix", 100.0f);
        setParameter(processor, "harmLatency", 22.0f);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        constexpr int count = 72000;
        const auto output = renderSine(processor, block, count, 220.0f, 0.4f, "Harmonizer octave");
        const int latency = processor.getLatencySamples();
        const int skip = std::max(latency + (int)std::lround(0.5 * kSampleRate), latency);
        const int length = count - skip;
        expect(length > 4096, "Harmonizer tune window too short. skip " + juce::String(skip));

        std::vector<float> windowed((size_t)length);
        for (int index = 0; index < length; ++index) {
            const float hann = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float)index / (float)(length - 1)));
            windowed[(size_t)index] = output[(size_t)(skip + index)] * hann;
        }

        float peakHz = 440.0f;
        float peakMag = 0.0f;
        for (int hz = 300; hz <= 600; ++hz) {
            const float mag = goertzel(windowed.data(), length, kSampleRate, (double)hz);
            if (mag > peakMag) {
                peakMag = mag;
                peakHz = (float)hz;
            }
        }
        expect(std::abs(peakHz - 440.0f) <= 4.0f,
               "Octave peak should land within 4 Hz of 440. peak " + juce::String(peakHz, 0));

        float side = 0.0f;
        int sideHz = 0;
        for (int hz = 100; hz <= 1200; ++hz) {
            if (std::abs(hz - 440) <= 25) {
                continue;
            }
            const float mag = goertzel(windowed.data(), length, kSampleRate, (double)hz);
            if (mag > side) {
                side = mag;
                sideHz = hz;
            }
        }
        const float ratio = side / juce::jmax(1.0e-8f, peakMag);
        expect(ratio < 0.01f, "Octave residue ratio " + juce::String(ratio, 5) + " at " + juce::String(sideHz));
        processor.releaseResources();
    }

    void testDropTune() {
        ProfilerAudioProcessor processor;
        setSoloStage(processor, SignalChain::Stage::PitchDrop);
        setParameter(processor, "dropOn", 1.0f);
        setParameter(processor, "dropSemi", -2.0f);
        setParameter(processor, "dropMix", 100.0f);
        setParameter(processor, "dropLatency", 20.0f);
        constexpr int block = 256;
        prepareProcessor(processor, block);

        constexpr int count = 72000;
        const auto output = renderSine(processor, block, count, 220.0f, 0.4f, "Drop tune");
        const int latency = processor.getLatencySamples();
        const int skip = std::max(latency + (int)std::lround(0.25 * kSampleRate), latency);
        const int length = count - skip;
        expect(length > 4096, "Drop tune window too short. skip " + juce::String(skip));

        const double expected = 220.0 * std::pow(2.0, -2.0 / 12.0);
        float peakHz = (float)expected;
        float peakMag = 0.0f;
        const int low = std::max(1, (int)std::floor(expected - 8.0));
        const int high = (int)std::ceil(expected + 8.0);
        for (int hz = low; hz <= high; ++hz) {
            const float mag = goertzel(output.data() + skip, length, kSampleRate, (double)hz);
            if (mag > peakMag) {
                peakMag = mag;
                peakHz = (float)hz;
            }
        }
        const float cents = (float)(1200.0 * std::log2((double)peakHz / expected));
        expect(std::abs(cents) <= 15.0f,
               "Drop peak cents " + juce::String(cents, 2) + " Hz " + juce::String(peakHz, 2));
        const float leftover = goertzel(output.data() + skip, length, kSampleRate, 220.0);
        const float leftoverRatio = leftover / juce::jmax(1.0e-8f, peakMag);
        expect(leftoverRatio < 0.2f, "Drop leftover at 220 Hz ratio " + juce::String(leftoverRatio, 4));
        processor.releaseResources();
    }

    juce::Array<Metric> metrics;
};

AudioContractUnitTests audioContractUnitTests;

}  // namespace
}  // namespace profiler_tests
