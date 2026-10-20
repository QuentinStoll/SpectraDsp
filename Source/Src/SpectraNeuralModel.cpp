#include "SpectraNeuralModel.h"

#include <NAM/get_dsp.h>

#include <exception>
#include <utility>

struct SpectraNeuralModel::Impl {
    std::unique_ptr<nam::DSP> model;
    std::string source;
    double expectedRate = -1.0;
    bool arbitraryRate = false;
};

SpectraNeuralModel::SpectraNeuralModel() = default;

SpectraNeuralModel::~SpectraNeuralModel() = default;

SpectraNeuralModel::LoadStatus SpectraNeuralModel::load(const std::string& json, std::unique_ptr<SpectraNeuralModel>& out) {
    out.reset();

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json);
    } catch (const std::exception&) {
        return LoadStatus::NotNeural;
    }

    if (!parsed.is_object() || !parsed.contains("architecture") || !parsed["architecture"].is_string()) {
        return LoadStatus::NotNeural;
    }

    try {
        nam::DspLoadOptions options;
        // Skip the constructor prewarm: Reset below runs it once the host rate and
        // block size are known, still off the audio thread.
        options.prewarm = false;
        auto model = nam::get_dsp(parsed, options);
        if (model == nullptr) {
            return LoadStatus::Invalid;
        }

        auto loaded = std::unique_ptr<SpectraNeuralModel>(new SpectraNeuralModel());
        loaded->_impl = std::make_unique<Impl>();
        loaded->_impl->expectedRate = model->GetExpectedSampleRate();
        loaded->_impl->arbitraryRate = model->SupportsArbitrarySampleRate();
        loaded->_impl->model = std::move(model);
        loaded->_impl->source = json;
        out = std::move(loaded);
        return LoadStatus::Loaded;
    } catch (const std::exception&) {
        return LoadStatus::Invalid;
    }
}

std::unique_ptr<SpectraNeuralModel> SpectraNeuralModel::clone() const {
    if (_impl == nullptr || _impl->source.empty()) {
        return nullptr;
    }

    std::unique_ptr<SpectraNeuralModel> copy;
    if (load(_impl->source, copy) != LoadStatus::Loaded) {
        return nullptr;
    }
    return copy;
}

double SpectraNeuralModel::expectedSampleRate() const {
    return _impl != nullptr ? _impl->expectedRate : -1.0;
}

bool SpectraNeuralModel::supportsArbitrarySampleRate() const {
    return _impl != nullptr && _impl->arbitraryRate;
}

bool SpectraNeuralModel::hasInputLevel() {
    return _impl != nullptr && _impl->model != nullptr && _impl->model->HasInputLevel();
}

bool SpectraNeuralModel::hasOutputLevel() {
    return _impl != nullptr && _impl->model != nullptr && _impl->model->HasOutputLevel();
}

double SpectraNeuralModel::inputLevelDbu() {
    return hasInputLevel() ? _impl->model->GetInputLevel() : 0.0;
}

double SpectraNeuralModel::outputLevelDbu() {
    return hasOutputLevel() ? _impl->model->GetOutputLevel() : 0.0;
}

void SpectraNeuralModel::reset(double sampleRate, int maxBlock) {
    if (_impl != nullptr && _impl->model != nullptr) {
        _impl->model->SetPrewarmOnReset(true);
        _impl->model->Reset(sampleRate, maxBlock);
    }
}

void SpectraNeuralModel::process(float* input, float* output, int frames) {
    if (_impl == nullptr || _impl->model == nullptr || input == nullptr || output == nullptr || frames <= 0) {
        return;
    }

    float* inputs[] = {input};
    float* outputs[] = {output};
    _impl->model->process(inputs, outputs, frames);
}
