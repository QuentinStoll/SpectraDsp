#pragma once

#include <JuceHeader.h>

#include <memory>

#include "Fx/TunerSnapshot.h"
#include "SignalChainLayout.h"

namespace Fx {
class FxRack {
   public:
    explicit FxRack(juce::AudioProcessorValueTreeState& apvts);
    ~FxRack();

    void prepare(double sampleRate, int maxBlock);
    void reset();
    void setTempo(double bpm) noexcept;
    // lane selects a private scratch set so two branches can run at once.
    // Calls that share a lane, or the same instance, must stay on one thread.
    void process(SignalChain::Stage stage,
                 int instance,
                 float* left,
                 float* right,
                 int numSamples,
                 bool enabled,
                 const float* monitorLeft = nullptr,
                 const float* monitorRight = nullptr,
                 int lane = 0);
    int latencySamplesFor(SignalChain::Stage stage) const;
    double tailSecondsFor(SignalChain::Stage stage) const;
    bool readTuner(TunerSnapshot& snapshot) const;

   private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
}  // namespace Fx
