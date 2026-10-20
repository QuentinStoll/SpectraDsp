#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// Host <-> model rate conversion. NeuralAmpModelerCore 0.6.0 has no resampler;
// the official plugin uses a 12-lobe Lanczos outside the core. This is a short
// Hann-windowed sinc with the same job: anti-alias on the way in, and a fixed
// delay the host can compensate.
class SpectraRateAdapter {
   public:
    static constexpr int kRadius = 24;

    void prepare(double hostRate, double modelRate, int maxHostBlock);
    int latencySamples() const noexcept { return _latency; }
    int maxCallbackFrames() const noexcept { return _maxModelFrames; }
    int droppedSamples() const noexcept { return _dropped; }
    int underruns() const noexcept { return _underruns; }

    template <typename Model>
    void process(const float* hostIn, float* hostOut, int hostFrames, Model&& model);

   private:
    struct Stage {
        double step = 1.0;
        float cutoff = 1.0f;
        std::vector<float> ring;
        int mask = 0;
        std::int64_t write = 0;
        double read = 0.0;

        void setup(double sourceRate, double destRate, int maxBlock);
        void clear();
        void push(const float* input, int count);
        int available(int limit) const;
        void pull(float* output, int count);
        float at(std::int64_t index) const;
        float interpolate(double position) const;
    };

    template <typename Model>
    void exchange(Model&& model);

    void clearQueues();
    void warm();
    int measureLatency();
    void enqueue(const float* samples, int count);
    int dequeue(float* output, int count);

    Stage _up;
    Stage _down;
    std::vector<float> _modelIn;
    std::vector<float> _modelOut;
    std::vector<float> _pending;
    int _pendingRead = 0;
    int _pendingWrite = 0;
    int _pendingCount = 0;
    int _maxHost = 0;
    int _maxModelFrames = 1;
    int _latency = 0;
    int _dropped = 0;
    int _underruns = 0;
    double _hostRate = 0.0;
    double _modelRate = 0.0;
};

inline void SpectraRateAdapter::Stage::setup(double sourceRate, double destRate, int maxBlock) {
    const double safeSource = sourceRate > 0.0 ? sourceRate : 1.0;
    const double safeDest = destRate > 0.0 ? destRate : 1.0;
    step = safeSource / safeDest;
    cutoff = static_cast<float>(std::min(1.0, safeDest / safeSource) * 0.9);
    int size = 1;
    const int need = std::max(8, maxBlock) + kRadius * 8;
    while (size < need) {
        size *= 2;
    }
    ring.assign(static_cast<std::size_t>(size), 0.0f);
    mask = size - 1;
    clear();
}

inline void SpectraRateAdapter::Stage::clear() {
    std::fill(ring.begin(), ring.end(), 0.0f);
    write = 0;
    read = 0.0;
}

inline void SpectraRateAdapter::Stage::push(const float* input, int count) {
    for (int i = 0; i < count; ++i) {
        ring[static_cast<std::size_t>(write) & static_cast<std::size_t>(mask)] = input[i];
        ++write;
    }
}

inline int SpectraRateAdapter::Stage::available(int limit) const {
    const double latest = static_cast<double>(write - kRadius);
    if (!(latest > read) || !(step > 0.0)) {
        return 0;
    }
    const int ready = static_cast<int>(std::floor((latest - read) / step));
    return std::clamp(ready, 0, std::max(0, limit));
}

inline void SpectraRateAdapter::Stage::pull(float* output, int count) {
    for (int i = 0; i < count; ++i) {
        output[i] = interpolate(read);
        read += step;
    }
}

inline float SpectraRateAdapter::Stage::at(std::int64_t index) const {
    if (index < 0 || index >= write || index < write - static_cast<std::int64_t>(ring.size())) {
        return 0.0f;
    }
    return ring[static_cast<std::size_t>(index) & static_cast<std::size_t>(mask)];
}

inline float SpectraRateAdapter::Stage::interpolate(double position) const {
    constexpr float kPi = 3.14159265358979323846f;
    const auto center = static_cast<std::int64_t>(std::floor(position));
    float sum = 0.0f;
    float weight = 0.0f;
    for (int tap = 1 - kRadius; tap <= kRadius; ++tap) {
        const float distance = static_cast<float>(position - static_cast<double>(center + tap));
        const float magnitude = std::abs(distance);
        if (magnitude >= static_cast<float>(kRadius)) {
            continue;
        }
        const float window = 0.5f * (1.0f + std::cos(kPi * distance / static_cast<float>(kRadius)));
        float kernel = cutoff;
        if (magnitude > 1.0e-6f) {
            const float radians = kPi * cutoff * distance;
            kernel = cutoff * std::sin(radians) / radians;
        }
        const float coefficient = kernel * window;
        sum += coefficient * at(center + tap);
        weight += coefficient;
    }
    return weight > 1.0e-6f ? sum / weight : 0.0f;
}

