#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

// Reamp capture for NAM training. The positions follow the standard NAM input.wav (v3 signal):
// validation, silence, two blips, chirps, noise, the training audio, then the validation again.
namespace AmpCapture {

constexpr double kSampleRate = 48000.0;
constexpr int kStandardFrames = 9'120'000;
constexpr int kValidationFrames = 432'000;
constexpr int kNoiseStart = 492'000;
constexpr int kNoiseEnd = 498'000;
constexpr std::array<int, 2> kBlips{504'000, 552'000};
constexpr int kLookahead = 1'000;
constexpr int kLookback = 12'000;
constexpr int kRoundTripFrames = 600'000;
constexpr int kTailFrames = 24'000;
constexpr int kBlipDisagreement = 20;
constexpr int kLatencyDriftLimit = 8;
constexpr double kReplicateEsrLimit = 0.01;
constexpr float kTrainingPeakDb = -6.0f;

// Why the start and the end of a take do not match. None means they match,
// or a latency/level difference was corrected without touching the amp gain.
enum class TakeIssue { None,
                       Latency,
                       Level,
                       Hiss,
                       Changed };

struct Analysis {
    bool detected = false;
    int latency = 0;
    int shift = 0;
    std::array<int, 2> blipLatency{-1, -1};
    float returnPeak = 0.0f;
    float noiseRms = 0.0f;
    bool clipped = false;
    bool replicateChecked = false;
    double replicateEsr = 0.0;
    // Extra samples the end validation needs to line up with the start.
    // Non-zero means the interface delay moved during the take, not the amp.
    int endDrift = 0;
    double alignedEsr = 0.0;
    double gainMatchedEsr = 0.0;
    TakeIssue issue = TakeIssue::None;
};

// Removes a measured Helix clock slip, then sets the peak to the training level.
// The amp gain and the pedal drive are not changed.
void finishTake(std::vector<float>& aligned, const Analysis& analysis);

bool loadReamp(const juce::File& file, std::vector<float>& samples, juce::String* errorMessage);

double esr(const float* prediction, const float* target, int frames);

// `sent` is what went out, `recorded` what came back from input 1, unshifted.
// The replicate check needs the full standard signal.
Analysis analyse(const std::vector<float>& sent, const std::vector<float>& recorded);

// Moves the recording earlier by `shift` and returns exactly `frames` samples.
std::vector<float> align(const std::vector<float>& recorded, int shift, int frames);

bool writeWav(const juce::File& file, const std::vector<float>& samples, juce::String* errorMessage);

}  // namespace AmpCapture
