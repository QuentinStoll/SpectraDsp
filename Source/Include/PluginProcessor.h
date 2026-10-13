#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>
#include <vector>

#include "EqBandLayout.h"
#include "Fx/FxRack.h"
#include "ProfileManager.h"
#include "SignalChainLayout.h"
#include "SpectraAmp.h"
#include "SpectraEngine.h"

namespace RTNeural {
template <typename T>
class Model;
}

//==============================================================================
/**
 */
class ProfilerAudioProcessor : public juce::AudioProcessor {
   public:
    //==============================================================================
    ProfilerAudioProcessor(juce::File profileDirectory = {},
                           juce::File playViewSettingsFile = {});
    ~ProfilerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState _apvts{*this, nullptr, "Parameters", createParameterLayout()};

    //===================================== Our func ===============================

    // Loading the Impulse responce file
    void loadIRFile();
    bool loadIRFile(const juce::File& file);
    void unloadIRFile();
    bool loadAmpFile(const juce::File& file);
    bool loadAmpFromMemory(const void* data, size_t size);
    bool loadIrFromMemory(const void* data, size_t size);
    bool loadProtectedAmp(const void* data, size_t size);
    bool loadProtectedIr(const void* data, size_t size);
    bool isSpectraLoaded() const noexcept;
    bool unlockSpectraSession(const juce::String& accessToken);
    void lockSpectraSession();
    bool beginIrCapture(juce::String* errorMessage);
    bool irCaptureFinished() const noexcept;
    bool sealIrCapture(juce::MemoryBlock& sealed, std::array<std::uint8_t, 32>& key, juce::String* errorMessage);
    bool storeCapturedIr(const juce::String& title,
                         const std::array<std::uint8_t, 32>& key,
                         const juce::MemoryBlock& sealed,
                         juce::String* errorMessage);
    void unloadAmpFile();
    bool isIRLoaded() const noexcept;
    bool isAmpFileLoaded() const noexcept;
    juce::File getCurrentIRFile() const;
    juce::File getCurrentAmpFile() const;
    bool applyProfile(int profileIndex, juce::String* errorMessage = nullptr);
    void syncLoadedFilesWithCurrentProfile();
    juce::String getAppliedProfileId() const;
    void clearAppliedProfile();
    ProfileManager& getProfileManager() noexcept;
    const ProfileManager& getProfileManager() const noexcept;
    float getRmsLevelInput() const noexcept;
    float getRmsLevelOutput() const noexcept;
    bool readTuner(Fx::TunerSnapshot& snapshot) const;
    SignalChain::Layout getChainLayout() const noexcept;
    void setChainLayout(const SignalChain::Layout& layout);
    void resetChainLayout();
    void placeChainStage(SignalChain::Stage stage, int slot);
    void clearChainSlot(int slot);

   private:
    enum ChainPositions {
        Gain = 0,
        NoiseGate
    };

    enum EqPositions {
        LowShelf = 0,
        Peak1,
        Peak2,
        Peak3,
        Peak4,
        HighShelf
    };

    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    using Chain = juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,
        juce::dsp::NoiseGate<float>>;

    using EqChain = juce::dsp::ProcessorChain<
        Filter,
        Filter,
        Filter,
        Filter,
        Filter,
        Filter>;

    Chain _chain;
    static constexpr int kChainCopies = SignalChain::movableSlotCount;

    std::array<EqChain, kChainCopies> _eqChains{};
    juce::dsp::Gain<float> _inputTrim;
    juce::dsp::Gain<float> _masterVolume;
    juce::dsp::Gain<float> _outputTrim;
    std::atomic<float> _rmsLevelInput{-60.0f};
    std::atomic<float> _rmsLevelOutput{-60.0f};

    static constexpr float SHELF_Q{EqBands::shelfQ};
    static constexpr float PEAK_Q{EqBands::peakQ};
    static constexpr double PARAMETER_RAMP_SECONDS{0.05};
    static constexpr double AMP_BYPASS_CROSSFADE_SECONDS{0.02};

    std::atomic<float>* _masterParam{nullptr};
    std::atomic<float>* _gainParam{nullptr};
    std::atomic<float>* _noiseParam{nullptr};
    std::atomic<float>* _inputParam{nullptr};
    std::atomic<float>* _outputParam{nullptr};

    std::array<std::atomic<float>*, EqBands::count> _eqGainParams{};
    std::array<std::atomic<float>*, EqBands::count> _eqFreqParams{};

    std::atomic<float>* _isMuteParam{nullptr};
    std::atomic<float>* _isEqEnabledParam{nullptr};
    std::atomic<float>* _isGateEnabledParam{nullptr};
    std::atomic<float>* _isAmpEnabledParam{nullptr};
    std::atomic<float>* _isCabEnabledParam{nullptr};
    std::atomic<float>* _cabLowCutParam{nullptr};
    std::atomic<float>* _isPedalEnabledParam{nullptr};
    std::atomic<float>* _pedalDriveParam{nullptr};
    std::atomic<float>* _pedalToneParam{nullptr};
    std::atomic<float>* _pedalLevelParam{nullptr};