inline void SpectraRateAdapter::prepare(double hostRate, double modelRate, int maxHostBlock) {
    _hostRate = hostRate;
    _modelRate = modelRate;
    _maxHost = std::max(1, maxHostBlock);
    _maxModelFrames = std::max(1, static_cast<int>(std::ceil(static_cast<double>(_maxHost) * modelRate / hostRate)) + 4);
    _up.setup(hostRate, modelRate, _maxHost);
    _down.setup(modelRate, hostRate, _maxModelFrames);
    _modelIn.assign(static_cast<std::size_t>(_maxModelFrames), 0.0f);
    _modelOut.assign(static_cast<std::size_t>(_maxModelFrames), 0.0f);
    _pending.assign(static_cast<std::size_t>(_maxHost * 8 + kRadius * 4), 0.0f);
    clearQueues();
    warm();
    _latency = measureLatency();
    clearQueues();
    _up.clear();
    _down.clear();
    warm();
    _dropped = 0;
    _underruns = 0;
}

inline void SpectraRateAdapter::clearQueues() {
    _pendingRead = 0;
    _pendingWrite = 0;
    _pendingCount = 0;
    std::fill(_pending.begin(), _pending.end(), 0.0f);
    std::fill(_modelIn.begin(), _modelIn.end(), 0.0f);
    std::fill(_modelOut.begin(), _modelOut.end(), 0.0f);
}

inline void SpectraRateAdapter::enqueue(const float* samples, int count) {
    const int capacity = static_cast<int>(_pending.size());
    if (capacity <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (_pendingCount == capacity) {
            _pendingRead = (_pendingRead + 1) % capacity;
            --_pendingCount;
            ++_dropped;
        }
        _pending[static_cast<std::size_t>(_pendingWrite)] = samples[i];
        _pendingWrite = (_pendingWrite + 1) % capacity;
        ++_pendingCount;
    }
}

inline int SpectraRateAdapter::dequeue(float* output, int count) {
    const int capacity = static_cast<int>(_pending.size());
    const int got = std::min(count, _pendingCount);
    for (int i = 0; i < got; ++i) {
        output[i] = _pending[static_cast<std::size_t>(_pendingRead)];
        _pendingRead = (_pendingRead + 1) % capacity;
    }
    _pendingCount -= got;
    return got;
}

template <typename Model>
inline void SpectraRateAdapter::exchange(Model&& model) {
    while (_up.available(_maxModelFrames) > 0) {
        const int frames = _up.available(_maxModelFrames);
        _up.pull(_modelIn.data(), frames);
        model(_modelIn.data(), _modelOut.data(), frames);
        _down.push(_modelOut.data(), frames);
    }
    while (_down.available(_maxModelFrames) > 0) {
        const int frames = _down.available(_maxModelFrames);
        _down.pull(_modelOut.data(), frames);
        enqueue(_modelOut.data(), frames);
    }
}

template <typename Model>
inline void SpectraRateAdapter::process(const float* hostIn, float* hostOut, int hostFrames, Model&& model) {
    if (hostIn == nullptr || hostOut == nullptr || hostFrames <= 0) {
        return;
    }

    int offset = 0;
    while (offset < hostFrames) {
        const int count = std::min(hostFrames - offset, _maxHost);
        _up.push(hostIn + offset, count);
        exchange(model);
        const int got = dequeue(hostOut + offset, count);
        if (got < count) {
            const float hold = got > 0 ? hostOut[offset + got - 1] : 0.0f;
            std::fill(hostOut + offset + got, hostOut + offset + count, hold);
            ++_underruns;
        }
        offset += count;
    }
}

inline void SpectraRateAdapter::warm() {
    const auto identity = [](const float* input, float* output, int frames) {
        std::memcpy(output, input, sizeof(float) * static_cast<std::size_t>(frames));
    };
    std::vector<float> silence(static_cast<std::size_t>(_maxHost), 0.0f);
    std::vector<float> discard(static_cast<std::size_t>(_maxHost), 0.0f);
    for (int block = 0; block < 6; ++block) {
        process(silence.data(), discard.data(), _maxHost, identity);
    }
}

inline int SpectraRateAdapter::measureLatency() {
    const auto identity = [](const float* input, float* output, int frames) {
        std::memcpy(output, input, sizeof(float) * static_cast<std::size_t>(frames));
    };
    constexpr int kLength = 4096;
    std::vector<float> input(static_cast<std::size_t>(kLength), 0.0f);
    std::vector<float> output(static_cast<std::size_t>(kLength), 0.0f);
    input.front() = 1.0f;
    for (int offset = 0; offset < kLength; offset += _maxHost) {
        const int count = std::min(_maxHost, kLength - offset);
        process(input.data() + offset, output.data() + offset, count, identity);
    }

    int peak = 0;
    float peakValue = 0.0f;
    for (int index = 0; index < kLength; ++index) {
        const float value = std::abs(output[static_cast<std::size_t>(index)]);
        if (value > peakValue) {
            peakValue = value;
            peak = index;
        }
    }
    return peakValue > 1.0e-4f ? peak : 0;
}
