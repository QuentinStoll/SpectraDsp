#pragma once

#include <memory>
#include <string>

// Loaded clear neural amp. The engine headers stay inside the static library so the
// plugin does not bind a second copy of the JSON type.
class SpectraNeuralModel {
   public:
    enum class LoadStatus {
        NotNeural,
        Loaded,
        Invalid
    };

    static LoadStatus load(const std::string& json, std::unique_ptr<SpectraNeuralModel>& out);

    SpectraNeuralModel(const SpectraNeuralModel&) = delete;
    SpectraNeuralModel& operator=(const SpectraNeuralModel&) = delete;
    ~SpectraNeuralModel();

    std::unique_ptr<SpectraNeuralModel> clone() const;
    double expectedSampleRate() const;
    bool supportsArbitrarySampleRate() const;
    bool hasInputLevel();
    bool hasOutputLevel();
    double inputLevelDbu();
    double outputLevelDbu();
    void reset(double sampleRate, int maxBlock);
    void process(float* input, float* output, int frames);

   private:
    SpectraNeuralModel();

    struct Impl;
    std::unique_ptr<Impl> _impl;
};
