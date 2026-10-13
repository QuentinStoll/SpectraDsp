#include "SpectraAmp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "SpectraNeuralModel.h"

namespace {
constexpr std::size_t kMaxModelBytes = 32u * 1024u * 1024u;
constexpr int kResampleLatencySamples = 1;

bool needsExternalResample(double modelRate, double hostRate, bool arbitraryRate) {
    if (arbitraryRate || !(modelRate > 0.0) || !(hostRate > 0.0)) {
        return false;
    }
    return std::abs(modelRate - hostRate) > 1.0;
}

void resampleLinear(const float* input,
                    int inputCount,
                    float* output,
                    int outputCount,
                    float& previous,
                    double& cursor) {
    if (input == nullptr || output == nullptr || inputCount <= 0 || outputCount <= 0) {
        return;
    }

    const double step = static_cast<double>(inputCount) / static_cast<double>(outputCount);
    for (int i = 0; i < outputCount; ++i) {
        const double position = cursor + static_cast<double>(i) * step;
        const int index = static_cast<int>(std::floor(position));
        const float fraction = static_cast<float>(position - static_cast<double>(index));
        const float left = index <= 0 ? previous : input[std::clamp(index - 1, 0, inputCount - 1)];
        const int rightIndex = std::clamp(index, 0, inputCount - 1);
        const float right = input[rightIndex];
        output[i] = left + (right - left) * fraction;
    }

    cursor += static_cast<double>(outputCount) * step - static_cast<double>(inputCount);
    if (!std::isfinite(cursor) || std::abs(cursor) > 1.0) {
        cursor = 0.0;
    }
    previous = input[inputCount - 1];
}
}  // namespace

struct SpectraAmp::Instance {
    std::unique_ptr<SpectraNeuralModel> model;
    std::vector<float> hostIn;
    std::vector<float> hostOut;
    std::vector<float> modelIn;
    std::vector<float> modelOut;
    float previousIn = 0.0f;
    float previousOut = 0.0f;
    double cursorIn = 0.0;
    double cursorOut = 0.0;
    float delaySample = 0.0f;
    bool ready = false;
};

struct SpectraAmp::Engine {
    std::vector<Instance> instances;
    double modelRate = -1.0;
    double hostRate = 0.0;
    int maxHostBlock = 0;
    int modelBlock = 0;
    int latency = 0;
    bool resample = false;
    bool arbitraryRate = false;
};

SpectraAmp::SpectraAmp() = default;

SpectraAmp::~SpectraAmp() {
    clear();
}

SpectraAmp::LoadStatus SpectraAmp::load(const void* data, std::size_t size) {
    if (data == nullptr || size == 0 || size > kMaxModelBytes) {
        return LoadStatus::Invalid;
    }

    std::string text(static_cast<const char*>(data), size);
    _source = std::move(text);
    std::unique_ptr<SpectraNeuralModel> probed;
    const auto status = SpectraNeuralModel::load(_source, probed);
    probed.reset();
    if (status == SpectraNeuralModel::LoadStatus::NotNeural) {
        _source.clear();
        return LoadStatus::NotNeural;
    }
    if (status != SpectraNeuralModel::LoadStatus::Loaded) {
        _source.clear();
        return LoadStatus::Invalid;
    }

    auto engine = build(1);
    if (engine == nullptr) {
        _source.clear();
        return LoadStatus::Invalid;
    }

    if (!publish(std::move(engine))) {
        if (!isLoaded()) {
            _source.clear();
        }
        return LoadStatus::Invalid;
    }
    return LoadStatus::Loaded;
}

