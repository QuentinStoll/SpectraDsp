#include "AppUpdate.h"

#include "SettingsPath.h"

#ifndef SPECTRA_RELEASE_TAG
#define SPECTRA_RELEASE_TAG "0.0.0-dev"
#endif

namespace AppUpdate {
namespace {
struct ReleaseId {
    bool valid = false;
    bool calendar = false;
    int part[4] = {};
};

ReleaseId parseReleaseId(const juce::String& tag) {
    ReleaseId id;
    auto body = tag.trim();
    if (body.startsWithIgnoreCase("v")) {
        body = body.substring(1);
    }

    int count = 0;
    juce::String digits;
    const auto push = [&]() {
        if (digits.isEmpty() || count >= 4) {
            digits.clear();
            return;
        }
        id.part[count++] = digits.getIntValue();
        digits.clear();
    };

    for (int index = 0; index < body.length(); ++index) {
        const auto character = body[index];
        if (juce::CharacterFunctions::isDigit(character)) {
            digits += character;
        } else {
            push();
        }
    }
    push();

    id.valid = count > 0 && id.part[0] > 0;
    id.calendar = id.part[0] >= 2000;
    return id;
}

juce::String jsonString(const juce::var& object, const juce::Identifier& key) {
    if (auto* dynamicObject = object.getDynamicObject()) {
        if (dynamicObject->hasProperty(key)) {
            return dynamicObject->getProperty(key).toString();
        }
    }
    return {};
}

juce::String displayTag(const juce::String& tag) {
    return tag.startsWithIgnoreCase("v") ? tag.substring(1) : tag;
}

juce::File skippedTagFile() {
    return getSettingsFolder().getChildFile("skipped_update.txt");
}

juce::String httpGet(const juce::String& url, const juce::String& extraHeaders, int* statusCode) {
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

juce::String thisOs() {
#if JUCE_WINDOWS
    return "windows";
#elif JUCE_MAC
    return "macos";
#else
    return "linux";
#endif
}

juce::String thisArch() {
#if JUCE_ARM || defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "x64";
#endif
}
}  // namespace

juce::String currentTag() {
    return SPECTRA_RELEASE_TAG;
}

juce::String installerFileName(const juce::String& os, const juce::String& arch) {
    const auto extension = os == "windows" ? "exe" : (os == "macos" ? "pkg" : "run");
    return "Spectra-" + os + "-" + arch + "." + extension;
}

juce::String expectedInstallerName() {
    return installerFileName(thisOs(), thisArch());
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
    const auto latest = parseReleaseId(latestTag);
    const auto current = parseReleaseId(currentTag);
    if (!latest.valid) {
        return false;
    }
    if (!current.valid || latest.calendar != current.calendar) {
        return current.valid ? latest.calendar != current.calendar : true;
    }

    for (int index = 0; index < 4; ++index) {
        if (latest.part[index] != current.part[index]) {
            return latest.part[index] > current.part[index];
        }
    }
    return false;
}

bool isInstalledRelease() {
    return parseReleaseId(currentTag()).valid;
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
        offer.message = "No installer for this computer in " + displayTag(offer.latestTag) + ".";
        return offer;
    }
    if (!isNewer(offer.latestTag, installedTag)) {
        offer.message = "Spectra is up to date.";
        return offer;
    }

    offer.available = true;
    offer.message = "Spectra " + displayTag(offer.latestTag) + " is ready.";
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

juce::String skippedTag() {
    const auto file = skippedTagFile();
    if (!file.existsAsFile()) {
        return {};
    }
    return file.loadFileAsString().trim();
}

void rememberSkippedTag(const juce::String& tag) {
    auto file = skippedTagFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(tag.trim());
}

bool downloadInstaller(const juce::String& url,
                       const juce::File& destination,
                       juce::String* error,
                       const std::function<void(float)>& onProgress) {
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

    const auto total = stream->getTotalLength();
    juce::int64 written = 0;
    char buffer[64 * 1024];
    for (;;) {
        const auto read = stream->read(buffer, static_cast<int>(sizeof(buffer)));
        if (read <= 0) {
            break;
        }
        if (!out->write(buffer, static_cast<size_t>(read))) {
            if (error != nullptr) {
                *error = "Could not write the installer to disk.";
            }
            return false;
        }
        written += read;
        if (onProgress != nullptr && total > 0) {
            onProgress(static_cast<float>(written) / static_cast<float>(total));
        }
    }
    out.reset();
    if (written <= 0 || !destination.existsAsFile()) {
        if (error != nullptr) {
            *error = "The downloaded installer was empty.";
        }
        return false;
    }
    if (onProgress != nullptr) {
        onProgress(1.0f);
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

#if JUCE_MAC
    juce::ChildProcess quarantine;
    quarantine.start("xattr -d com.apple.quarantine \"" + installer.getFullPathName() + "\"");
    quarantine.waitForProcessToFinish(2000);
#elif JUCE_LINUX
    installer.setExecutePermission(true);
#endif

    if (!installer.startAsProcess()) {
        installer.revealToUser();
        if (error != nullptr) {
            *error = "Could not open the installer.";
        }
        return false;
    }
    return true;
}
}  // namespace AppUpdate
