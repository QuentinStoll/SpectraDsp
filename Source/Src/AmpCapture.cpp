#include "AmpCapture.h"

#include <algorithm>
#include <cmath>

namespace AmpCapture {
namespace {

void setError(juce::String* errorMessage, const juce::String& message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

// NAM's trigger: first sample of the blip response above the background noise.
int firstTrigger(const std::vector<float>& recorded, const int* blips, int blipCount, float threshold) {
    for (int offset = 0; offset < kLookahead + kLookback; ++offset) {
        double sum = 0.0;
        for (int blip = 0; blip < blipCount; ++blip) {
            sum += recorded[static_cast<std::size_t>(blips[blip] - kLookahead + offset)];
        }
        if (std::abs(sum / blipCount) > threshold) {
            return offset - kLookahead;
        }
    }
    return -1 - kLookahead;
}

}  // namespace

bool loadReamp(const juce::File& file, std::vector<float>& samples, juce::String* errorMessage) {
    samples.clear();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr) {
        setError(errorMessage, "Could not read " + file.getFileName() + ".");
        return false;
    }
    if (std::abs(reader->sampleRate - kSampleRate) > 0.5) {
        setError(errorMessage, "The reamp file must be 48 kHz.");
        return false;
    }
    if (reader->lengthInSamples < kRoundTripFrames || reader->lengthInSamples > kStandardFrames * 2) {
        setError(errorMessage, "This is not NAM's standard input.wav.");
        return false;
    }

    const auto frames = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(1, frames);
    if (!reader->read(&buffer, 0, frames, 0, true, false)) {
        setError(errorMessage, "Could not decode " + file.getFileName() + ".");
        return false;
    }
    samples.assign(buffer.getReadPointer(0), buffer.getReadPointer(0) + frames);
    return true;
}

double esr(const float* prediction, const float* target, int frames) {
    double error = 0.0;
    double energy = 0.0;
    for (int sample = 0; sample < frames; ++sample) {
        const double difference = static_cast<double>(prediction[sample]) - target[sample];
        error += difference * difference;
        energy += static_cast<double>(target[sample]) * target[sample];
    }
    return energy > 0.0 ? error / energy : 1.0;
}

Analysis analyse(const std::vector<float>& sent, const std::vector<float>& recorded) {
    Analysis analysis;
    const auto lastBlip = kBlips.back() + kLookback;
    if (sent.size() < static_cast<std::size_t>(kRoundTripFrames) || recorded.size() < static_cast<std::size_t>(lastBlip)) {
        return analysis;
    }

    for (const auto sample : recorded) {
        const auto magnitude = std::abs(sample);
        analysis.returnPeak = std::max(analysis.returnPeak, magnitude);
    }
    analysis.clipped = analysis.returnPeak >= 0.999f;

    float background = 0.0f;
    double noiseEnergy = 0.0;
    for (int sample = kNoiseStart; sample < kNoiseEnd; ++sample) {
        const auto value = recorded[static_cast<std::size_t>(sample)];
        background = std::max(background, std::abs(value));
        noiseEnergy += static_cast<double>(value) * value;
    }
    analysis.noiseRms = static_cast<float>(std::sqrt(noiseEnergy / (kNoiseEnd - kNoiseStart)));

    const auto threshold = std::max(background + 0.0003f, 1.001f * background);
    const auto latency = firstTrigger(recorded, kBlips.data(), static_cast<int>(kBlips.size()), threshold);
    for (std::size_t blip = 0; blip < kBlips.size(); ++blip) {
        analysis.blipLatency[blip] = firstTrigger(recorded, &kBlips[blip], 1, threshold);
    }
    if (latency < 0) {
        return analysis;
    }

    analysis.detected = true;
    analysis.latency = latency;
    // NAM keeps one sample of margin before the response so the model never needs the future.
    analysis.shift = std::max(0, latency - 1);

    if (sent.size() == static_cast<std::size_t>(kStandardFrames)) {
        const auto aligned = align(recorded, analysis.shift, kStandardFrames);
        analysis.replicateChecked = true;
        analysis.replicateEsr = esr(aligned.data(), aligned.data() + (kStandardFrames - kValidationFrames), kValidationFrames);
    }
    return analysis;
}

std::vector<float> align(const std::vector<float>& recorded, int shift, int frames) {
    std::vector<float> aligned(static_cast<std::size_t>(std::max(0, frames)), 0.0f);
    const auto start = static_cast<std::size_t>(std::max(0, shift));
    if (start < recorded.size()) {
        const auto count = std::min(aligned.size(), recorded.size() - start);
        std::copy_n(recorded.begin() + static_cast<std::ptrdiff_t>(start), count, aligned.begin());
    }
    return aligned;
}

bool writeWav(const juce::File& file, const std::vector<float>& samples, juce::String* errorMessage) {
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr) {
        setError(errorMessage, "Could not create " + file.getFullPathName() + ".");
        return false;
    }

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream,
                                      juce::AudioFormatWriterOptions{}
                                          .withSampleRate(kSampleRate)
                                          .withNumChannels(1)
                                          .withBitsPerSample(24));
    if (writer == nullptr) {
        setError(errorMessage, "Could not write " + file.getFileName() + ".");
        return false;
    }

    const float* channels[] = {samples.data()};
    if (!writer->writeFromFloatArrays(channels, 1, static_cast<int>(samples.size()))) {
        setError(errorMessage, "Could not write " + file.getFileName() + ".");
        return false;
    }
    return true;
}

}  // namespace AmpCapture
