#include <JuceHeader.h>

#include <utility>

#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ProfileCatalog.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {

struct ScopedTestFolder {
    explicit ScopedTestFolder(juce::File folderIn)
        : folder(std::move(folderIn)) {}

    ~ScopedTestFolder() {
        if (folder != juce::File{}) {
            folder.deleteRecursively();
        }
    }

    ScopedTestFolder(const ScopedTestFolder&) = delete;
    ScopedTestFolder& operator=(const ScopedTestFolder&) = delete;

    juce::File folder;
};

class PresetCatalogTests : public juce::UnitTest {
   public:
    PresetCatalogTests() : juce::UnitTest("preset and catalog", "Profiler") {}

    void runTest() override {
        runCase("preset restores the chain", [this] { testPresetRestoresChain(); });
        runCase("catalog keeps profiles encrypted", [this] { testCatalogKeepsProfilesEncrypted(); });
    }

   private:
    template <typename Function>
    void runCase(const juce::String& testCaseName, Function&& testCase) {
        if (requestedTestCase.isNotEmpty() && requestedTestCase != testCaseName) {
            return;
        }
        beginTest(testCaseName);
        testCase();
    }

    ScopedTestFolder makeTestFolder() {
        const auto baseFolder = juce::File(PROFILER_TEST_TEMP_ROOT);
        expect(baseFolder.createDirectory().wasOk());
        const auto folder = baseFolder.getChildFile(juce::Uuid().toDashedString());
        expect(folder.createDirectory().wasOk());
        return ScopedTestFolder(folder);
    }

    void testPresetRestoresChain() {
        auto root = makeTestFolder();
        auto profiles = root.folder.getChildFile("profiles");
        auto settings = root.folder.getChildFile("settings.json");
        auto presets = root.folder.getChildFile("presets");
        auto catalog = root.folder.getChildFile("catalog");
        expect(profiles.createDirectory().wasOk());
        expect(presets.createDirectory().wasOk());
        expect(catalog.createDirectory().wasOk());

        ProfilerAudioProcessor processor(profiles, settings, presets, catalog);
        auto* master = processor._apvts.getParameter("master");
        expect(master != nullptr);
        master->setValueNotifyingHost(master->convertTo0to1(82.0f));
        processor.clearAllChainBlocks();
        processor.placeChainStage(SignalChain::Stage::Pedal, 3);
        processor.placeChainStage(SignalChain::Stage::Amp, 5);

        juce::String error;
        expect(processor.saveCurrentPreset("Heavy", &error), error);

        master->setValueNotifyingHost(master->convertTo0to1(10.0f));
        processor.resetChainLayout();

        expect(processor.applyPreset(0, &error), error);
        expectWithinAbsoluteError(master->convertFrom0to1(master->getValue()), 82.0f, 0.01f);

        const auto layout = processor.getChainLayout();
        expect(layout.atSlot(3) == SignalChain::Stage::Pedal);
        expect(layout.atSlot(5) == SignalChain::Stage::Amp);
        expect(layout.slotFor(SignalChain::Stage::Cab) < 0);

        const auto savedFile = presets.findChildFiles(juce::File::findFiles, false, "*.spectrapreset")[0];
        const auto json = savedFile.loadFileAsString();
        expect(json.contains("ampProfileId"));
        expect(!json.contains("\"irPath\""));
        expect(!json.contains("\"ampPath\""));
    }

    void testCatalogKeepsProfilesEncrypted() {
        auto root = makeTestFolder();
        ProfileCatalog catalog(root.folder);
        juce::MemoryBlock cipher;
        cipher.setSize(16);
        cipher.fillWith(0x11);
        juce::MemoryBlock key;
        key.setSize(32);
        key.fillWith(0xAB);

        juce::String error;
        expect(catalog.storeCipher("amp-1", "Clean", ProfileCatalog::Kind::Amp, cipher, 1, 40.0f, false, &error), error);
        catalog.rememberKey("amp-1", ProfileCatalog::Kind::Amp, key);

        const auto files = root.folder.findChildFiles(juce::File::findFiles, true, "*");
        expect(files.size() >= 2);
        bool sawCipher = false;
        for (const auto& file : files) {
            expect(file.getFileExtension() == ".spectra" || file.getFileName() == "catalog.json");
            juce::MemoryBlock bytes;
            expect(file.loadFileAsData(bytes));
            expect(bytes.getSize() < 4 || static_cast<unsigned char>(bytes[0]) != 0xAB);
            if (file.hasFileExtension(".spectra")) {
                sawCipher = true;
                expect(bytes.getSize() == cipher.getSize());
            }
        }
        expect(sawCipher);

        ProfileCatalog reloaded(root.folder);
        juce::MemoryBlock storedKey;
        expect(!reloaded.copyKey("amp-1", ProfileCatalog::Kind::Amp, storedKey));
        juce::MemoryBlock storedCipher;
        expect(reloaded.readCipher("amp-1", ProfileCatalog::Kind::Amp, storedCipher));
        expect(storedCipher.getSize() == cipher.getSize());
        const auto* entry = reloaded.find("amp-1", ProfileCatalog::Kind::Amp);
        expect(entry != nullptr);
        if (entry != nullptr) {
            expectEquals(entry->title, juce::String("Clean"));
            expect(entry->cipherOnDisk);
        }
    }
};

PresetCatalogTests presetCatalogTests;

}  // namespace
}  // namespace profiler_tests
