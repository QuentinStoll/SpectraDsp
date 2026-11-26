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

// Onset of one click. The threshold is a fraction of that click's own peak, so
// hiss cannot fire the detector before the click. Returns samples after `blip`.
int clickOnset(const std::vector<float>& recorded, int blip, float noisePeak) {
    const auto begin = blip - kLookahead;
    const auto count = kLookahead + kLookback;
    if (begin < 0 || begin + count > static_cast<int>(recorded.size())) {
        return -1 - kLookahead;
    }
    float peak = 0.0f;
    for (int offset = 0; offset < count; ++offset) {
        peak = std::max(peak, std::abs(recorded[static_cast<std::size_t>(begin + offset)]));
    }
    if (peak < 1.0e-5f || peak < noisePeak * 3.0f) {
        return -1 - kLookahead;
    }
    const auto threshold = std::max(noisePeak * 3.0f, peak * 0.2f);
    for (int offset = 0; offset < count; ++offset) {
        if (std::abs(recorded[static_cast<std::size_t>(begin + offset)]) >= threshold) {
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

double windowEsr(const float* start, const float* end, int frames) {
    double error = 0.0;
    double energy = 0.0;
    for (int sample = 0; sample < frames; ++sample) {
        const double difference = static_cast<double>(start[sample]) - end[sample];
        error += difference * difference;
        energy += static_cast<double>(start[sample]) * start[sample];
    }
    return energy > 0.0 ? error / energy : 1.0;
}

double esr(const float* prediction, const float* target, int frames) {
    return windowEsr(prediction, target, frames);
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

    for (std::size_t blip = 0; blip < kBlips.size(); ++blip) {
        analysis.blipLatency[blip] = clickOnset(recorded, kBlips[blip], background);
    }
    if (analysis.blipLatency[0] < 0 || analysis.blipLatency[1] < 0) {
        return analysis;
    }

    analysis.detected = true;
    analysis.latency = (analysis.blipLatency[0] + analysis.blipLatency[1]) / 2;
    // NAM keeps one sample of margin before the response so the model never needs the future.
    analysis.shift = std::max(0, analysis.latency - 1);

    if (sent.size() == static_cast<std::size_t>(kStandardFrames)) {
        const auto aligned = align(recorded, analysis.shift, kStandardFrames);
        analysis.replicateChecked = true;
        const auto* start = aligned.data();
        const auto* end = aligned.data() + (kStandardFrames - kValidationFrames);
        constexpr int kMatchFrames = 48'000;
        constexpr int kMaxDrift = 2'000;
        analysis.replicateEsr = windowEsr(start, end, kValidationFrames);
        // One-sample steps. A distorted waveform only matches at the exact lag,
        // so a coarse step would miss a clock slip and call it a tone change.
        // The search uses one second. NAM's own check uses the whole validation,
        // so the lag is scored again on the full window before it can be accepted.
        double best = 1.0e9;
        analysis.endDrift = 0;
        for (int drift = -kMaxDrift; drift <= kMaxDrift; ++drift) {
            const auto score = windowEsr(start, end + drift, kMatchFrames);
            if (score < best) {
                best = score;
                analysis.endDrift = drift;
            }
        }
        const auto overlap = kValidationFrames - std::abs(analysis.endDrift);
        const auto* overlapStart = start + (analysis.endDrift < 0 ? -analysis.endDrift : 0);
        const auto* overlapEnd = end + (analysis.endDrift > 0 ? analysis.endDrift : 0);
        analysis.alignedEsr = overlap > 0 ? windowEsr(overlapStart, overlapEnd, overlap) : 1.0;

        double startEnergy = 0.0;
        double endEnergy = 0.0;
        for (int sample = 0; sample < overlap; ++sample) {
            const auto value = overlapStart[sample];
            const auto other = overlapEnd[sample];
            startEnergy += static_cast<double>(value) * value;
            endEnergy += static_cast<double>(other) * other;
        }
        const auto matchScale = endEnergy > 0.0 ? std::sqrt(startEnergy / endEnergy) : 1.0;
        double matchedError = 0.0;
        for (int sample = 0; sample < overlap; ++sample) {
            const auto difference = static_cast<double>(overlapStart[sample]) - matchScale * overlapEnd[sample];
            matchedError += difference * difference;
        }
        analysis.gainMatchedEsr = startEnergy > 0.0 ? matchedError / startEnergy : 1.0;

        const auto startRms = overlap > 0 ? std::sqrt(startEnergy / overlap) : 0.0;
        if (analysis.replicateEsr < kReplicateEsrLimit) {
            analysis.issue = TakeIssue::None;
        } else if (analysis.alignedEsr < kReplicateEsrLimit) {
            analysis.issue = TakeIssue::Latency;
        } else if (analysis.gainMatchedEsr < kReplicateEsrLimit) {
            analysis.issue = TakeIssue::Level;
        } else if (startRms < static_cast<double>(analysis.noiseRms) * 4.0) {
            analysis.issue = TakeIssue::Hiss;
        } else {
            analysis.issue = TakeIssue::Changed;
        }

        // NAM compares the whole start validation to the whole end validation and
        // refuses to export if that ESR is 0.01 or higher. A one-second match is
        // not enough to send the take.
        if (analysis.issue == TakeIssue::Latency) {
            auto corrected = aligned;
            finishTake(corrected, analysis);
            analysis.alignedEsr = windowEsr(corrected.data(),
                                            corrected.data() + (kStandardFrames - kValidationFrames),
                                            kValidationFrames);
            if (analysis.alignedEsr >= kReplicateEsrLimit) {
                analysis.issue = TakeIssue::Changed;
            }
        }
    }
    return analysis;
}

void removeClockSlip(std::vector<float>& aligned, int endDrift) {
    if (endDrift == 0 || aligned.empty()) {
        return;
    }
    const auto frames = static_cast<int>(aligned.size());
    // NAM compares the start validation to the end validation unchanged.
    // The start block stays put. The slip ramps in after it and holds through the end block.
    const auto hold = std::min(frames, kValidationFrames);
    const auto arrive = std::min(frames, std::max(hold + 1, kStandardFrames - kValidationFrames));
    const auto source = aligned;
    for (int index = 0; index < frames; ++index) {
        double drift = 0.0;
        if (index >= arrive) {
            drift = static_cast<double>(endDrift);
        } else if (index > hold) {
            drift = static_cast<double>(endDrift) * (index - hold) / (arrive - hold);
        }
        const auto position = static_cast<double>(index) + drift;
        const auto left = static_cast<int>(std::floor(position));
        const auto fraction = static_cast<float>(position - left);
        const auto sampleAt = [&](int sample) {
            return sample >= 0 && sample < frames ? source[static_cast<std::size_t>(sample)] : 0.0f;
        };
        aligned[static_cast<std::size_t>(index)] = sampleAt(left) + (sampleAt(left + 1) - sampleAt(left)) * fraction;
    }
}

void normalizePeak(std::vector<float>& aligned) {
    float peak = 0.0f;
    for (const auto sample : aligned) {
        peak = std::max(peak, std::abs(sample));
    }
    if (peak < 1.0e-8f || peak >= 0.999f) {
        return;
    }
    const auto gain = juce::Decibels::decibelsToGain(kTrainingPeakDb) / peak;
    for (auto& sample : aligned) {
        sample *= gain;
    }
}

void finishTake(std::vector<float>& aligned, const Analysis& analysis) {
    if (analysis.issue == TakeIssue::Latency) {
        removeClockSlip(aligned, analysis.endDrift);
    }
    normalizePeak(aligned);
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
