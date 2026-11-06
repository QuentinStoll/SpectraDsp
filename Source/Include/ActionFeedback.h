#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

namespace ActionFeedback {
class Sink {
   public:
    virtual ~Sink() = default;
    virtual void reportAction(const juce::String& message, bool error = false) = 0;
};

// Route feedback to the owning editor, keeping separate plugin instances isolated.
inline void post(juce::Component& source, const juce::String& message, bool error = false) {
    if (message.trim().isEmpty()) {
        return;
    }
    for (auto* component = &source; component != nullptr; component = component->getParentComponent()) {
        if (auto* sink = dynamic_cast<Sink*>(component)) {
            sink->reportAction(message, error);
            return;
        }
    }
}

class ParameterChanges {
   public:
    explicit ParameterChanges(const juce::Array<juce::AudioProcessorParameter*>& parameters);
    ~ParameterChanges();
    void setEnabled(bool enabled);
    void discardPending();
    juce::StringArray takeMessages();  // Message thread only.
    static juce::String describe(juce::AudioProcessorParameter& parameter);

   private:
    struct Watcher;
    std::atomic<bool> _enabled{false};
    std::vector<std::unique_ptr<Watcher>> _watchers;
};
}  // namespace ActionFeedback