void SpectraAmp::clear() {
    _source.clear();
    _active.store(-1, std::memory_order_release);
    _latency.store(0, std::memory_order_relaxed);

    const auto start = std::chrono::steady_clock::now();
    while ((std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(1000)) {
        if (_readers[0].load(std::memory_order_acquire) == 0 && _readers[1].load(std::memory_order_acquire) == 0) {
            break;
        }
        std::this_thread::yield();
    }
    retire();
}

void SpectraAmp::prepare(double sampleRate, int maxBlockSize) {
    _hostRate = sampleRate;
    _maxBlock = std::max(1, maxBlockSize);

    const int slot = _active.load(std::memory_order_acquire);
    if (slot < 0 || _owned[slot] == nullptr) {
        _latency.store(0, std::memory_order_relaxed);
        return;
    }

    try {
        configure(*_owned[slot]);
    } catch (const std::exception&) {
        _latency.store(0, std::memory_order_relaxed);
        return;
    }
    _latency.store(_owned[slot]->latency, std::memory_order_relaxed);
}

void SpectraAmp::setInstanceCount(int count) {
    if (_source.empty()) {
        return;
    }

    count = std::max(1, count);
    const int slot = _active.load(std::memory_order_acquire);
    if (slot >= 0 && _owned[slot] != nullptr && static_cast<int>(_owned[slot]->instances.size()) == count) {
        return;
    }

    auto engine = build(count);
    if (engine != nullptr) {
        publish(std::move(engine));
    }
}

bool SpectraAmp::render(int instance, const float* input, float* wet, int numSamples) noexcept {
    if (input == nullptr || wet == nullptr || numSamples <= 0) {
        return false;
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        const int slot = _active.load(std::memory_order_acquire);
        if (slot < 0 || _owned[slot] == nullptr) {
            return false;
        }

        _readers[slot].fetch_add(1, std::memory_order_acq_rel);
        if (_active.load(std::memory_order_acquire) == slot) {
            const bool rendered = renderEngine(*_owned[slot], instance, input, wet, numSamples);
            _readers[slot].fetch_sub(1, std::memory_order_release);
            return rendered;
        }
        _readers[slot].fetch_sub(1, std::memory_order_release);
    }

    return false;
}

int SpectraAmp::latencySamples() const noexcept {
    return _latency.load(std::memory_order_relaxed);
}

bool SpectraAmp::isLoaded() const noexcept {
    return _active.load(std::memory_order_acquire) >= 0;
}

std::unique_ptr<SpectraAmp::Engine> SpectraAmp::build(int instanceCount) const {
    if (_source.empty() || instanceCount <= 0) {
        return nullptr;
    }

    std::unique_ptr<SpectraNeuralModel> prototype;
    const auto status = SpectraNeuralModel::load(_source, prototype);
    if (status != SpectraNeuralModel::LoadStatus::Loaded || prototype == nullptr) {
        return nullptr;
    }

    auto engine = std::make_unique<Engine>();
    engine->modelRate = prototype->expectedSampleRate();
    engine->arbitraryRate = prototype->supportsArbitrarySampleRate();
    engine->instances.resize(static_cast<std::size_t>(instanceCount));
    engine->instances.front().model = std::move(prototype);
    for (int index = 1; index < instanceCount; ++index) {
        engine->instances[static_cast<std::size_t>(index)].model = engine->instances.front().model->clone();
        if (engine->instances[static_cast<std::size_t>(index)].model == nullptr) {
            return nullptr;
        }
    }

    try {
        configure(*engine);
    } catch (const std::exception&) {
        return nullptr;
    }
    return engine;
}

void SpectraAmp::configure(Engine& engine) const {
    engine.hostRate = _hostRate;
    engine.maxHostBlock = std::max(1, _maxBlock);
    engine.resample = needsExternalResample(engine.modelRate, _hostRate, engine.arbitraryRate);
    engine.latency = engine.resample ? kResampleLatencySamples : 0;

    const double ratio = engine.resample ? engine.modelRate / _hostRate : 1.0;
    engine.modelBlock = engine.resample
                            ? static_cast<int>(std::ceil(static_cast<double>(engine.maxHostBlock) * ratio)) + 8
                            : engine.maxHostBlock;
    engine.modelBlock = std::max(engine.modelBlock, 1);

    if (!(_hostRate > 0.0)) {
        return;
    }

    const double resetRate = engine.resample ? engine.modelRate : _hostRate;
    const int resetBlock = engine.resample ? engine.modelBlock : engine.maxHostBlock;
    for (auto& instance : engine.instances) {
        if (instance.model == nullptr) {
            instance.ready = false;
            continue;
        }
        instance.ready = false;
        instance.model->reset(resetRate, resetBlock);
        instance.hostIn.assign(static_cast<std::size_t>(engine.maxHostBlock), 0.0f);
        instance.hostOut.assign(static_cast<std::size_t>(engine.maxHostBlock), 0.0f);
        instance.modelIn.assign(static_cast<std::size_t>(engine.modelBlock), 0.0f);
        instance.modelOut.assign(static_cast<std::size_t>(engine.modelBlock), 0.0f);
        instance.previousIn = 0.0f;
        instance.previousOut = 0.0f;
        instance.cursorIn = 0.0;
        instance.cursorOut = 0.0;
        instance.delaySample = 0.0f;
        instance.ready = true;
    }
}

bool SpectraAmp::publish(std::unique_ptr<Engine> engine) {
    if (engine == nullptr) {
        return false;
    }

    const int active = _active.load(std::memory_order_acquire);
    const int next = active < 0 ? 0 : 1 - active;
    const auto start = std::chrono::steady_clock::now();
    while (_readers[next].load(std::memory_order_acquire) != 0) {
        if ((std::chrono::steady_clock::now() - start) > std::chrono::milliseconds(1000)) {
            return false;
        }
        std::this_thread::yield();
    }

    const int latency = engine->latency;
    _owned[next] = std::move(engine);
    _latency.store(latency, std::memory_order_relaxed);
    _active.store(next, std::memory_order_release);
    retire();
    return true;
}

void SpectraAmp::retire() {
    const int active = _active.load(std::memory_order_acquire);
    for (int slot = 0; slot < 2; ++slot) {
        if (slot == active) {
            continue;
        }
        if (_readers[slot].load(std::memory_order_acquire) == 0) {
            _owned[slot].reset();
        }
    }
}

bool SpectraAmp::renderEngine(Engine& engine, int instance, const float* input, float* wet, int numSamples) noexcept {
    if (instance < 0 || instance >= static_cast<int>(engine.instances.size()) || engine.maxHostBlock <= 0) {
        return false;
    }

    auto& slot = engine.instances[static_cast<std::size_t>(instance)];
    if (!slot.ready || slot.model == nullptr) {
        return false;
    }

    int offset = 0;
    while (offset < numSamples) {
        const int count = std::min(numSamples - offset, engine.maxHostBlock);
        const float* in = input + offset;
        float* out = wet + offset;
        try {
            if (!engine.resample) {
                std::memcpy(slot.hostIn.data(), in, sizeof(float) * static_cast<std::size_t>(count));
                slot.model->process(slot.hostIn.data(), out, count);
            } else {
                const int modelFrames = std::clamp(
                    static_cast<int>(std::lround(static_cast<double>(count) * engine.modelRate / engine.hostRate)),
                    1,
                    engine.modelBlock);
                resampleLinear(in, count, slot.modelIn.data(), modelFrames, slot.previousIn, slot.cursorIn);
                slot.model->process(slot.modelIn.data(), slot.modelOut.data(), modelFrames);
                resampleLinear(slot.modelOut.data(), modelFrames, slot.hostOut.data(), count, slot.previousOut, slot.cursorOut);
                out[0] = slot.delaySample;
                if (count > 1) {
                    std::memcpy(out + 1, slot.hostOut.data(), sizeof(float) * static_cast<std::size_t>(count - 1));
                }
                slot.delaySample = slot.hostOut[static_cast<std::size_t>(count - 1)];
            }
        } catch (const std::exception&) {
            return false;
        }
        offset += count;
    }

    return true;
}
