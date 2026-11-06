#pragma once

#include <JuceHeader.h>

namespace UiSettings {
bool loadActionStatusEnabled();
void saveActionStatusEnabled(bool enabled);

bool loadHighContrastEnabled();
void saveHighContrastEnabled(bool enabled);

int normaliseInterfaceScalePercent(int percent);
int loadInterfaceScalePercent();
void saveInterfaceScalePercent(int percent);

int loadBackgroundColourPreset();
void saveBackgroundColourPreset(int presetId);
int normaliseBackgroundColourPreset(int presetId);

int loadTroubleshootingSetting();
void saveTroubleshootingSetting(int settingId);

int loadHardwareInfoSetting();
void saveHardwareInfoSetting(int settingId);
}  // namespace UiSettings