    std::array<juce::SmoothedValue<float>, EqBands::count> _eqGainSmoothed{};
    std::array<juce::SmoothedValue<float>, EqBands::count> _eqFreqSmoothed{};
    juce::SmoothedValue<float> _cabLowCutSmoothed;
    bool _eqCoeffsDirty{true};
    std::atomic<std::uint64_t> _chainLayoutPacked{SignalChain::defaultPacked};
    Fx::FxRack _fx;
    int _reportedLatency = 0;

    std::array<Filter, kChainCopies> _cabLowCuts{};
    std::array<Filter, kChainCopies> _pedalToneFilters{};

    enum class IrCaptureState {
        Idle = 0,
        Recording = 1,
        Complete = 2
    };

    struct IrCapture {
        std::atomic<int> state{static_cast<int>(IrCaptureState::Idle)};
        std::atomic<std::uint32_t> index{0};
        std::vector<float> sweep;
        std::vector<float> recorded;
        std::size_t sweepFrames = 0;
        double sampleRate = 0.0;
    };

    ProfileManager _profileManager;
    SpectraEngine _spectra;
    IrCapture _irCapture;

    bool renderIrCapture(juce::AudioBuffer<float>& buffer);
    juce::String _appliedProfileId;

    void updateEqCoefficients();
    void updateCabLowCutCoefficients();
    void applyProfileFileValues(const juce::NamedValueSet& values);
    void processGateStage(juce::dsp::ProcessContextReplacing<float>& context);
    void processAmpStage(juce::AudioBuffer<float>& buffer,
                         juce::dsp::ProcessContextReplacing<float>& context,
                         int numSamples,
                         int instance);
    void processCabStage(juce::dsp::ProcessContextReplacing<float>& context, int instance);
    void processEqStage(juce::dsp::ProcessContextReplacing<float>& context, int instance);
    void processPedalStage(juce::AudioBuffer<float>& buffer,
                           juce::dsp::ProcessContextReplacing<float>& context,
                           int numSamples,
                           int instance);
    void syncAmpCopies(bool force = false);
    void setEqBypassed(bool bypassed);
    void loadIrIntoConvolvers(const void* data, size_t size);
    void updatePedalToneCoefficients();
    void updateReportedLatency();
    struct AmpModelMetadata {
        int inputSkip = 0;
        float inputGain = 1.0f;
        float outputGain = 1.0f;
    };

    static float getParameterValue(const std::atomic<float>* parameter, float fallback) noexcept;
    static bool isCompatibleAmpModel(const RTNeural::Model<float>& model);
    static float getMasterGainLinear(float masterPercent) noexcept;
    std::unique_ptr<RTNeural::Model<float>> parseAmpModel(const void* data,
                                                          size_t size,
                                                          AmpModelMetadata* metadata) const;
    bool publishAmpModel(std::unique_ptr<RTNeural::Model<float>> model,
                         const juce::File& sourceFile,
                         const AmpModelMetadata& metadata);
    bool installAmpPayload(const void* data, size_t size, const juce::File& sourceFile);

    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfilerAudioProcessor)

    //================================= Ir load =====================================

    // Buffer that contain the ir
    juce::AudioBuffer<float> _irBuffer;
    juce::MemoryBlock _irBytes;

    // Is ir loaded bool
    bool _irLoaded = false;
    juce::File _currentIRFile;

    std::array<juce::dsp::Convolution, kChainCopies> _cabConvolvers{};

    //================================= Amp file load ====================================
    bool _ampFileLoaded = false;
    juce::File _currentAmpFile;

    //================================= RTNeural Load ====================================
    // Declaration of the model type (for example, a generic sequential model)
    // You can adjust the type according to your model architecture (LSTM, Dense, Conv, etc.)
    std::unique_ptr<RTNeural::Model<float>> _neuralAmp;
    std::vector<std::unique_ptr<RTNeural::Model<float>>> _ampCopies;
    juce::MemoryBlock _ampModelBytes;
    SpectraAmp _spectraAmp;
    std::vector<float> _spectraWet;

    juce::CriticalSection _ampModelLock;
    bool _ampLoaded = false;  // Initialized to false until the JSON is loaded
    int _ampInputSkip = 0;
    float _ampInputGain = 1.0f;
    float _ampOutputGain = 1.0f;
    juce::SmoothedValue<float> _ampWetMix;

    // Keep the oversampler if needed, but be careful with the model's training sample rate!
    juce::dsp::Oversampling<float> oversampler{2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true};

    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>,
               kChainCopies>
        _dcBlockers{};
};
