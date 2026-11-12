#pragma once
#include <JuceHeader.h>

/**
 * Returns the JUCE File object pointing to the application's data subfolder.
 * Path: [AppData]/[PluginName]
 */
inline juce::File getSettingsFolder() {
    juce::File rootDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File subFolder = rootDir.getChildFile(ProjectInfo::projectName);
    if (!subFolder.exists())
        subFolder.createDirectory();
    return subFolder;
}

/**
 * Attempts to find a file by name within the settings folder.
 * Returns the File object. You can check .exists() on the result.
 */
inline juce::File getFileInSettingsFolder(const juce::String& fileName) {
    return getSettingsFolder().getChildFile(fileName);
}
