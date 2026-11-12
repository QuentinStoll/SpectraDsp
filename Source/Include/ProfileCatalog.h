#pragma once

#include <JuceHeader.h>

#include <vector>

// Account amp and IR profiles. The audio stays encrypted on disk.
// Keys, when the server issues one, live only in this process.
class ProfileCatalog : public juce::ChangeBroadcaster {
   public:
    enum class Kind {
        Amp,
        Ir
    };

    struct Entry {
        juce::String id;
        juce::String title;
        juce::String author;
        juce::String summary;
        juce::String badge;
        Kind kind = Kind::Amp;
        int conditionProfile = 0;
        float conditionAmount = 0.0f;
        bool cipherOnDisk = false;
        bool local = false;
    };

    struct Listing {
        juce::String id;
        juce::String title;
        juce::String author;
        juce::String summary;
        juce::String badge;
        Kind kind = Kind::Amp;
        int conditionProfile = 0;
        float conditionAmount = 0.0f;
    };

    explicit ProfileCatalog(juce::File directory = {});

    ~ProfileCatalog() override;

    // Replaces the account list. Local captures stay. Cipher files are not deleted.
    void syncAccount(const std::vector<Listing>& listings);

    // Writes the encrypted blob only. Never writes a key, wav, or plain model.
    bool storeCipher(const juce::String& id,
                     const juce::String& title,
                     Kind kind,
                     const juce::MemoryBlock& cipher,
                     int conditionProfile,
                     float conditionAmount,
                     bool local,
                     juce::String* errorMessage = nullptr);

    // Keeps the opening key in memory for this session. It is not written.
    void rememberKey(const juce::String& id, Kind kind, const juce::MemoryBlock& key);

    bool readCipher(const juce::String& id, Kind kind, juce::MemoryBlock& cipher) const;
    void remove(const juce::String& id, Kind kind);
    bool copyKey(const juce::String& id, Kind kind, juce::MemoryBlock& key) const;
    const Entry* find(const juce::String& id, Kind kind) const;

    std::vector<Entry> entries(Kind kind) const;

   private:
    juce::File _directory;
    std::vector<Entry> _entries;
    std::vector<std::pair<juce::String, juce::MemoryBlock>> _keys;

    juce::File directory() const;
    juce::File indexFile() const;
    juce::File cipherFile(const juce::String& id, Kind kind) const;
    static juce::String safeId(const juce::String& id);
    static juce::String keyToken(const juce::String& id, Kind kind);
    void loadIndex();
    void saveIndex() const;
    Entry* findMutable(const juce::String& id, Kind kind);
    void wipeKeys();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfileCatalog)
};
