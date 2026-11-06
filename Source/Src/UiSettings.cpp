#include "UiSettings.h"

#include "SettingsPath.h"

namespace {
const juce::String uiSettingsFileName{"ui_settings.json"};
const juce::Identifier backgroundColourPresetId{"backgroundColourPreset"};
const juce::Identifier troubleshootingSettingId{"troubleshootingSetting"};
const juce::Identifier hardwareInfoSettingId{"hardwareInfoSetting"};
const juce::Identifier interfaceScalePercentId{"interfaceScalePercent"};
const juce::Identifier highContrastEnabledId{"highContrastEnabled"};

constexpr int defaultBackgroundColourPreset = 1;
constexpr int firstBackgroundColourPreset = 1;
constexpr int lastBackgroundColourPreset = 5;
constexpr int defaultToggleSetting = 1;
constexpr int firstToggleSetting = 1;
constexpr int lastToggleSetting = 2;

juce::File getUiSettingsFile() {
    return getFileInSettingsFolder(uiSettingsFileName);
}

juce::DynamicObject loadUiSettingsObject() {
    juce::DynamicObject settingsObject;
    const auto settingsFile = getUiSettingsFile();

    if (!settingsFile.existsAsFile()) {
        return settingsObject;
    }

    const auto parsed = juce::JSON::parse(settingsFile.loadFileAsString());
    if (auto* loadedSettings = parsed.getDynamicObject()) {
        settingsObject = *loadedSettings;
    }

    return settingsObject;
}

void saveUiSettingsObject(const juce::DynamicObject& settingsObject) {
    juce::String json = "{";
    bool firstProperty = true;

    const auto appendProperty = [&](const juce::Identifier& propertyId) {
        if (!settingsObject.hasProperty(propertyId)) {
            return;
        }

        if (!firstProperty) {
            json << ",";
        }

        firstProperty = false;
        json << "\"" << propertyId.toString() << "\":"
             << settingsObject.getProperty(propertyId).toString();
    };

    appendProperty(backgroundColourPresetId);
    appendProperty(troubleshootingSettingId);
    appendProperty(hardwareInfoSettingId);
    appendProperty(interfaceScalePercentId);
    appendProperty(highContrastEnabledId);

    json << "}";
    getUiSettingsFile().replaceWithText(json);
}

int normaliseToggleSetting(int settingId) {
    if (settingId < firstToggleSetting || settingId > lastToggleSetting) {
        return defaultToggleSetting;
    }

    return settingId;
}

int loadIntSetting(const juce::DynamicObject& settingsObject,
                   const juce::Identifier& propertyId,
                   int defaultValue) {
    if (settingsObject.hasProperty(propertyId)) {
        return static_cast<int>(settingsObject.getProperty(propertyId));
    }

    return defaultValue;
}
}  // namespace

namespace UiSettings {
bool loadHighContrastEnabled() {
    const auto settingsObject = loadUiSettingsObject();
    return loadIntSetting(settingsObject, highContrastEnabledId, 0) == 1;
}

void saveHighContrastEnabled(bool enabled) {
    auto settingsObject = loadUiSettingsObject();
    settingsObject.setProperty(highContrastEnabledId, enabled ? 1 : 0);
    saveUiSettingsObject(settingsObject);
}

int normaliseInterfaceScalePercent(int percent) {
    return percent >= 100 && percent <= 200 && percent % 25 == 0 ? percent : 100;
}

int loadInterfaceScalePercent() {
    const auto settingsObject = loadUiSettingsObject();
    return normaliseInterfaceScalePercent(
        loadIntSetting(settingsObject, interfaceScalePercentId, 100));
}

void saveInterfaceScalePercent(int percent) {
    auto settingsObject = loadUiSettingsObject();
    settingsObject.setProperty(interfaceScalePercentId, normaliseInterfaceScalePercent(percent));
    saveUiSettingsObject(settingsObject);
}

int normaliseBackgroundColourPreset(int presetId) {
    if (presetId < firstBackgroundColourPreset || presetId > lastBackgroundColourPreset) {
        return defaultBackgroundColourPreset;
    }

    return presetId;
}

int loadBackgroundColourPreset() {
    const auto settingsObject = loadUiSettingsObject();
    return normaliseBackgroundColourPreset(
        loadIntSetting(settingsObject, backgroundColourPresetId, defaultBackgroundColourPreset));
}

void saveBackgroundColourPreset(int presetId) {
    auto settingsObject = loadUiSettingsObject();
    settingsObject.setProperty(backgroundColourPresetId,
                               normaliseBackgroundColourPreset(presetId));
    saveUiSettingsObject(settingsObject);
}

int loadTroubleshootingSetting() {
    const auto settingsObject = loadUiSettingsObject();
    return normaliseToggleSetting(
        loadIntSetting(settingsObject, troubleshootingSettingId, defaultToggleSetting));
}

void saveTroubleshootingSetting(int settingId) {
    auto settingsObject = loadUiSettingsObject();
    settingsObject.setProperty(troubleshootingSettingId,
                               normaliseToggleSetting(settingId));
    saveUiSettingsObject(settingsObject);
}

int loadHardwareInfoSetting() {
    const auto settingsObject = loadUiSettingsObject();
    return normaliseToggleSetting(
        loadIntSetting(settingsObject, hardwareInfoSettingId, defaultToggleSetting));
}

void saveHardwareInfoSetting(int settingId) {
    auto settingsObject = loadUiSettingsObject();
    settingsObject.setProperty(hardwareInfoSettingId,
                               normaliseToggleSetting(settingId));
    saveUiSettingsObject(settingsObject);
}
}  // namespace UiSettings
