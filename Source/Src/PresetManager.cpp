#include "PresetManager.h"

#include <algorithm>
#include <utility>

#include "SettingsPath.h"

namespace {
const juce::Identifier formatId{"format"};
const juce::Identifier versionId{"version"};
const juce::Identifier idId{"id"};
const juce::Identifier nameId{"name"};
const juce::Identifier createdAtId{"createdAt"};
const juce::Identifier updatedAtId{"updatedAt"};
const juce::Identifier parametersId{"parameters"};
const juce::Identifier chainLayoutLowId{"chainLayoutLow"};
const juce::Identifier chainLayoutHighId{"chainLayoutHigh"};
const juce::Identifier chainBypassId{"chainBypass"};
const juce::Identifier routeHeaderId{"routeHeader"};
const juce::Identifier routePathBId{"routePathB"};
const juce::Identifier routeBypassId{"routeBypass"};
const juce::Identifier ampProfileIdId{"ampProfileId"};
const juce::Identifier irProfileIdId{"irProfileId"};
const juce::Identifier currentPresetIdId{"currentPresetId"};

const juce::String presetFormat{"spectrapreset"};
constexpr int presetFormatVersion = 1;
}  // namespace

PresetManager::PresetManager(juce::File directory)
    : _directory(std::move(directory)) {
    refresh();
    loadCurrentId();
}

void PresetManager::refresh() {
    _presets.clear();
    const auto folder = directory();
    if (!folder.isDirectory()) {
        return;
    }

    auto files = folder.findChildFiles(juce::File::findFiles, false, "*" + juce::String(fileExtension));
    for (const auto& file : files) {
        if (auto preset = readPreset(file, nullptr)) {
            _presets.push_back(*preset);
        }
    }

    std::sort(_presets.begin(), _presets.end(), [](const Preset& left, const Preset& right) {
        return left.name.compareIgnoreCase(right.name) < 0;
    });
}

int PresetManager::getCount() const noexcept {
    return static_cast<int>(_presets.size());
}

juce::StringArray PresetManager::getNames() const {
    juce::StringArray names;
    for (const auto& preset : _presets) {
        names.add(preset.name);
    }
    return names;
}

const Preset* PresetManager::getPreset(int index) const {
    if (index < 0 || index >= getCount()) {
        return nullptr;
    }
    return &_presets[static_cast<size_t>(index)];
}

int PresetManager::getCurrentIndex() const {
    if (_currentPresetId.isEmpty()) {
        return -1;
    }
    for (int index = 0; index < getCount(); ++index) {
        if (_presets[static_cast<size_t>(index)].id == _currentPresetId) {
            return index;
        }
    }
    return -1;
}

juce::String PresetManager::getCurrentId() const {
    return _currentPresetId;
}

bool PresetManager::savePreset(Preset& preset, juce::String* errorMessage) {
    const auto folder = directory();
    if (!folder.isDirectory() && !folder.createDirectory().wasOk()) {
        setError(errorMessage, "Could not create the preset folder.");
        return false;
    }

    if (preset.id.isEmpty()) {
        preset.id = juce::Uuid().toDashedString();
    }
    if (preset.name.trim().isEmpty()) {
        preset.name = "Preset " + juce::String(getCount() + 1);
    } else {
        preset.name = preset.name.trim();
    }

    const auto now = timestamp();
    if (preset.createdAt.isEmpty()) {
        preset.createdAt = now;
    }
    preset.updatedAt = now;
    preset.file = fileForId(preset.id);

    if (!writePreset(preset, errorMessage)) {
        return false;
    }

    refresh();
    sendChangeMessage();
    return true;
}

bool PresetManager::deletePreset(int index, juce::String* errorMessage) {
    const auto* preset = getPreset(index);
    if (preset == nullptr) {
        setError(errorMessage, "Preset not found.");
        return false;
    }

    const auto deletedId = preset->id;
    if (!preset->file.deleteFile()) {
        setError(errorMessage, "Could not delete the preset.");
        return false;
    }

    if (deletedId == _currentPresetId) {
        clearCurrentPreset();
    }

    refresh();
    sendChangeMessage();
    return true;
}

void PresetManager::setCurrentPresetId(const juce::String& presetId) {
    const auto trimmed = presetId.trim();
    if (trimmed.isEmpty()) {
        clearCurrentPreset();
        return;
    }

    const auto previous = _currentPresetId;
    _currentPresetId = trimmed;
    if (getCurrentIndex() < 0) {
        _currentPresetId = previous;
        return;
    }

    if (_currentPresetId != previous) {
        saveCurrentId();
        sendChangeMessage();
    }
}

void PresetManager::clearCurrentPreset() {
    if (_currentPresetId.isEmpty()) {
        return;
    }
    _currentPresetId.clear();
    saveCurrentId();
    sendChangeMessage();
}

juce::File PresetManager::directory() const {
    if (_directory != juce::File{}) {
        return _directory;
    }
    return getSettingsFolder().getChildFile("Presets");
}

