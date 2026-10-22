#include <JuceHeader.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "Fx/FxCatalog.h"
#include "PluginProcessor.h"
#include "SignalChainLayout.h"
#include "SpectraRateAdapter.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {

class AudioProcessorUnitTests : public juce::UnitTest {
   public:
    AudioProcessorUnitTests()
        : juce::UnitTest("AudioProcessor", "Profiler") {}

    void runTest() override {
        runCase("audio processor metadata", [this] {
            testMetadata();
        });

        runCase("audio processor bus layouts", [this] {
            testBusLayouts();
        });

        runCase("audio processor parameter defaults", [this] {
            testParameterDefaults();
        });

        runCase("audio processor state round trip", [this] {
            testStateRoundTrip();
        });

        runCase("audio processor chain layout round trip", [this] {
            testChainLayoutRoundTrip();
        });

        runCase("audio processor process block smoke", [this] {
            testProcessBlockSmoke();
        });

        runCase("audio processor mute clears buffer", [this] {
            testMuteClearsBuffer();
        });

        runCase("audio processor master zero silences buffer", [this] {
            testMasterZeroSilencesBuffer();
        });

        runCase("audio processor noise gate zero bypasses", [this] {
            testNoiseGateZeroBypasses();
        });

        runCase("audio processor gain changes output level", [this] {
            testGainChangesOutputLevel();
        });

        runCase("audio processor silent input stays silent", [this] {
            testSilentInputStaysSilent();
        });

        runCase("audio processor handles prepare block sizes", [this] {
            testPrepareBlockSizes();
        });

        runCase("audio processor release and reprepare", [this] {
            testReleaseAndReprepare();
        });

        runCase("audio processor rejects missing asset files", [this] {
            testMissingAssetFiles();
        });

        runCase("audio processor amp model residual and bypass", [this] {
            testAmpModelResidualAndBypass();
        });

        runCase("audio processor spectra neural amp", [this] {
            testSpectraNeuralAmp();
        });

        runCase("audio processor cabinet ir reaches the signal", [this] {
            testCabinetIrReachesSignal();
        });

        runCase("audio processor effect chain", [this] {
            testEffectChain();
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

    void expectClose(float actual, float expected, const juce::String& label) {
        expect(std::abs(actual - expected) <= 0.001f,
               label + " expected " + juce::String(expected) + " but got " + juce::String(actual));
    }

    float getParameterValue(ProfilerAudioProcessor& processor, const juce::String& parameterId) {
        const auto* value = processor._apvts.getRawParameterValue(parameterId);
        expect(value != nullptr, "Missing parameter: " + parameterId);
        return value != nullptr ? value->load() : 0.0f;
    }

    void setParameterValue(ProfilerAudioProcessor& processor,
                           const juce::String& parameterId,
                           float value) {
        auto* parameter = processor._apvts.getParameter(parameterId);
        expect(parameter != nullptr, "Missing parameter: " + parameterId);

        if (parameter != nullptr) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }

    juce::AudioProcessor::BusesLayout makeLayout(const juce::AudioChannelSet& input,
                                                 const juce::AudioChannelSet& output) {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(input);
        layout.outputBuses.add(output);
        return layout;
    }

    void prepareProcessor(ProfilerAudioProcessor& processor, double sampleRate, int blockSize) {
        processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
        processor.prepareToPlay(sampleRate, blockSize);
    }

    void fillBuffer(juce::AudioBuffer<float>& buffer, float value) {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                buffer.setSample(channel, sample, value);
            }
        }
    }

    void fillSineBuffer(juce::AudioBuffer<float>& buffer,
                        double sampleRate,
                        float frequency,
                        float amplitude) {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                const auto phase = juce::MathConstants<double>::twoPi * frequency * sample / sampleRate;
                buffer.setSample(channel, sample, amplitude * static_cast<float>(std::sin(phase)));
            }
        }
    }

    bool allSamplesFinite(const juce::AudioBuffer<float>& buffer) {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            const auto* samples = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                if (!std::isfinite(samples[sample])) {
                    return false;
                }
            }
        }

        return true;
    }

    float getMaximumRmsLevel(const juce::AudioBuffer<float>& buffer) {
        float rms = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            rms = juce::jmax(rms, buffer.getRMSLevel(channel, 0, buffer.getNumSamples()));
        }

        return rms;
    }

    float processSineAndMeasureRms(ProfilerAudioProcessor& processor,
                                   int blockSize = 256,
                                   int blockCount = 32) {
        constexpr double sampleRate = 44100.0;
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < blockCount; ++block) {
            fillSineBuffer(buffer, sampleRate, 1000.0f, 0.25f);
            processor.processBlock(buffer, midi);
        }

        expect(allSamplesFinite(buffer), "Processed sine buffer should stay finite.");
        return getMaximumRmsLevel(buffer);
    }

    void testMetadata() {
        ProfilerAudioProcessor processor;

        expect(processor.getName() == "Profiler", "Processor should expose the plugin name.");
        expect(!processor.acceptsMidi(), "Profiler should not accept MIDI.");
        expect(!processor.producesMidi(), "Profiler should not produce MIDI.");
        expect(!processor.isMidiEffect(), "Profiler should not be a MIDI effect.");
        expect(processor.getTailLengthSeconds() == 0.0, "Profiler should not declare a tail.");
        expect(processor.getNumPrograms() == 1, "Profiler should expose one default program.");
        expect(processor.getCurrentProgram() == 0, "Profiler should start on program zero.");
        expect(processor.getProgramName(0).isEmpty(), "Default program should not have a custom name.");
    }

    void testBusLayouts() {
        ProfilerAudioProcessor processor;

        expect(processor.isBusesLayoutSupported(makeLayout(juce::AudioChannelSet::mono(),
                                                           juce::AudioChannelSet::stereo())),
               "Mono input with stereo output should be supported.");
        expect(!processor.isBusesLayoutSupported(makeLayout(juce::AudioChannelSet::mono(),
                                                            juce::AudioChannelSet::mono())),
               "Mono output should be rejected.");
        expect(!processor.isBusesLayoutSupported(makeLayout(juce::AudioChannelSet::stereo(),
                                                            juce::AudioChannelSet::stereo())),
               "Stereo input should be rejected.");
        expect(!processor.isBusesLayoutSupported(makeLayout(juce::AudioChannelSet::create5point1(),
                                                            juce::AudioChannelSet::create5point1())),
               "Surround layouts should be rejected.");
    }

    void testParameterDefaults() {
        ProfilerAudioProcessor processor;

        expect(processor.getParameters().size() == 27 + Fx::parameterCount(),
               "Unexpected processor parameter count.");
        expectClose(getParameterValue(processor, "master"), 50.0f, "master default");
        expectClose(getParameterValue(processor, "gain"), 0.0f, "gain default");
        expectClose(getParameterValue(processor, "noise"), 10.0f, "noise default");
        expectClose(getParameterValue(processor, "input"), 0.0f, "input default");
        expectClose(getParameterValue(processor, "output"), 0.0f, "output default");
        expectClose(getParameterValue(processor, "bass"), 1.0f, "bass default");
        expectClose(getParameterValue(processor, "mid"), -3.5f, "mid default");
        expectClose(getParameterValue(processor, "highMid"), 1.5f, "highMid default");
        expectClose(getParameterValue(processor, "treble"), 3.0f, "treble default");
        expectClose(getParameterValue(processor, "presence"), -1.5f, "presence default");
        expectClose(getParameterValue(processor, "depth"), 2.5f, "depth default");
        expectClose(getParameterValue(processor, "depthFreq"), 80.0f, "depthFreq default");
        expectClose(getParameterValue(processor, "bassFreq"), 180.0f, "bassFreq default");
        expectClose(getParameterValue(processor, "midFreq"), 450.0f, "midFreq default");
        expectClose(getParameterValue(processor, "highMidFreq"), 1000.0f, "highMidFreq default");
        expectClose(getParameterValue(processor, "trebleFreq"), 2800.0f, "trebleFreq default");
        expectClose(getParameterValue(processor, "presenceFreq"), 6000.0f, "presenceFreq default");
        expectClose(getParameterValue(processor, "isMute"), 0.0f, "isMute default");
        expectClose(getParameterValue(processor, "isEqEnabled"), 1.0f, "isEqEnabled default");
        expectClose(getParameterValue(processor, "isGateEnabled"), 1.0f, "isGateEnabled default");
        expectClose(getParameterValue(processor, "isAmpEnabled"), 1.0f, "isAmpEnabled default");
        expectClose(getParameterValue(processor, "isCabEnabled"), 1.0f, "isCabEnabled default");
        expectClose(getParameterValue(processor, "cabLowCut"), 80.0f, "cabLowCut default");
        expectClose(getParameterValue(processor, "isPedalEnabled"), 1.0f, "isPedalEnabled default");
        expectClose(getParameterValue(processor, "pedalDrive"), 4.0f, "pedalDrive default");
        expectClose(getParameterValue(processor, "pedalTone"), 65.0f, "pedalTone default");
        expectClose(getParameterValue(processor, "pedalLevel"), 0.0f, "pedalLevel default");
    }

    void testStateRoundTrip() {
        ProfilerAudioProcessor source;
        setParameterValue(source, "master", 25.0f);
        setParameterValue(source, "gain", -4.0f);
        setParameterValue(source, "bass", 6.0f);
        setParameterValue(source, "highMid", -3.0f);
        setParameterValue(source, "midFreq", 750.0f);
        setParameterValue(source, "isMute", 1.0f);
        setParameterValue(source, "isEqEnabled", 0.0f);

        juce::MemoryBlock state;
        source.getStateInformation(state);
        expect(state.getSize() > 0, "Saved state should not be empty.");

        ProfilerAudioProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

        expectClose(getParameterValue(restored, "master"), 25.0f, "restored master");
        expectClose(getParameterValue(restored, "gain"), -4.0f, "restored gain");
        expectClose(getParameterValue(restored, "bass"), 6.0f, "restored bass");
        expectClose(getParameterValue(restored, "highMid"), -3.0f, "restored highMid");
        expectClose(getParameterValue(restored, "midFreq"), 750.0f, "restored midFreq");
        expectClose(getParameterValue(restored, "isMute"), 1.0f, "restored isMute");
        expectClose(getParameterValue(restored, "isEqEnabled"), 0.0f, "restored isEqEnabled");
    }

    void testChainLayoutRoundTrip() {
        SignalChain::Layout layout;
        expect(layout.slotFor(SignalChain::Stage::Amp) == 2, "Default Amp slot should be 2.");
        expect(layout.slotFor(SignalChain::Stage::Cab) == 4, "Default Cab slot should be 4.");
        expect(layout.slotFor(SignalChain::Stage::Eq) == 6, "Default EQ slot should be 6.");
        expect(!layout.contains(SignalChain::Stage::Pedal), "Default layout should not include a pedal.");
        expect(layout.isValid(), "Default layout should be valid.");
        expect(SignalChain::Layout::fromPacked(0xFFFFFFFFu).isValid(),
               "Invalid packed layout should fall back to the default.");

        layout.moveSlot(2, 5);
        layout.moveSlot(4, 1);
        layout.moveSlot(6, 3);
        expect(layout.slotFor(SignalChain::Stage::Cab) == 1, "Cab should move to slot 1.");
        expect(layout.slotFor(SignalChain::Stage::Eq) == 3, "EQ should move to slot 3.");
        expect(layout.slotFor(SignalChain::Stage::Amp) == 5, "Amp should move to slot 5.");

        const auto order = layout.processingOrder();
        expect(order[0] == SignalChain::Stage::Cab, "Left-most movable block should process first.");
        expect(order[1] == SignalChain::Stage::Eq, "Middle movable block should process second.");
        expect(order[2] == SignalChain::Stage::Amp, "Right-most movable block should process last.");

        layout.place(SignalChain::Stage::Pedal, 2);
        expect(layout.contains(SignalChain::Stage::Pedal), "Placing a pedal should add it to the chain.");
        expect(layout.atSlot(2) == SignalChain::Stage::Pedal, "Pedal should occupy the chosen slot.");

        ProfilerAudioProcessor source;
        source.setChainLayout(layout);
        expect(source.getChainLayout().slotFor(SignalChain::Stage::Amp) == 5, "Processor should store the Amp slot.");
        expect(source.getChainLayout().slotFor(SignalChain::Stage::Cab) == 1, "Processor should store the Cab slot.");
        expect(source.getChainLayout().slotFor(SignalChain::Stage::Eq) == 3, "Processor should store the EQ slot.");
        expect(source.getChainLayout().slotFor(SignalChain::Stage::Pedal) == 2, "Processor should store the Pedal slot.");

        juce::MemoryBlock state;
        source.getStateInformation(state);
        ProfilerAudioProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        expect(restored.getChainLayout().packed() == layout.packed(),
               "Chain layout should survive processor state round-trip.");

        restored.resetChainLayout();
        expect(restored.getChainLayout().packed() == SignalChain::defaultPacked,
               "Reset should restore Amp/Cab/EQ to slots 2/4/6.");

        expect(SignalChain::visualColumn(SignalChain::downSlot) == 7,
               "The wrap node should stay at the right end of the upper row.");
        expect(SignalChain::visualRow(SignalChain::returnSlot) == 1, "The chain should continue on a second row.");
        expect(SignalChain::visualColumn(SignalChain::returnSlot) == 0,
               "The second row should start at the left, like a line wrap.");
        expect(SignalChain::visualColumn(SignalChain::outputSlot) == 7,
               "The output should sit at the right end of the second row.");

        SignalChain::Layout wrapped;
        wrapped.moveSlot(6, SignalChain::chainSlotAt(1, 6));
        wrapped.place(SignalChain::Stage::Pedal, SignalChain::chainSlotAt(1, 1));
        const auto wrappedOrder = wrapped.processingOrder();
        expect(wrappedOrder[0] == SignalChain::Stage::Amp, "Upper-row Amp should still process first.");
        expect(wrappedOrder[1] == SignalChain::Stage::Cab, "Upper-row Cab should process before the lower row.");
        expect(wrappedOrder[2] == SignalChain::Stage::Pedal,
               "The leftmost lower-row block should process first on that row.");
        expect(wrappedOrder[3] == SignalChain::Stage::Eq,
               "The rightmost lower-row block should process last.");

        ProfilerAudioProcessor wrappedSource;
        wrappedSource.setChainLayout(wrapped);
        juce::MemoryBlock wrappedState;
        wrappedSource.getStateInformation(wrappedState);
        ProfilerAudioProcessor wrappedRestored;
        wrappedRestored.setStateInformation(wrappedState.getData(), static_cast<int>(wrappedState.getSize()));
        expect(wrappedRestored.getChainLayout().packed() == wrapped.packed(),
               "A block on the lower row should survive processor state round-trip.");
        expect(wrappedRestored.getChainLayout().slotFor(SignalChain::Stage::Eq) == SignalChain::chainSlotAt(1, 6),
               "The lower-row EQ slot should survive processor state round-trip.");

        SignalChain::Layout duplicates;
        duplicates.place(SignalChain::Stage::Amp, 3);
        expect(duplicates.count(SignalChain::Stage::Amp) == 2, "Placing an amp should keep the existing one.");
        expect(duplicates.atSlot(2) == SignalChain::Stage::Amp, "The original amp should stay in place.");
        expect(duplicates.atSlot(3) == SignalChain::Stage::Amp, "The new amp should occupy the chosen slot.");
        const auto duplicateOrder = duplicates.processingOrder();
        expect(duplicateOrder[0] == SignalChain::Stage::Amp, "The first amp should process first.");
        expect(duplicateOrder[1] == SignalChain::Stage::Amp, "The second amp should process immediately after the first.");
        duplicates.clear(3);
        expect(duplicates.count(SignalChain::Stage::Amp) == 1, "Clearing a slot should remove only that amp.");

        ProfilerAudioProcessor singleAmp;
        prepareProcessor(singleAmp, 44100.0, 256);
        setParameterValue(singleAmp, "gain", 6.0f);
        const auto oneAmpRms = processSineAndMeasureRms(singleAmp);
        ProfilerAudioProcessor doubledAmp;
        auto doubledLayout = doubledAmp.getChainLayout();
        doubledLayout.place(SignalChain::Stage::Amp, 3);
        doubledAmp.setChainLayout(doubledLayout);
        prepareProcessor(doubledAmp, 44100.0, 256);
        setParameterValue(doubledAmp, "gain", 6.0f);
        const auto twoAmpRms = processSineAndMeasureRms(doubledAmp);
        expect(twoAmpRms > oneAmpRms * 1.5f, "Two amp blocks in series should apply the amp stage twice.");

        ProfilerAudioProcessor reordered;
        reordered.setChainLayout(layout);
        prepareProcessor(reordered, 44100.0, 256);
        setParameterValue(reordered, "gain", 6.0f);
        const auto boostedRms = processSineAndMeasureRms(reordered);
        reordered.releaseResources();
        expect(boostedRms > 0.01f, "Amp gain should still apply after the chain is reordered.");
    }

    void testProcessBlockSmoke() {
        ProfilerAudioProcessor processor;
        prepareProcessor(processor, 44100.0, 128);

        juce::AudioBuffer<float> buffer(2, 128);
        fillBuffer(buffer, 0.1f);
        juce::MidiBuffer midi;

        processor.processBlock(buffer, midi);

        expect(allSamplesFinite(buffer), "Processed samples should stay finite.");
        expect(buffer.getMagnitude(0, buffer.getNumSamples()) > 0.0f,
               "Processor should leave an audible signal for a non-muted input.");
        expect(processor.getRmsLevelInput() > -40.0f, "Input meter should react to incoming audio.");
        processor.releaseResources();
    }

    void testMuteClearsBuffer() {
        ProfilerAudioProcessor processor;
        setParameterValue(processor, "isMute", 1.0f);
        prepareProcessor(processor, 44100.0, 128);

        juce::AudioBuffer<float> buffer(2, 128);
        fillBuffer(buffer, 0.5f);
        juce::MidiBuffer midi;

        processor.processBlock(buffer, midi);

        expectClose(buffer.getMagnitude(0, buffer.getNumSamples()), 0.0f, "muted buffer magnitude");
        expect(processor.getRmsLevelInput() > -20.0f, "Input meter should still see audio while muted.");
        expectClose(processor.getRmsLevelOutput(), -60.0f, "Output meter should go silent while muted");
        processor.releaseResources();
    }

    void testMasterZeroSilencesBuffer() {
        ProfilerAudioProcessor processor;
        setParameterValue(processor, "master", 0.0f);
        prepareProcessor(processor, 44100.0, 256);

        const auto rms = processSineAndMeasureRms(processor);

        expect(rms <= 0.001f, "Master volume at zero should silence the processed output.");
        processor.releaseResources();
    }

    void testNoiseGateZeroBypasses() {
        ProfilerAudioProcessor processor;
        setParameterValue(processor, "noise", 0.0f);
        prepareProcessor(processor, 44100.0, 256);

        const auto rms = processSineAndMeasureRms(processor);

        expect(rms > 0.01f, "Noise gate at zero should bypass and pass signal.");
        processor.releaseResources();
    }

    void testGainChangesOutputLevel() {
        ProfilerAudioProcessor unityProcessor;
        prepareProcessor(unityProcessor, 44100.0, 256);
        const auto unityRms = processSineAndMeasureRms(unityProcessor);
        unityProcessor.releaseResources();

        ProfilerAudioProcessor boostedProcessor;
        setParameterValue(boostedProcessor, "gain", 6.0f);
        prepareProcessor(boostedProcessor, 44100.0, 256);
        const auto boostedRms = processSineAndMeasureRms(boostedProcessor);
        boostedProcessor.releaseResources();

        ProfilerAudioProcessor cutProcessor;
        setParameterValue(cutProcessor, "gain", -6.0f);
        prepareProcessor(cutProcessor, 44100.0, 256);
        const auto cutRms = processSineAndMeasureRms(cutProcessor);
        cutProcessor.releaseResources();

        expect(unityRms > 0.01f, "Unity-gain processor should produce measurable signal.");
        expect(boostedRms > unityRms * 1.35f, "Positive gain should increase output RMS.");
        expect(cutRms < unityRms * 0.80f, "Negative gain should decrease output RMS.");
    }

    void testSilentInputStaysSilent() {
        ProfilerAudioProcessor processor;
        prepareProcessor(processor, 48000.0, 512);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        juce::MidiBuffer midi;

        processor.processBlock(buffer, midi);

        expect(allSamplesFinite(buffer), "Silent processed buffer should stay finite.");
        expectClose(buffer.getMagnitude(0, buffer.getNumSamples()), 0.0f, "silent input output magnitude");
        processor.releaseResources();
    }

    void testPrepareBlockSizes() {
        const int blockSizes[] = {1, 16, 128, 512};
        const double sampleRates[] = {44100.0, 48000.0};

        for (const auto sampleRate : sampleRates) {
            for (const auto blockSize : blockSizes) {
                ProfilerAudioProcessor processor;
                prepareProcessor(processor, sampleRate, blockSize);

                juce::AudioBuffer<float> buffer(2, blockSize);
                fillSineBuffer(buffer, sampleRate, 1000.0f, 0.2f);
                juce::MidiBuffer midi;

                processor.processBlock(buffer, midi);
                expect(allSamplesFinite(buffer),
                       "Processed buffer should stay finite for sample rate " + juce::String(sampleRate) +
                           " and block size " + juce::String(blockSize));
                processor.releaseResources();
            }
        }
    }

    void testReleaseAndReprepare() {
        ProfilerAudioProcessor processor;
        prepareProcessor(processor, 44100.0, 128);

        juce::AudioBuffer<float> firstBuffer(2, 128);
        fillSineBuffer(firstBuffer, 44100.0, 1000.0f, 0.2f);
        juce::MidiBuffer midi;
        processor.processBlock(firstBuffer, midi);
        processor.releaseResources();

        prepareProcessor(processor, 48000.0, 64);
        juce::AudioBuffer<float> secondBuffer(2, 64);
        fillSineBuffer(secondBuffer, 48000.0, 1000.0f, 0.2f);
        processor.processBlock(secondBuffer, midi);

        expect(allSamplesFinite(firstBuffer), "First processed buffer should stay finite.");
        expect(allSamplesFinite(secondBuffer), "Reprepared processed buffer should stay finite.");
        expect(secondBuffer.getMagnitude(0, secondBuffer.getNumSamples()) > 0.0f,
               "Reprepared processor should still produce signal.");
        processor.releaseResources();
    }

    void testMissingAssetFiles() {
        ProfilerAudioProcessor processor;
        const auto missingFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                     .getChildFile("profiler-missing-test-file.wav");

        expect(!missingFile.existsAsFile(), "Missing-file fixture should not exist.");
        expect(!processor.loadIRFile(missingFile), "Missing IR file should be rejected.");
        expect(!processor.isIRLoaded(), "Missing IR file should not mark IR as loaded.");

        expect(!processor.loadAmpFile(missingFile), "Missing amp file should be rejected.");
        expect(!processor.isAmpFileLoaded(), "Missing amp file should not mark amp as loaded.");

        processor.unloadIRFile();
        processor.unloadAmpFile();
        expect(!processor.isIRLoaded(), "Unloaded IR should stay unloaded.");
        expect(!processor.isAmpFileLoaded(), "Unloaded amp should stay unloaded.");
    }

    void silenceOtherStages(ProfilerAudioProcessor& processor) {
        setParameterValue(processor, "isEqEnabled", 0.0f);
        setParameterValue(processor, "isGateEnabled", 0.0f);
        setParameterValue(processor, "isPedalEnabled", 0.0f);
        setParameterValue(processor, "isCabEnabled", 0.0f);
        setParameterValue(processor, "gain", 0.0f);
        setParameterValue(processor, "master", 50.0f);
    }

    bool loadAmpJson(ProfilerAudioProcessor& processor, const char* json) {
        return processor.loadAmpFromMemory(json, std::strlen(json));
    }

    void clearMovableSlots(SignalChain::Layout& layout) {
        for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
            layout.clear(SignalChain::chainSlotForMovableIndex(index));
        }
    }

    bool samplesAreFinite(const juce::AudioBuffer<float>& buffer) {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            const auto* samples = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                if (!std::isfinite(samples[sample]) || std::abs(samples[sample]) > 8.0f) {
                    return false;
                }
            }
        }
        return true;
    }

    void testEffectChain() {
        expect(Fx::parameterCount() > 100, "Effect catalog should expose the studio modules.");
        expect(Fx::moduleFor(SignalChain::Stage::ReverbShimmer) != nullptr, "Shimmer module should exist.");
        expect(Fx::moduleFor(SignalChain::Stage::Amp) == nullptr, "Amp stays outside the effect catalog.");

        SignalChain::Layout shimmer;
        clearMovableSlots(shimmer);
        shimmer.place(SignalChain::Stage::ReverbShimmer, 9);
        const auto restored = SignalChain::Layout::fromPacked(shimmer.packed());
        expect(restored.slotFor(SignalChain::Stage::ReverbShimmer) == 9, "Shimmer should round-trip in slot 9.");
        expect(restored.atSlot(2) == SignalChain::Stage::Empty, "Cleared amp slot should stay empty.");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        for (int stageValue = static_cast<int>(SignalChain::Stage::PitchHarmonizer);
             stageValue <= static_cast<int>(SignalChain::Stage::PitchDrop);
             ++stageValue) {
            const auto stage = static_cast<SignalChain::Stage>(stageValue);
            ProfilerAudioProcessor processor;
            SignalChain::Layout layout;
            clearMovableSlots(layout);
            layout.place(stage, 1);
            processor.setChainLayout(layout);
            setParameterValue(processor, "isGateEnabled", 0.0f);
            setParameterValue(processor, "master", 50.0f);
            prepareProcessor(processor, sampleRate, blockSize);

            auto renderSine = [&](int blocks) {
                juce::AudioBuffer<float> buffer(2, blockSize);
                juce::MidiBuffer midi;
                for (int block = 0; block < blocks; ++block) {
                    for (int sample = 0; sample < blockSize; ++sample) {
                        const auto value = 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f *
                                                           static_cast<float>(block * blockSize + sample) /
                                                           static_cast<float>(sampleRate));
                        buffer.setSample(0, sample, value);
                        buffer.setSample(1, sample, 0.0f);
                    }
                    processor.processBlock(buffer, midi);
                    if (!samplesAreFinite(buffer)) {
                        return false;
                    }
                }
                return true;
            };

            bool finite = renderSine(8);
            if (const auto* module = Fx::moduleFor(stage)) {
                for (int param = 1; param < module->paramCount; ++param) {
                    const auto& spec = module->params[param];
                    if (spec.type == Fx::ParamType::Bool) {
                        setParameterValue(processor, spec.id, 0.0f);
                    } else if (spec.type == Fx::ParamType::Choice) {
                        setParameterValue(processor, spec.id, 0.0f);
                    } else {
                        setParameterValue(processor, spec.id, spec.minimum);
                    }
                }
                finite = finite && renderSine(4);
                for (int param = 1; param < module->paramCount; ++param) {
                    const auto& spec = module->params[param];
                    if (spec.type == Fx::ParamType::Bool) {
                        setParameterValue(processor, spec.id, 1.0f);
                    } else if (spec.type == Fx::ParamType::Choice) {
                        int lastIndex = 0;
                        if (spec.choices != nullptr) {
                            for (const char* cursor = spec.choices; *cursor != '\0'; ++cursor) {
                                if (*cursor == '|') {
                                    ++lastIndex;
                                }
                            }
                        }
                        setParameterValue(processor, spec.id, static_cast<float>(lastIndex));
                    } else {
                        setParameterValue(processor, spec.id, spec.maximum);
                    }
                }
                finite = finite && renderSine(8);
            }
            processor.releaseResources();
            expect(finite, "Effect stage " + juce::String(stageValue) + " should stay finite.");
        }

        ProfilerAudioProcessor delay;
        SignalChain::Layout delayLayout;
        clearMovableSlots(delayLayout);
        delayLayout.place(SignalChain::Stage::DelayTape, 1);
        delay.setChainLayout(delayLayout);
        setParameterValue(delay, "isGateEnabled", 0.0f);
        setParameterValue(delay, "master", 50.0f);
        setParameterValue(delay, "tapeTime", 80.0f);
        setParameterValue(delay, "tapeFb", 0.0f);
        setParameterValue(delay, "tapeMix", 100.0f);
        setParameterValue(delay, "tapeWow", 0.0f);
        setParameterValue(delay, "tapeSat", 0.0f);
        prepareProcessor(delay, sampleRate, blockSize);

        juce::MidiBuffer midi;
        for (int block = 0; block < 16; ++block) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            buffer.clear();
            delay.processBlock(buffer, midi);
        }

        int peakSample = -1;
        float peak = 0.0f;
        float earlyPeak = 0.0f;
        for (int block = 0; block < 24; ++block) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            buffer.clear();
            if (block == 0) {
                buffer.setSample(0, 0, 1.0f);
            }
            delay.processBlock(buffer, midi);
            for (int sample = 0; sample < blockSize; ++sample) {
                const auto time = block * blockSize + sample;
                const auto magnitude = std::abs(buffer.getSample(0, sample));
                if (time < static_cast<int>(0.02 * sampleRate)) {
                    earlyPeak = std::max(earlyPeak, magnitude);
                }
                if (magnitude > peak) {
                    peak = magnitude;
                    peakSample = time;
                }
            }
        }
        delay.releaseResources();
        expect(earlyPeak < 0.05f, "Wet tape delay should not return the impulse immediately.");
        expect(peak > 0.05f, "Wet tape delay should return the impulse (peak " + juce::String(peak, 4) + ").");
        expect(std::abs(peakSample - static_cast<int>(0.08 * sampleRate)) < static_cast<int>(0.015 * sampleRate),
               "Tape delay peak should land near 80 ms.");

        ProfilerAudioProcessor wet;
        ProfilerAudioProcessor dry;
        SignalChain::Layout plateLayout;
        clearMovableSlots(plateLayout);
        plateLayout.place(SignalChain::Stage::ReverbPlate, 1);
        wet.setChainLayout(plateLayout);
        SignalChain::Layout dryLayout;
        clearMovableSlots(dryLayout);
        dry.setChainLayout(dryLayout);
        for (auto* processor : {&wet, &dry}) {
            setParameterValue(*processor, "isGateEnabled", 0.0f);
            setParameterValue(*processor, "master", 50.0f);
            prepareProcessor(*processor, sampleRate, blockSize);
        }
        setParameterValue(wet, "plateMix", 100.0f);
        setParameterValue(wet, "plateOn", 0.0f);

        float worst = 0.0f;
        for (int block = 0; block < 6; ++block) {
            juce::AudioBuffer<float> wetBuffer(2, blockSize);
            juce::AudioBuffer<float> dryBuffer(2, blockSize);
            for (int sample = 0; sample < blockSize; ++sample) {
                const auto value = 0.15f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f *
                                                    static_cast<float>(block * blockSize + sample) / static_cast<float>(sampleRate));
                wetBuffer.setSample(0, sample, value);
                dryBuffer.setSample(0, sample, value);
            }
            wet.processBlock(wetBuffer, midi);
            dry.processBlock(dryBuffer, midi);
            if (block < 2) {
                continue;
            }
            for (int sample = 0; sample < blockSize; ++sample) {
                worst = std::max(worst, std::abs(wetBuffer.getSample(0, sample) - dryBuffer.getSample(0, sample)));
            }
        }
        wet.releaseResources();
        dry.releaseResources();
        expect(worst < 0.002f, "Bypassed plate should match the dry chain after the crossfade.");

        expect(Fx::moduleFor(SignalChain::Stage::DelayTapeExtreme) == nullptr, "Tape Sat should leave the catalog.");
        expect(Fx::moduleFor(SignalChain::Stage::PitchDrop) != nullptr, "Drop pitch shifter should be in the catalog.");
        SignalChain::Layout retired;
        clearMovableSlots(retired);
        retired.slots[0] = SignalChain::Stage::DelayTapeExtreme;
        expect(SignalChain::Layout::fromPacked(retired.packed()).atSlot(SignalChain::chainSlotForMovableIndex(0)) ==
                   SignalChain::Stage::Empty,
               "A saved Tape Sat block should be removed on load.");

        ProfilerAudioProcessor spring;
        SignalChain::Layout springLayout;
        clearMovableSlots(springLayout);
        springLayout.place(SignalChain::Stage::ReverbSpring, 1);
        spring.setChainLayout(springLayout);
        setParameterValue(spring, "isGateEnabled", 0.0f);
        setParameterValue(spring, "master", 50.0f);
        setParameterValue(spring, "springMix", 100.0f);
        setParameterValue(spring, "springBoing", 100.0f);
        setParameterValue(spring, "springDamp", 0.0f);
        setParameterValue(spring, "springDecay", 2.0f);
        prepareProcessor(spring, sampleRate, blockSize);
        float springPeak = 0.0f;
        for (int block = 0; block < 24; ++block) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int sample = 0; sample < blockSize; ++sample) {
                const auto value = 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f *
                                                   static_cast<float>(block * blockSize + sample) /
                                                   static_cast<float>(sampleRate));
                buffer.setSample(0, sample, value);
                buffer.setSample(1, sample, value);
            }
            juce::MidiBuffer springMidi;
            spring.processBlock(buffer, springMidi);
            springPeak = std::max(springPeak, buffer.getMagnitude(0, 0, blockSize));
        }
        spring.releaseResources();
        expect(springPeak < 1.5f, "Spring reverb should not scream over the input.");

        auto renderChainRms = [&](ProfilerAudioProcessor& processor) {
            double energy = 0.0;
            constexpr int blocks = 16;
            for (int block = 0; block < blocks; ++block) {
                juce::AudioBuffer<float> buffer(2, blockSize);
                for (int sample = 0; sample < blockSize; ++sample) {
                    const auto value = 0.1f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f *
                                                       static_cast<float>(block * blockSize + sample) /
                                                       static_cast<float>(sampleRate));
                    buffer.setSample(0, sample, value);
                    buffer.setSample(1, sample, value);
                }
                juce::MidiBuffer chainMidi;
                processor.processBlock(buffer, chainMidi);
                if (block >= 8) {
                    energy += static_cast<double>(buffer.getRMSLevel(0, 0, blockSize));
                }
            }
            return static_cast<float>(energy / 8.0);
        };

        const char* doubledModel =
            R"({"in_shape":[null,null,1],"in_skip":0,"layers":[{"type":"dense","activation":"","shape":[null,null,1],"weights":[[[2.0]],[0.0]]}]})";
        ProfilerAudioProcessor amps;
        expect(loadAmpJson(amps, doubledModel), "Doubling amp model should load.");
        SignalChain::Layout ampLayout;
        clearMovableSlots(ampLayout);
        const auto firstAmp = SignalChain::chainSlotForMovableIndex(0);
        const auto secondAmp = SignalChain::chainSlotForMovableIndex(1);
        ampLayout.place(SignalChain::Stage::Amp, firstAmp);
        ampLayout.place(SignalChain::Stage::Amp, secondAmp);
        amps.setChainLayout(ampLayout);
        silenceOtherStages(amps);
        setParameterValue(amps, "isAmpEnabled", 1.0f);
        setParameterValue(amps, "master", 50.0f);
        setParameterValue(amps, "gain", 0.0f);
        prepareProcessor(amps, sampleRate, blockSize);
        expect(amps.getChainLayout().count(SignalChain::Stage::Amp) == 2, "Two amp blocks should sit on the chain.");
        const auto bothAmps = renderChainRms(amps);
        amps.setChainSlotBypassed(firstAmp, true);
        const auto secondAmpOnly = renderChainRms(amps);
        amps.setChainSlotBypassed(firstAmp, false);
        amps.setChainSlotBypassed(secondAmp, true);
        const auto firstAmpOnly = renderChainRms(amps);
        amps.setChainSlotBypassed(firstAmp, true);
        const auto noAmps = renderChainRms(amps);
        amps.releaseResources();
        expect(bothAmps > secondAmpOnly * 1.5f, "Two doubling amps should be louder than one.");
        expect(secondAmpOnly > noAmps * 1.5f, "Bypassing the first amp should leave the second one running.");
        expect(firstAmpOnly > noAmps * 1.5f, "Bypassing the second amp should leave the first one running.");

        ProfilerAudioProcessor tuned;
        SignalChain::Layout tuneLayout;
        clearMovableSlots(tuneLayout);
        tuneLayout.place(SignalChain::Stage::PitchDrop, 1);
        tuneLayout.place(SignalChain::Stage::Tuner, 2);
        tuned.setChainLayout(tuneLayout);
        setParameterValue(tuned, "isGateEnabled", 0.0f);
        setParameterValue(tuned, "master", 50.0f);
        setParameterValue(tuned, "dropSemi", -12.0f);
        setParameterValue(tuned, "dropMix", 100.0f);
        setParameterValue(tuned, "dropLatency", 20.0f);
        setParameterValue(tuned, "tunMode", 0.0f);
        prepareProcessor(tuned, sampleRate, blockSize);
        for (int block = 0; block < 48; ++block) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int sample = 0; sample < blockSize; ++sample) {
                const auto value = 0.25f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f *
                                                    static_cast<float>(block * blockSize + sample) /
                                                    static_cast<float>(sampleRate));
                buffer.setSample(0, sample, value);
                buffer.setSample(1, sample, value);
            }
            juce::MidiBuffer tuneMidi;
            tuned.processBlock(buffer, tuneMidi);
        }
        Fx::TunerSnapshot snapshot;
        const auto read = tuned.readTuner(snapshot);
        tuned.releaseResources();
        expect(read && snapshot.count == 1, "Tuner should lock onto the direct input.");
        expect(std::abs(snapshot.midi[0] - 57) <= 1, "Tuner should report the input pitch, not the dropped signal.");
    }

    void testAmpModelResidualAndBypass() {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        const char* zeroModel =
            R"({"in_shape":[null,null,1],"in_skip":0,"layers":[{"type":"dense","activation":"","shape":[null,null,1],"weights":[[[0.0]],[0.0]]}]})";
        const char* residualModel =
            R"({"in_shape":[null,null,1],"in_skip":1,"layers":[{"type":"dense","activation":"","shape":[null,null,1],"weights":[[[0.0]],[0.0]]}]})";
        const char* attenuatedModel =
            R"({"in_shape":[null,null,1],"in_skip":1,"out_gain":-6.0,"layers":[{"type":"dense","activation":"","shape":[null,null,1],"weights":[[[0.0]],[0.0]]}]})";

        ProfilerAudioProcessor dry;
        silenceOtherStages(dry);
        prepareProcessor(dry, sampleRate, blockSize);
        const auto dryRms = processSineAndMeasureRms(dry, blockSize, 32);
        dry.releaseResources();
        expect(dryRms > 0.05f, "Dry amp path should pass the test tone.");

        ProfilerAudioProcessor replaced;
        silenceOtherStages(replaced);
        expect(loadAmpJson(replaced, zeroModel), "Zero model should load.");
        prepareProcessor(replaced, sampleRate, blockSize);
        const auto replacedRms = processSineAndMeasureRms(replaced, blockSize, 8);
        expect(replacedRms < 0.01f, "A model without in_skip should replace the dry signal.");

        setParameterValue(replaced, "isAmpEnabled", 0.0f);
        const auto bypassedRms = processSineAndMeasureRms(replaced, blockSize, 32);
        expect(std::abs(bypassedRms - dryRms) < dryRms * 0.15f,
               "Disabling the amp should restore the dry signal after the crossfade.");

        setParameterValue(replaced, "isAmpEnabled", 1.0f);
        const auto restoredRms = processSineAndMeasureRms(replaced, blockSize, 8);
        expect(restoredRms < 0.01f, "Re-enabling the amp should settle back to the model within one crossfade.");
        replaced.releaseResources();

        ProfilerAudioProcessor residual;
        silenceOtherStages(residual);
        expect(loadAmpJson(residual, residualModel), "Residual model should load.");
        prepareProcessor(residual, sampleRate, blockSize);
        const auto residualRms = processSineAndMeasureRms(residual, blockSize, 32);
        expect(std::abs(residualRms - dryRms) < dryRms * 0.15f,
               "in_skip should add the dry signal back onto a zero network.");
        residual.releaseResources();

        ProfilerAudioProcessor attenuated;
        silenceOtherStages(attenuated);
        expect(loadAmpJson(attenuated, attenuatedModel), "Attenuated residual model should load.");
        prepareProcessor(attenuated, sampleRate, blockSize);
        const auto attenuatedRms = processSineAndMeasureRms(attenuated, blockSize, 32);
        expect(std::abs(attenuatedRms - dryRms * 0.5f) < dryRms * 0.12f,
               "out_gain should scale the model output in decibels.");
        attenuated.releaseResources();
    }

    float renderSineRms(ProfilerAudioProcessor& processor, double sampleRate, int blockSize, int blockCount) {
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block < blockCount; ++block) {
            fillSineBuffer(buffer, sampleRate, 1000.0f, 0.25f);
            processor.processBlock(buffer, midi);
            expect(allSamplesFinite(buffer), "Spectra amp sine should stay finite.");
        }
        return getMaximumRmsLevel(buffer);
    }

    void testSpectraNeuralAmp() {
        constexpr double matchedRate = 48000.0;
        constexpr double hostRate = 44100.0;
        constexpr int blockSize = 256;
        const char* halfModel =
            R"({"version":"0.5.4","architecture":"Linear","config":{"receptive_field":1,"bias":false},"weights":[0.5],"sample_rate":48000})";
        const char* brokenModel =
            R"({"version":"0.5.4","architecture":"Linear","config":{"receptive_field":1,"bias":false},"weights":[],"sample_rate":48000})";
        const char* lstmModel =
            R"({"version":"0.5.4","architecture":"LSTM","config":{"input_size":1,"hidden_size":3,"num_layers":1},"weights":[-0.21677088737487793,-0.6683622002601624,-0.2560940980911255,-0.3588429093360901,0.17952610552310944,0.19445613026618958,-0.01662646047770977,0.5353694558143616,-0.2536540627479553,-0.5132213234901428,-0.020476307719945908,0.08592455089092255,-0.6891753673553467,0.3627359867095947,0.008421811275184155,0.3113192617893219,0.14251480996608734,0.07989779114723206,-0.18211324512958527,0.7118963003158569,0.41084015369415283,-0.6571938395500183,-0.13214066624641418,-0.2698603868484497,0.49387243390083313,-0.3491725027561188,0.6353667974472046,-0.5005152225494385,0.2052856683731079,-0.4301638901233673,-0.15770092606544495,-0.7181791067123413,0.056290093809366226,-0.49049463868141174,0.6623441576957703,0.09029324352741241,0.34005245566368103,0.16416560113430023,0.15520110726356506,-0.4155678153038025,-0.36928507685661316,0.3211132884025574,-0.6769840121269226,-0.1575538069009781,0.05268515646457672,-0.4191459119319916,0.599330484867096,0.21518059074878693,-4.246325492858887,-3.315647840499878,-4.328850746154785,4.496089458465576,5.015639305114746,3.6492037773132324,0.14431169629096985,-0.6633821725845337,0.11673200130462646,-0.1418764889240265,-0.4897872805595398,-0.8689419031143188,-0.06714004278182983,-0.4450395107269287,-0.02142983116209507,-0.15136894583702087,-2.775207996368408,-0.08681213855743408,0.05702732503414154,0.670292317867279,0.31442636251449585,0.30793967843055725],"sample_rate":48000})";

        ProfilerAudioProcessor broken;
        expect(!loadAmpJson(broken, brokenModel), "A neural model with empty weights should be rejected.");

        ProfilerAudioProcessor dry;
        silenceOtherStages(dry);
        prepareProcessor(dry, matchedRate, blockSize);
        const auto dryLatency = dry.getLatencySamples();
        const auto dryRms = renderSineRms(dry, matchedRate, blockSize, 16);
        dry.releaseResources();
        expect(dryRms > 0.05f, "Dry path at the model rate should pass the test tone.");

        ProfilerAudioProcessor matched;
        silenceOtherStages(matched);
        expect(loadAmpJson(matched, halfModel), "Half-gain linear model should load.");
        prepareProcessor(matched, matchedRate, blockSize);
        expect(matched.getLatencySamples() == dryLatency,
               "A model running at the host rate should not add latency.");
        const auto matchedRms = renderSineRms(matched, matchedRate, blockSize, 16);
        expect(std::abs(matchedRms - dryRms * 0.5f) < dryRms * 0.12f,
               "A weight of 0.5 should halve the dry level.");

        const char* smallContainer =
            R"({"version":"0.5.4","architecture":"SlimmableContainer","config":{"submodels":[{"max_value":0.5,"model":{"version":"0.5.4","architecture":"Linear","config":{"receptive_field":1,"bias":false},"weights":[0.25],"sample_rate":48000}},{"max_value":1.0,"model":{"version":"0.5.4","architecture":"Linear","config":{"receptive_field":1,"bias":false},"weights":[0.5],"sample_rate":48000}}]},"weights":[],"sample_rate":48000})";
        ProfilerAudioProcessor slim;
        silenceOtherStages(slim);
        expect(loadAmpJson(slim, smallContainer), "A slimmable container should load.");
        prepareProcessor(slim, matchedRate, blockSize);
        const auto slimRms = renderSineRms(slim, matchedRate, blockSize, 16);
        expect(std::abs(slimRms - dryRms * 0.25f) < dryRms * 0.12f,
               "A slimmable model should start on its smallest network, matching NAM's Slim default of 0.");
        slim.releaseResources();

        setParameterValue(matched, "isAmpEnabled", 0.0f);
        const auto bypassedRms = renderSineRms(matched, matchedRate, blockSize, 32);
        expect(std::abs(bypassedRms - dryRms) < dryRms * 0.15f,
               "Disabling the amp should restore the dry signal after the crossfade.");
        matched.releaseResources();

        ProfilerAudioProcessor resampled;
        ProfilerAudioProcessor dryHost;
        silenceOtherStages(resampled);
        silenceOtherStages(dryHost);
        expect(loadAmpJson(resampled, lstmModel), "LSTM fixture should load.");
        prepareProcessor(resampled, hostRate, blockSize);
        prepareProcessor(dryHost, hostRate, blockSize);
        SpectraRateAdapter rateProbe;
        rateProbe.prepare(hostRate, matchedRate, blockSize);
        expect(resampled.getLatencySamples() == dryHost.getLatencySamples() + rateProbe.latencySamples(),
               "A 48 kHz model on a 44.1 kHz host should report the sinc adapter latency.");
        const auto resampledRms = renderSineRms(resampled, hostRate, blockSize, 8);
        expect(resampledRms > 0.0f, "Resampled model should produce a signal.");
        resampled.releaseResources();
        dryHost.releaseResources();

        const char* boostedOutput =
            R"({"version":"0.5.4","architecture":"Linear","config":{"receptive_field":1,"bias":false},"weights":[1.0],"sample_rate":48000,"metadata":{"input_level_dbu":12.0,"output_level_dbu":18.0}})";
        const char* hotterInput =
            R"({"version":"0.5.4","architecture":"Linear","config":{"receptive_field":1,"bias":false},"weights":[1.0],"sample_rate":48000,"metadata":{"input_level_dbu":6.0}})";
        const float sixDb = std::pow(10.0f, 6.0f / 20.0f);

        ProfilerAudioProcessor outputLevel;
        silenceOtherStages(outputLevel);
        expect(loadAmpJson(outputLevel, boostedOutput), "A model with output level metadata should load.");
        prepareProcessor(outputLevel, matchedRate, blockSize);
        const auto boostedRms = renderSineRms(outputLevel, matchedRate, blockSize, 16);
        expect(std::abs(boostedRms - dryRms * sixDb) < dryRms * 0.08f,
               "An output level 6 dB above the 12 dBu reference should raise the sine by 6 dB.");
        float earlyRms = 0.0f;
        float lateRms = 0.0f;
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            for (int block = 0; block < 16; ++block) {
                fillSineBuffer(buffer, matchedRate, 1000.0f, 0.25f);
                outputLevel.processBlock(buffer, midi);
                if (block == 8) {
                    earlyRms = getMaximumRmsLevel(buffer);
                }
                if (block == 15) {
                    lateRms = getMaximumRmsLevel(buffer);
                }
            }
        }
        expect(std::abs(earlyRms - lateRms) < juce::jmax(1.0e-4f, lateRms * 0.02f),
               "A warmed linear model should hold its level from block to block.");
        outputLevel.releaseResources();

        ProfilerAudioProcessor inputLevel;
        silenceOtherStages(inputLevel);
        expect(loadAmpJson(inputLevel, hotterInput), "A model with input level metadata should load.");
        prepareProcessor(inputLevel, matchedRate, blockSize);
        const auto hotterRms = renderSineRms(inputLevel, matchedRate, blockSize, 16);
        expect(std::abs(hotterRms - dryRms * sixDb) < dryRms * 0.08f,
               "An input level 6 dB below the 12 dBu reference should raise the sine by 6 dB.");
        inputLevel.releaseResources();

        ProfilerAudioProcessor prewarmed;
        silenceOtherStages(prewarmed);
        expect(loadAmpJson(prewarmed, lstmModel), "LSTM fixture should load for the prewarm check.");
        prepareProcessor(prewarmed, matchedRate, blockSize);
        float firstSilence = 0.0f;
        float settledSilence = 0.0f;
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            buffer.clear();
            prewarmed.processBlock(buffer, midi);
            firstSilence = buffer.getMagnitude(0, 0, blockSize);
            for (int block = 0; block < 188; ++block) {
                buffer.clear();
                prewarmed.processBlock(buffer, midi);
            }
            settledSilence = buffer.getMagnitude(0, 0, blockSize);
        }
        expect(std::abs(firstSilence - settledSilence) <= juce::jmax(1.0e-3f, 0.05f * juce::jmax(firstSilence, settledSilence)),
               "The first silent block should already match the LSTM state after another second. first " +
                   juce::String(firstSilence, 5) + " settled " + juce::String(settledSilence, 5));
        prewarmed.releaseResources();

        expectRateAdapterPassesBand(rateProbe);
        expectRateAdapterRejectsAlias();
    }

    void expectRateAdapterPassesBand(SpectraRateAdapter& adapter) {
        constexpr double hostRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int blocks = 160;
        const int lag = adapter.latencySamples();
        std::vector<float> input(static_cast<std::size_t>(blockSize * blocks), 0.0f);
        std::vector<float> output(input.size(), 0.0f);
        const auto identity = [](const float* in, float* out, int frames) {
            std::memcpy(out, in, sizeof(float) * static_cast<std::size_t>(frames));
        };
        for (int block = 0; block < blocks; ++block) {
            for (int sample = 0; sample < blockSize; ++sample) {
                const int index = block * blockSize + sample;
                const auto phase = juce::MathConstants<double>::twoPi * 1000.0 * index / hostRate;
                input[static_cast<std::size_t>(index)] = 0.25f * static_cast<float>(std::sin(phase));
            }
            adapter.process(input.data() + block * blockSize, output.data() + block * blockSize, blockSize, identity);
        }

        const auto score = [&](int delay) {
            double dot = 0.0;
            double energyIn = 0.0;
            double energyOut = 0.0;
            for (int index = delay; index < static_cast<int>(input.size()); ++index) {
                const float in = input[static_cast<std::size_t>(index - delay)];
                const float out = output[static_cast<std::size_t>(index)];
                dot += static_cast<double>(in) * out;
                energyIn += static_cast<double>(in) * in;
                energyOut += static_cast<double>(out) * out;
            }
            return energyIn > 0.0 && energyOut > 0.0 ? dot / std::sqrt(energyIn * energyOut) : 0.0;
        };
        const double aligned = score(lag);
        expect(aligned > 0.98, "A 1 kHz sine should survive the sinc round trip at the reported latency.");
        expect(adapter.droppedSamples() == 0, "The sinc adapter should not drop samples over a one-second tone.");
        expect(adapter.underruns() == 0, "The sinc adapter should not underrun over a one-second tone.");
    }

    juce::MemoryBlock makeDelayedImpulseWav(int delaySamples) {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream>();
        auto* rawStream = static_cast<juce::MemoryOutputStream*>(stream.get());
        juce::WavAudioFormat format;
        auto writer = format.createWriterFor(stream,
                                             juce::AudioFormatWriterOptions{}
                                                 .withSampleRate(48000.0)
                                                 .withNumChannels(1)
                                                 .withBitsPerSample(16));
        if (writer == nullptr) {
            return {};
        }

        juce::AudioBuffer<float> samples(1, delaySamples + 32);
        samples.clear();
        samples.setSample(0, 0, 0.25f);
        samples.setSample(0, delaySamples, 1.0f);
        const auto wrote = writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples());
        writer->flush();
        auto block = rawStream->getMemoryBlock();
        writer.reset();
        if (!wrote) {
            return {};
        }
        return block;
    }

    void testCabinetIrReachesSignal() {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;
        constexpr int delaySamples = 80;
        const auto wav = makeDelayedImpulseWav(delaySamples);
        expect(wav.getSize() > 0, "Delayed impulse wav should be written.");

        ProfilerAudioProcessor processor;
        setParameterValue(processor, "isEqEnabled", 0.0f);
        setParameterValue(processor, "isGateEnabled", 0.0f);
        setParameterValue(processor, "isPedalEnabled", 0.0f);
        setParameterValue(processor, "isAmpEnabled", 0.0f);
        setParameterValue(processor, "isCabEnabled", 1.0f);
        setParameterValue(processor, "gain", 0.0f);
        setParameterValue(processor, "master", 50.0f);
        prepareProcessor(processor, sampleRate, blockSize);
        expect(processor.loadIrFromMemory(wav.getData(), wav.getSize()), "Cabinet IR should load.");

        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        buffer.clear();
        processor.processBlock(buffer, midi);
        juce::Thread::sleep(150);
        for (int block = 0; block < 40; ++block) {
            buffer.clear();
            processor.processBlock(buffer, midi);
        }

        buffer.clear();
        buffer.setSample(0, 0, 0.8f);
        buffer.setSample(1, 0, 0.8f);
        processor.processBlock(buffer, midi);

        int peakIndex = 0;
        float peak = 0.0f;
        for (int sample = 0; sample < blockSize; ++sample) {
            const float value = std::abs(buffer.getSample(0, sample));
            if (value > peak) {
                peak = value;
                peakIndex = sample;
            }
        }
        expect(std::abs(buffer.getSample(0, 0)) < 0.05f,
               "A loaded cabinet IR should hold most of the impulse back. sample0 "
                   + juce::String(buffer.getSample(0, 0), 4));
        expect(peakIndex >= delaySamples - 8 && peakIndex <= delaySamples + 24,
               "The cabinet impulse should land near the IR delay. peak "
                   + juce::String(peak, 4) + " at " + juce::String(peakIndex));
        expect(peak > 0.02f, "The cabinet IR should produce a peak. peak " + juce::String(peak, 4));

        setParameterValue(processor, "isCabEnabled", 0.0f);
        for (int block = 0; block < 4; ++block) {
            buffer.clear();
            processor.processBlock(buffer, midi);
        }
        buffer.clear();
        buffer.setSample(0, 0, 0.8f);
        buffer.setSample(1, 0, 0.8f);
        processor.processBlock(buffer, midi);
        expect(std::abs(buffer.getSample(0, 0) - 0.8f) < 0.08f,
               "Bypassing the cabinet should pass the impulse straight through.");
        processor.releaseResources();
    }

    void expectRateAdapterRejectsAlias() {
        constexpr double hostRate = 96000.0;
        constexpr int blockSize = 256;
        constexpr int blocks = 64;
        SpectraRateAdapter adapter;
        adapter.prepare(hostRate, 48000.0, blockSize);
        const auto identity = [](const float* in, float* out, int frames) {
            std::memcpy(out, in, sizeof(float) * static_cast<std::size_t>(frames));
        };
        float peak = 0.0f;
        std::vector<float> input(static_cast<std::size_t>(blockSize), 0.0f);
        std::vector<float> output(static_cast<std::size_t>(blockSize), 0.0f);
        const int skip = adapter.latencySamples() + blockSize;
        for (int block = 0; block < blocks; ++block) {
            for (int sample = 0; sample < blockSize; ++sample) {
                const int index = block * blockSize + sample;
                const auto phase = juce::MathConstants<double>::twoPi * 30000.0 * index / hostRate;
                input[static_cast<std::size_t>(sample)] = 0.25f * static_cast<float>(std::sin(phase));
            }
            adapter.process(input.data(), output.data(), blockSize, identity);
            if (block * blockSize < skip) {
                continue;
            }
            for (float sample : output) {
                peak = std::max(peak, std::abs(sample));
            }
        }
        expect(peak < 0.05f,
               "A 30 kHz sine heading into a 48 kHz model should be rejected. peak " + juce::String(peak, 5));
        expect(adapter.droppedSamples() == 0, "Alias check should not drop samples.");
    }
};

AudioProcessorUnitTests audioProcessorUnitTests;

}  // namespace
}  // namespace profiler_tests
