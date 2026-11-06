#include "ActionFeedback.h"
#include "Components/ActionStatusBar.h"
#include "Components/NotificationBanner.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {
struct TestSink : juce::Component, ActionFeedback::Sink {
    void reportAction(const juce::String& message, bool error) override {
        latest = message;
        failed = error;
    }
    juce::String latest;
    bool failed = false;
};

class ActionFeedbackTests : public juce::UnitTest {
   public:
    ActionFeedbackTests() : juce::UnitTest("ActionFeedback", "Profiler") {}
    void runTest() override {
        const juce::String caseName("action feedback routing and user gestures");
        if (requestedTestCase.isNotEmpty() && requestedTestCase != caseName) return;
        beginTest(caseName);
        TestSink first, second;
        juce::Component nested, control;
        first.addAndMakeVisible(nested);
        nested.addAndMakeVisible(control);
        ActionFeedback::post(control, "Settings opened.");
        expectEquals(first.latest, juce::String("Settings opened."));
        expect(second.latest.isEmpty());
        NotificationBanner notification;
        nested.addAndMakeVisible(notification);
        notification.showMessage("Could not load profile", NotificationBanner::Type::Error, 0);
        expectEquals(first.latest, juce::String("Could not load profile"));
        expect(first.failed);
        ActionFeedback::post(control, " ");
        expect(first.failed, "Empty messages must not erase the latest result");

        juce::AudioParameterFloat volume(juce::ParameterID{"master", 1}, "Master Volume",
                                         juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f);
        juce::AudioParameterBool mute(juce::ParameterID{"mute", 1}, "Mute", false);
        ActionFeedback::ParameterChanges changes({&volume, &mute});
        volume.beginChangeGesture();
        volume.setValueNotifyingHost(0.6f);
        volume.endChangeGesture();
        expect(changes.takeMessages().isEmpty(), "Feedback must be opt-in");
        changes.setEnabled(true);
        volume.setValueNotifyingHost(0.7f);
        expect(changes.takeMessages().isEmpty(), "Automation must not be mistaken for a UI gesture");
        volume.beginChangeGesture();
        volume.setValueNotifyingHost(0.8f);
        expect(changes.takeMessages().isEmpty(), "Wait for the final drag value");
        volume.endChangeGesture();
        auto messages = changes.takeMessages();
        expectEquals(messages.size(), 1);
        expect(messages[0].contains("Master Volume:") && messages[0].contains("80") && messages[0].contains("%"));
        expect(changes.takeMessages().isEmpty(), "Each gesture should be reported once");
        mute.beginChangeGesture();
        mute.setValueNotifyingHost(1.0f);
        mute.endChangeGesture();
        expectEquals(changes.takeMessages().joinIntoString(""), juce::String("Mute: On"));
        volume.beginChangeGesture();
        volume.endChangeGesture();
        changes.discardPending();
        expect(changes.takeMessages().isEmpty(), "An explicit result must supersede queued control feedback");
        changes.setEnabled(false);
        volume.beginChangeGesture();
        volume.endChangeGesture();
        changes.setEnabled(true);
        expect(changes.takeMessages().isEmpty(), "Re-enabling must not replay old actions");

        ActionStatusBar bar;
        bar.setSize(500, 64);
        const juce::String longMessage = juce::String::repeatedString("A long readable result. ", 30);
        bar.showMessage(longMessage);
        expectEquals(bar.getMessage(), longMessage, "Keep the complete message, including text that needs scrolling");
        bar.showMessage("Import failed.", true);
        expectEquals(bar.getMessage(), juce::String("Error: Import failed."));
    }
};
ActionFeedbackTests actionFeedbackTests;
}  // namespace
}  // namespace profiler_tests
