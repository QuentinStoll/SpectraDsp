#include "StandaloneAudio.h"

#if JUCE_WINDOWS && JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace StandaloneAudio {

#if JUCE_WINDOWS && JucePlugin_Build_Standalone
namespace {
constexpr const char* kExclusiveType = "Windows Audio (Exclusive Mode)";
constexpr const char* kSharedType = "Windows Audio";
constexpr const char* kMigratedKey = "spectraLeftExclusiveAudio";
constexpr double kPreferredRate = 48000.0;

void markMigrated(juce::PropertySet* settings) {
    if (settings != nullptr) {
        settings->setValue(kMigratedKey, true);
    }
}
}  // namespace

void moveOffExclusiveMode() {
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr) {
        return;
    }

    auto* settings = holder->settings.get();
    if (settings != nullptr && settings->getBoolValue(kMigratedKey, false)) {
        return;
    }

    auto& manager = holder->deviceManager;
    if (manager.getCurrentAudioDevice() == nullptr) {
        // Nothing is open, for instance because the audio interface is unplugged. Leave the saved
        // setup alone and check again on a later launch.
        return;
    }

    if (manager.getCurrentAudioDeviceType() != kExclusiveType) {
        // Already shared. Remember it, so a later, deliberate switch to exclusive mode is kept.
        markMigrated(settings);
        return;
    }

    const auto before = manager.getAudioDeviceSetup();
    manager.setCurrentAudioDeviceType(kSharedType, false);

    // The type switch picks the system default devices. Go back to the devices that were in use.
    auto setup = manager.getAudioDeviceSetup();
    setup.inputDeviceName = before.inputDeviceName;
    setup.outputDeviceName = before.outputDeviceName;
    setup.inputChannels = before.inputChannels;
    setup.outputChannels = before.outputChannels;
    setup.useDefaultInputChannels = before.useDefaultInputChannels;
    setup.useDefaultOutputChannels = before.useDefaultOutputChannels;
    setup.sampleRate = kPreferredRate;
    setup.bufferSize = 0;  // the device's default period

    const auto error = manager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty() || manager.getCurrentAudioDevice() == nullptr) {
        // Shared mode cannot open those devices. Put the previous setup back rather than lose audio.
        DBG("Spectra: shared mode failed, restoring exclusive mode: " << error);
        manager.setCurrentAudioDeviceType(kExclusiveType, false);
        manager.setAudioDeviceSetup(before, true);
    }

    markMigrated(settings);
    holder->saveAudioDeviceState();
}
#else
void moveOffExclusiveMode() {}
#endif

}  // namespace StandaloneAudio
