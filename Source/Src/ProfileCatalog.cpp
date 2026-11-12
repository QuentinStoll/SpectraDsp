#include "ProfileCatalog.h"

#include <algorithm>
#include <utility>

#include "SettingsPath.h"

namespace {
const juce::Identifier profilesId{"profiles"};
const juce::Identifier idId{"id"};
const juce::Identifier titleId{"title"};
const juce::Identifier kindId{"kind"};
const juce::Identifier conditionProfileId{"conditionProfile"};
const juce::Identifier conditionAmountId{"conditionAmount"};
const juce::Identifier localId{"local"};
const juce::Identifier authorId{"author"};
const juce::Identifier summaryId{"summary"};
const juce::Identifier badgeId{"badge"};

juce::String kindToString(ProfileCatalog::Kind kind) {
    return kind == ProfileCatalog::Kind::Ir ? "ir" : "amp";
}

ProfileCatalog::Kind kindFromString(const juce::String& kind) {
    return kind == "ir" ? ProfileCatalog::Kind::Ir : ProfileCatalog::Kind::Amp;
}
}  // namespace

ProfileCatalog::ProfileCatalog(juce::File directory)
    : _directory(std::move(directory)) {
    loadIndex();
}

ProfileCatalog::~ProfileCatalog() {
    wipeKeys();
}

void ProfileCatalog::syncAccount(const std::vector<Listing>& listings) {
    std::vector<Entry> next;
    for (const auto& existing : _entries) {
        if (existing.local) {
            next.push_back(existing);
        }
    }

    for (const auto& listing : listings) {
        if (listing.id.isEmpty()) {
            continue;
        }

        Entry entry;
        entry.id = listing.id;
        entry.title = listing.title.isNotEmpty() ? listing.title : listing.id;
        entry.kind = listing.kind;
        entry.author = listing.author;
        entry.summary = listing.summary;
        entry.badge = listing.badge;
        entry.conditionProfile = listing.conditionProfile;
        entry.conditionAmount = listing.conditionAmount;
        entry.local = false;
        entry.cipherOnDisk = cipherFile(entry.id, entry.kind).existsAsFile();

        bool replaced = false;
        for (auto& current : next) {
            if (current.id == entry.id && current.kind == entry.kind) {
                current = entry;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            next.push_back(entry);
        }
    }

    _entries = std::move(next);
    saveIndex();
    sendChangeMessage();
}

bool ProfileCatalog::storeCipher(const juce::String& id,
                                 const juce::String& title,
                                 Kind kind,
                                 const juce::MemoryBlock& cipher,
                                 int conditionProfile,
                                 float conditionAmount,
                                 bool local,
                                 juce::String* errorMessage) {
    if (id.isEmpty() || cipher.getSize() == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "This profile has no encrypted data.";
        }
        return false;
    }

    const auto folder = directory();
    if (!folder.isDirectory() && !folder.createDirectory().wasOk()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not create the profile folder.";
        }
        return false;
    }

    const auto file = cipherFile(id, kind);
    if (!file.replaceWithData(cipher.getData(), cipher.getSize())) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not store the encrypted profile.";
        }
        return false;
    }

    // A key must never sit beside the cipher. Drop any leftover from older builds.
    const auto leftoverKey = file.withFileExtension(".key");
    if (leftoverKey.existsAsFile()) {
        leftoverKey.deleteFile();
    }

    auto* entry = findMutable(id, kind);
    if (entry == nullptr) {
        _entries.push_back({});
        entry = &_entries.back();
    }

    entry->id = id;
    entry->title = title.isNotEmpty() ? title : id;
    entry->kind = kind;
    entry->conditionProfile = conditionProfile;
    entry->conditionAmount = conditionAmount;
    entry->cipherOnDisk = true;
    entry->local = local;
    if (entry->badge.isEmpty()) {
        entry->badge = kind == Kind::Ir ? "IR" : "AMP";
    }
    if (local && kind == Kind::Ir && entry->summary.isEmpty()) {
        entry->summary = "Captured in Spectra.";
    }
    saveIndex();
    sendChangeMessage();
    return true;
}

void ProfileCatalog::rememberKey(const juce::String& id, Kind kind, const juce::MemoryBlock& key) {
    if (key.getSize() != 32) {
        return;
    }

    const auto token = keyToken(id, kind);
    for (auto& stored : _keys) {
        if (stored.first == token) {
            stored.second.fillWith(0);
            stored.second = key;
            return;
        }
    }

    _keys.emplace_back(token, key);
}

void ProfileCatalog::remove(const juce::String& id, Kind kind) {
    cipherFile(id, kind).deleteFile();
    const auto token = keyToken(id, kind);
    for (auto it = _keys.begin(); it != _keys.end(); ++it) {
        if (it->first == token) {
            it->second.fillWith(0);
            _keys.erase(it);
            break;
        }
    }
    _entries.erase(std::remove_if(_entries.begin(), _entries.end(), [&](const Entry& entry) {
                       return entry.id == id && entry.kind == kind;
                   }),
                   _entries.end());
    saveIndex();
    sendChangeMessage();
}