juce::File PresetManager::settingsFile() const {
    return directory().getChildFile("current.json");
}

juce::File PresetManager::fileForId(const juce::String& presetId) const {
    return directory().getChildFile(presetId + fileExtension);
}

bool PresetManager::writePreset(const Preset& preset, juce::String* errorMessage) const {
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty(formatId, presetFormat);
    root->setProperty(versionId, presetFormatVersion);
    root->setProperty(idId, preset.id);
    root->setProperty(nameId, preset.name);
    root->setProperty(createdAtId, preset.createdAt);
    root->setProperty(updatedAtId, preset.updatedAt);
    root->setProperty(chainLayoutLowId, static_cast<int>(preset.chainLayout & 0xFFFFFFFFu));
    root->setProperty(chainLayoutHighId, static_cast<int>((preset.chainLayout >> 32) & 0xFFFFFFFFu));
    root->setProperty(chainBypassId, static_cast<int>(preset.chainBypass));
    root->setProperty(routeHeaderId, static_cast<int>(preset.routeHeader));
    root->setProperty(routePathBId, static_cast<int>(preset.routePathB));
    root->setProperty(routeBypassId, static_cast<int>(preset.routeBypass));
    root->setProperty(ampProfileIdId, preset.ampProfileId);
    root->setProperty(irProfileIdId, preset.irProfileId);

    auto parameters = std::make_unique<juce::DynamicObject>();
    for (int index = 0; index < preset.parameters.size(); ++index) {
        parameters->setProperty(preset.parameters.getName(index), preset.parameters.getValueAt(index));
    }
    root->setProperty(parametersId, juce::var(parameters.release()));

    if (preset.file.replaceWithText(juce::JSON::toString(juce::var(root.release()), false))) {
        return true;
    }

    setError(errorMessage, "Could not write the preset.");
    return false;
}

std::optional<Preset> PresetManager::readPreset(const juce::File& file, juce::String* errorMessage) const {
    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr || root->getProperty(formatId).toString() != presetFormat) {
        setError(errorMessage, "Unsupported preset file: " + file.getFileName());
        return std::nullopt;
    }

    const auto version = root->getProperty(versionId);
    if (!version.isVoid() && static_cast<int>(version) != presetFormatVersion) {
        setError(errorMessage, "Unsupported preset version: " + file.getFileName());
        return std::nullopt;
    }

    Preset preset;
    preset.file = file;
    preset.id = root->getProperty(idId).toString();
    if (preset.id.isEmpty()) {
        preset.id = file.getFileNameWithoutExtension();
    }
    preset.name = root->getProperty(nameId).toString();
    if (preset.name.isEmpty()) {
        preset.name = preset.id;
    }
    preset.createdAt = root->getProperty(createdAtId).toString();
    preset.updatedAt = root->getProperty(updatedAtId).toString();

    const auto low = static_cast<std::uint32_t>(static_cast<int>(root->getProperty(chainLayoutLowId)));
    const auto high = static_cast<std::uint32_t>(static_cast<int>(root->getProperty(chainLayoutHighId)));
    preset.chainLayout = (static_cast<std::uint64_t>(high) << 32) | static_cast<std::uint64_t>(low);
    preset.chainBypass = static_cast<std::uint16_t>(static_cast<int>(root->getProperty(chainBypassId)) & 0x0FFF);
    preset.routeHeader = static_cast<std::uint32_t>(static_cast<int>(root->getProperty(routeHeaderId)));
    preset.routePathB = static_cast<std::uint32_t>(static_cast<int>(root->getProperty(routePathBId)));
    preset.routeBypass = static_cast<std::uint8_t>(static_cast<int>(root->getProperty(routeBypassId)) & 0x3F);
    preset.ampProfileId = root->getProperty(ampProfileIdId).toString();
    preset.irProfileId = root->getProperty(irProfileIdId).toString();

    const auto parameters = root->getProperty(parametersId);
    if (auto* object = parameters.getDynamicObject()) {
        const auto& properties = object->getProperties();
        for (int index = 0; index < properties.size(); ++index) {
            preset.parameters.set(properties.getName(index), properties.getValueAt(index));
        }
    }

    return preset;
}

void PresetManager::loadCurrentId() {
    _currentPresetId.clear();
    if (!settingsFile().existsAsFile()) {
        return;
    }
    const auto parsed = juce::JSON::parse(settingsFile().loadFileAsString());
    if (auto* object = parsed.getDynamicObject()) {
        _currentPresetId = object->getProperty(currentPresetIdId).toString().trim();
    }
}

void PresetManager::saveCurrentId() const {
    const auto folder = directory();
    if (!folder.isDirectory() && !folder.createDirectory().wasOk()) {
        return;
    }
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty(currentPresetIdId, _currentPresetId);
    settingsFile().replaceWithText(juce::JSON::toString(juce::var(object.release()), false));
}

juce::String PresetManager::timestamp() {
    return juce::Time::getCurrentTime().toISO8601(true);
}

void PresetManager::setError(juce::String* errorMessage, const juce::String& message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}
