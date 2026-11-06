#include "AppUpdate.h"

#ifndef SPECTRA_RELEASE_TAG
#define SPECTRA_RELEASE_TAG "0.0.0-dev"
#endif

namespace AppUpdate {
namespace {
juce::String jsonString(const juce::var& object, const juce::Identifier& key) {
    if (auto* dynamicObject = object.getDynamicObject()) {
        if (dynamicObject->hasProperty(key)) {
            return dynamicObject->getProperty(key).toString();
        }
    }
    return {};
}

juce::String httpGet(const juce::String& url, juce::String extraHeaders, int* statusCode) {
    juce::URL request(url);
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(20000)
                             .withExtraHeaders("User-Agent: Spectra\r\n" + extraHeaders)
                             .withHttpRequestCmd("GET")
                             .withStatusCode(statusCode);
    if (auto stream = request.createInputStream(options)) {
        return stream->readEntireStreamAsString();
    }
    return {};
}
}  // namespace

juce::String currentTag() {
    return SPECTRA_RELEASE_TAG;
}

juce::String expectedInstallerName() {
#if JUCE_WINDOWS
#if defined(_M_ARM64) || defined(__aarch64__)
    return "Spectra-windows-arm64.exe";
#else
    return "Spectra-windows-x64.exe";
#endif
#elif JUCE_MAC
    return "Spectra-macos-arm64.pkg";
#else
    return "Spectra-linux-x64.run";
#endif
}

int runNumberFromTag(const juce::String& tag) {
    const auto trimmed = tag.trim();
    const auto dash = trimmed.lastIndexOfChar('-');
    const auto numberText = dash >= 0 ? trimmed.substring(dash + 1) : trimmed;
    if (!numberText.containsOnly("0123456789") || numberText.isEmpty()) {
        return 0;
    }
    return numberText.getIntValue();
}

bool isNewer(const juce::String& latestTag, const juce::String& currentTag) {
    const auto latest = runNumberFromTag(latestTag);
    const auto current = runNumberFromTag(currentTag);
    if (latest <= 0) {
        return false;
    }
    return latest > current;
}

Offer parseLatestJson(const juce::String& json,
                      const juce::String& installerName,
                      const juce::String& installedTag) {
    Offer offer;
    offer.currentTag = installedTag;
    offer.installerName = installerName;

    const auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) {
        offer.message = "Could not read the GitHub release.";
        return offer;
    }

    offer.latestTag = jsonString(parsed, "tag_name");
    if (auto* assets = parsed.getProperty("assets", {}).getArray()) {
        for (const auto& asset : *assets) {
            if (jsonString(asset, "name") == installerName) {
                offer.installerUrl = jsonString(asset, "browser_download_url");
                break;
            }
        }
    }

    if (offer.latestTag.isEmpty()) {
        offer.message = "The latest GitHub release has no version tag.";
        return offer;
    }
    if (offer.installerUrl.isEmpty()) {
        offer.message = "No installer for this system in " + offer.latestTag + ".";
        return offer;
    }
    if (!isNewer(offer.latestTag, installedTag)) {
        offer.message = "Spectra is up to date (" + installedTag + ").";
        return offer;
    }

    offer.available = true;
    offer.message = "Spectra " + offer.latestTag + " is available.";
    return offer;
}

Offer checkLatest() {
    int status = 0;
    const auto body = httpGet(githubLatestUrl, "Accept: application/vnd.github+json\r\n", &status);
    if (status != 200 || body.isEmpty()) {
        Offer offer;
        offer.currentTag = currentTag();
        offer.installerName = expectedInstallerName();
        offer.message = "Could not reach GitHub for updates.";
        return offer;
    }
    return parseLatestJson(body, expectedInstallerName(), currentTag());
}

bool downloadInstaller(const juce::String& url, const juce::File& destination, juce::String* error) {
    int status = 0;
    juce::URL request(url);
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(120000)
                             .withExtraHeaders("User-Agent: Spectra\r\nAccept: application/octet-stream\r\n")
                             .withHttpRequestCmd("GET")
                             .withStatusCode(&status);
    auto stream = request.createInputStream(options);
    if (stream == nullptr || (status != 0 && status != 200)) {
        if (error != nullptr) {
            *error = "Could not download the installer.";
        }
        return false;
    }

    destination.deleteFile();
    destination.getParentDirectory().createDirectory();
    auto out = destination.createOutputStream();
    if (out == nullptr) {
        if (error != nullptr) {
            *error = "Could not write the installer to disk.";
        }
        return false;
    }
    const auto written = out->writeFromInputStream(*stream, -1);
    out.reset();
    if (written <= 0 || !destination.existsAsFile()) {
        if (error != nullptr) {
            *error = "The downloaded installer was empty.";
        }
        return false;
    }
    return true;
}

bool launchInstaller(const juce::File& installer, juce::String* error) {
    if (!installer.existsAsFile()) {
        if (error != nullptr) {
            *error = "The installer file is missing.";
        }
        return false;
    }

#if JUCE_LINUX
    installer.setExecutePermission(true);
#endif

    if (!installer.startAsProcess()) {
        if (error != nullptr) {
            *error = "Could not open the installer.";
        }
        return false;
    }
    return true;
}
}  // namespace AppUpdate
