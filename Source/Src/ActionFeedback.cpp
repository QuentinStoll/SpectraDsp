#include "ActionFeedback.h"

#include "EqBandLayout.h"
#include "Fx/FxCatalog.h"

namespace ActionFeedback {
struct ParameterChanges::Watcher : juce::AudioProcessorParameter::Listener {
    Watcher(juce::AudioProcessorParameter& value, std::atomic<bool>& enabled)
        : parameter(value), active(enabled) { parameter.addListener(this); }
    ~Watcher() override { parameter.removeListener(this); }
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override {
        // No strings, GUI calls, or allocations on a host/audio callback.
        if (!starting && active.load(std::memory_order_relaxed)) {
            pending.store(true, std::memory_order_release);
        }
    }
    juce::AudioProcessorParameter& parameter;
    std::atomic<bool>& active;
    std::atomic<bool> pending{false};
};

ParameterChanges::ParameterChanges(const juce::Array<juce::AudioProcessorParameter*>& parameters) {
    for (auto* parameter : parameters) {
        _watchers.push_back(std::make_unique<Watcher>(*parameter, _enabled));
    }
}
ParameterChanges::~ParameterChanges() = default;

void ParameterChanges::setEnabled(bool enabled) {
    _enabled.store(enabled, std::memory_order_relaxed);
    discardPending();
}
void ParameterChanges::discardPending() {
    for (const auto& watcher : _watchers) {
        watcher->pending.store(false, std::memory_order_relaxed);
    }
}
juce::StringArray ParameterChanges::takeMessages() {
    juce::StringArray result;
    for (const auto& watcher : _watchers) {
        if (watcher->pending.exchange(false, std::memory_order_acquire) && _enabled.load(std::memory_order_relaxed)) {
            result.add(describe(watcher->parameter));
        }
    }
    return result;
}

juce::String ParameterChanges::describe(juce::AudioProcessorParameter& parameter) {
    auto name = parameter.getName(128);
    auto value = parameter.getText(parameter.getValue(), 128).trim();
    if (dynamic_cast<juce::AudioParameterBool*>(&parameter) != nullptr) {
        value = parameter.getValue() >= 0.5f ? "On" : "Off";
    }
    auto unit = parameter.getLabel().trim();
    if (unit.isEmpty()) {
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(&parameter)) {
            const auto& id = identified->paramID;
            if (id == "gain" || id == "noise" || id == "input" || id == "output" || id == "pedalLevel") unit = "dB";
            else if (id == "master" || id == "pedalTone" || id == "conditionAmount") unit = "%";
            else if (id == "cabLowCut") unit = "Hz";
            for (const auto& band : EqBands::specs) {
                if (id == band.gainId) unit = "dB";
                if (id == band.freqId) unit = "Hz";
            }
            for (int i = 0; i < Fx::moduleCount(); ++i) {
                const auto& module = Fx::moduleAt(i);
                for (int j = 0; j < module.paramCount; ++j) {
                    const auto& spec = module.params[j];
                    if (id == spec.id) {
                        if (spec.suffix != nullptr) unit = spec.suffix;
                        name = juce::String(module.name) + " " + name;
                    }
                }
            }
        }
    }
    return name + ": " + value + (unit.isNotEmpty() ? " " + unit : "");
}
}  // namespace ActionFeedback
