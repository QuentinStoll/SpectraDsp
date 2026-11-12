#include "Components/SignalChainStrip.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {
class SignalChainStripTests : public juce::UnitTest {
   public:
    SignalChainStripTests() : juce::UnitTest("SignalChainStrip", "Profiler") {}

    void runTest() override {
        const juce::String name("signal chain selection and deletion across parallel paths");
        if (requestedTestCase.isNotEmpty() && requestedTestCase != name) {
            return;
        }
        beginTest(name);
        SignalChainStrip strip;
        strip.setSize(1000, 400);
        SignalChain::ParallelRoute route;
        route.mergeAfter = 2;
        route.placePathB(SignalChain::Stage::Eq, 0);
        strip.setRoute(route);

        auto selectedCount = [&strip]() {
            int count = 0;
            for (auto* child : strip.getChildren()) {
                if (auto* block = dynamic_cast<SignalChainBlock*>(child); block != nullptr && block->getToggleState()) {
                    ++count;
                }
            }
            return count;
        };

        const auto parallelSlot = SignalChain::pathBSlotForIndex(0);
        strip.selectSlot(parallelSlot);
        expectEquals(strip.getSelectedChainSlot(), parallelSlot);
        expect(strip.selectedStage() == SignalChain::Stage::Eq);
        expectEquals(selectedCount(), 1, "Selecting path B must clear the previous highlight");
        strip.setLayout(strip.getLayout());
        expectEquals(strip.getSelectedChainSlot(), parallelSlot, "Refreshing the main chain must preserve the path B selection");
        strip.selectSlot(SignalChain::pathBSlotForIndex(1));
        expectEquals(strip.getSelectedChainSlot(), parallelSlot, "An empty slot must not steal selection");

        int removedSlot = -1;
        strip.onBlockRemoved = [&removedSlot](int slot) { removedSlot = slot; };
        expect(strip.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
        expectEquals(removedSlot, parallelSlot);

        const auto ampSlot = strip.getLayout().slotFor(SignalChain::Stage::Amp);
        strip.selectSlot(ampSlot);
        expectEquals(selectedCount(), 1, "Returning to the main chain must clear the path B highlight");
        expect(strip.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)));
        expectEquals(removedSlot, ampSlot);
        strip.selectSlot(SignalChain::inputSlot);
        expect(!strip.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)), "Input must not be removable");
        expectEquals(removedSlot, ampSlot);
    }
};

static SignalChainStripTests signalChainStripTests;
}  // namespace
}  // namespace profiler_tests
