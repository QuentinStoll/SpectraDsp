#include "Components/ScalableViewport.h"
#include "ProfilerConstantValues.h"
#include "SettingsPath.h"
#include "TestRunner.h"
#include "UiSettings.h"

namespace profiler_tests {
namespace {

class ScopedUiSettingsFile {
   public:
    ScopedUiSettingsFile()
        : file(getFileInSettingsFolder("ui_settings.json")),
          existed(file.existsAsFile()),
          originalContents(existed ? file.loadFileAsString() : juce::String{}) {}

    ~ScopedUiSettingsFile() {
        if (existed) {
            file.replaceWithText(originalContents);
        } else {
            file.deleteFile();
        }
    }

    juce::File file;

   private:
    bool existed;
    juce::String originalContents;
};

class UiSettingsUnitTests : public juce::UnitTest {
   public:
    UiSettingsUnitTests()
        : juce::UnitTest("UiSettings", "Profiler") {}

    void runTest() override {
        runCase("ui settings defaults and normalisation", [this] {
            testDefaultsAndNormalisation();
        });
        runCase("ui settings persistence", [this] {
            testPersistence();
        });
        runCase("ui settings invalid JSON", [this] {
            testInvalidJson();
        });
        runCase("interface scaling keeps controls reachable", [this] {
            ScalableViewport viewport(editorWidth, editorHeight);
            for (const int percent : {100, 125, 150, 175, 200}) {
                viewport.setBounds(0, 0, editorWidth, editorHeight);
                viewport.setScalePercent(percent);
                auto& content = viewport.getContent();
                expectEquals(content.getWidth(), editorWidth);
                expectEquals(content.getHeight(), editorHeight);
                expectWithinAbsoluteError(content.getTransform().mat00, percent / 100.0f, 0.001f);
                auto* canvas = viewport.getViewedComponent();
                expectEquals(canvas->getWidth(), juce::roundToInt(editorWidth * percent / 100.0f));
                expectEquals(canvas->getHeight(), juce::roundToInt(editorHeight * percent / 100.0f));

                viewport.setViewPosition(canvas->getWidth(), canvas->getHeight());
                expectEquals(viewport.getViewPositionX() + viewport.getMaximumVisibleWidth(), canvas->getWidth());
                expectEquals(viewport.getViewPositionY() + viewport.getMaximumVisibleHeight(), canvas->getHeight());
                viewport.setScalePercent(100);
                expectEquals(viewport.getViewPositionX(), 0);
                expectEquals(viewport.getViewPositionY(), 0);
                expect(!viewport.getHorizontalScrollBar().isVisible());
                expect(!viewport.getVerticalScrollBar().isVisible());
            }

            viewport.setSize(editorMinWidth, editorMinHeight);
            viewport.setScalePercent(200);
            expect(viewport.getHorizontalScrollBar().isVisible());
            expect(viewport.getVerticalScrollBar().isVisible());
            viewport.setSize(editorWidth * 3, editorHeight * 3);
            expect(!viewport.getHorizontalScrollBar().isVisible());
            expect(!viewport.getVerticalScrollBar().isVisible());
            expectEquals(viewport.getViewedComponent()->getWidth(), viewport.getWidth());
            expectEquals(viewport.getViewedComponent()->getHeight(), viewport.getHeight());
        });
    }

   private:
    template <typename Fn>
    void runCase(const juce::String& name, Fn&& fn) {
        if (requestedTestCase.isNotEmpty() && requestedTestCase != name) {
            return;
        }

        beginTest(name);
        fn();
    }

    void testDefaultsAndNormalisation() {
        ScopedUiSettingsFile settings;
        settings.file.deleteFile();

        expectEquals(UiSettings::loadBackgroundColourPreset(), 1);
        expectEquals(UiSettings::loadTroubleshootingSetting(), 1);
        expectEquals(UiSettings::loadHardwareInfoSetting(), 1);
        expectEquals(UiSettings::loadInterfaceScalePercent(), 100);
        for (const int percent : {100, 125, 150, 175, 200}) {
            expectEquals(UiSettings::normaliseInterfaceScalePercent(percent), percent);
        }
        for (const int percent : {-100, 0, 99, 101, 201, 999}) {
            expectEquals(UiSettings::normaliseInterfaceScalePercent(percent), 100);
        }
        expectEquals(UiSettings::normaliseBackgroundColourPreset(3), 3);
        expectEquals(UiSettings::normaliseBackgroundColourPreset(0), 1);
        expectEquals(UiSettings::normaliseBackgroundColourPreset(6), 1);
    }

    void testPersistence() {
        ScopedUiSettingsFile settings;
        settings.file.deleteFile();

        UiSettings::saveBackgroundColourPreset(4);
        UiSettings::saveInterfaceScalePercent(175);
        UiSettings::saveTroubleshootingSetting(2);
        UiSettings::saveHardwareInfoSetting(2);

        expectEquals(UiSettings::loadBackgroundColourPreset(), 4);
        expectEquals(UiSettings::loadTroubleshootingSetting(), 2);
        expectEquals(UiSettings::loadHardwareInfoSetting(), 2);
        expectEquals(UiSettings::loadInterfaceScalePercent(), 175);

        UiSettings::saveInterfaceScalePercent(150);
        expectEquals(UiSettings::loadBackgroundColourPreset(), 4);
        expectEquals(UiSettings::loadTroubleshootingSetting(), 2);
        expectEquals(UiSettings::loadHardwareInfoSetting(), 2);
        expectEquals(UiSettings::loadInterfaceScalePercent(), 150);

        UiSettings::saveBackgroundColourPreset(99);
        UiSettings::saveTroubleshootingSetting(99);
        UiSettings::saveHardwareInfoSetting(99);
        UiSettings::saveInterfaceScalePercent(99);

        expectEquals(UiSettings::loadBackgroundColourPreset(), 1);
        expectEquals(UiSettings::loadTroubleshootingSetting(), 1);
        expectEquals(UiSettings::loadHardwareInfoSetting(), 1);
        expectEquals(UiSettings::loadInterfaceScalePercent(), 100);
    }

    void testInvalidJson() {
        ScopedUiSettingsFile settings;
        expect(settings.file.replaceWithText("not valid JSON"));

        expectEquals(UiSettings::loadBackgroundColourPreset(), 1);
        expectEquals(UiSettings::loadTroubleshootingSetting(), 1);
        expectEquals(UiSettings::loadHardwareInfoSetting(), 1);
        expectEquals(UiSettings::loadInterfaceScalePercent(), 100);
    }
};

UiSettingsUnitTests uiSettingsUnitTests;

}  // namespace
}  // namespace profiler_tests