bool ProfileCatalog::readCipher(const juce::String& id, Kind kind, juce::MemoryBlock& cipher) const {
    cipher.reset();
    const auto file = cipherFile(id, kind);
    return file.existsAsFile() && file.loadFileAsData(cipher) && cipher.getSize() > 0;
}

bool ProfileCatalog::copyKey(const juce::String& id, Kind kind, juce::MemoryBlock& key) const {
    key.reset();
    const auto token = keyToken(id, kind);
    for (const auto& stored : _keys) {
        if (stored.first == token && stored.second.getSize() == 32) {
            key = stored.second;
            return true;
        }
    }
    return false;
}

const ProfileCatalog::Entry* ProfileCatalog::find(const juce::String& id, Kind kind) const {
    for (const auto& entry : _entries) {
        if (entry.id == id && entry.kind == kind) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<ProfileCatalog::Entry> ProfileCatalog::entries(Kind kind) const {
    std::vector<Entry> matches;
    for (const auto& entry : _entries) {
        if (entry.kind == kind) {
            matches.push_back(entry);
        }
    }
    return matches;
}

juce::File ProfileCatalog::directory() const {
    if (_directory != juce::File{}) {
        return _directory;
    }
    return getSettingsFolder().getChildFile("Catalog");
}

juce::File ProfileCatalog::indexFile() const {
    return directory().getChildFile("catalog.json");
}

juce::File ProfileCatalog::cipherFile(const juce::String& id, Kind kind) const {
    const auto prefix = kind == Kind::Ir ? "ir-" : "amp-";
    return directory().getChildFile(prefix + safeId(id) + ".spectra");
}

juce::String ProfileCatalog::safeId(const juce::String& id) {
    auto safe = id.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    return safe.isNotEmpty() ? safe : "profile";
}

juce::String ProfileCatalog::keyToken(const juce::String& id, Kind kind) {
    return kindToString(kind) + ":" + id;
}

void ProfileCatalog::loadIndex() {
    _entries.clear();
    const auto file = indexFile();
    if (!file.existsAsFile()) {
        return;
    }

    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr) {
        return;
    }

    const auto profiles = root->getProperty(profilesId);
    if (auto* list = profiles.getArray()) {
        for (const auto& item : *list) {
            auto* object = item.getDynamicObject();
            if (object == nullptr) {
                continue;
            }
            Entry entry;
            entry.id = object->getProperty(idId).toString();
            entry.title = object->getProperty(titleId).toString();
            entry.kind = kindFromString(object->getProperty(kindId).toString());
            entry.conditionProfile = static_cast<int>(object->getProperty(conditionProfileId));
            entry.conditionAmount = static_cast<float>(static_cast<double>(object->getProperty(conditionAmountId)));
            entry.local = static_cast<bool>(object->getProperty(localId));
            entry.author = object->getProperty(authorId).toString();
            entry.summary = object->getProperty(summaryId).toString();
            entry.badge = object->getProperty(badgeId).toString();
            if (entry.id.isEmpty()) {
                continue;
            }
            if (entry.title.isEmpty()) {
                entry.title = entry.id;
            }
            entry.cipherOnDisk = cipherFile(entry.id, entry.kind).existsAsFile();
            _entries.push_back(entry);
        }
    }
}

void ProfileCatalog::saveIndex() const {
    const auto folder = directory();
    if (!folder.isDirectory() && !folder.createDirectory().wasOk()) {
        return;
    }

    auto list = juce::Array<juce::var>();
    for (const auto& entry : _entries) {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty(idId, entry.id);
        object->setProperty(titleId, entry.title);
        object->setProperty(kindId, kindToString(entry.kind));
        object->setProperty(conditionProfileId, entry.conditionProfile);
        object->setProperty(conditionAmountId, entry.conditionAmount);
        object->setProperty(localId, entry.local);
        object->setProperty(authorId, entry.author);
        object->setProperty(summaryId, entry.summary);
        object->setProperty(badgeId, entry.badge);
        list.add(juce::var(object.release()));
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty(profilesId, list);
    indexFile().replaceWithText(juce::JSON::toString(juce::var(root.release()), false));
}

ProfileCatalog::Entry* ProfileCatalog::findMutable(const juce::String& id, Kind kind) {
    for (auto& entry : _entries) {
        if (entry.id == id && entry.kind == kind) {
            return &entry;
        }
    }
    return nullptr;
}

void ProfileCatalog::wipeKeys() {
    for (auto& stored : _keys) {
        stored.second.fillWith(0);
    }
    _keys.clear();
}
