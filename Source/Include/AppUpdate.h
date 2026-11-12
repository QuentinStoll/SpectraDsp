#pragma once

#include <JuceHeader.h>

#include <functional>

namespace AppUpdate {
inline constexpr const char* githubLatestUrl =
    "https://api.github.com/repos/QuentinStoll/SpectraDsp/releases/latest";

struct Offer {
    bool available = false;
    juce::String currentTag;
    juce::String latestTag;
    juce::String installerUrl;
    juce::String installerName;
    juce::String message;
};

juce::String currentTag();
juce::String installerFileName(const juce::String& os, const juce::String& arch);
juce::String expectedInstallerName();
int runNumberFromTag(const juce::String& tag);
bool isNewer(const juce::String& latestTag, const juce::String& currentTag);
bool isInstalledRelease();
Offer parseLatestJson(const juce::String& json,
                      const juce::String& installerName,
                      const juce::String& installedTag);
Offer checkLatest();
juce::String skippedTag();
void rememberSkippedTag(const juce::String& tag);
bool downloadInstaller(const juce::String& url,
                       const juce::File& destination,
                       juce::String* error,
                       const std::function<void(float)>& onProgress = {});
bool launchInstaller(const juce::File& installer, juce::String* error);
}  // namespace AppUpdate
