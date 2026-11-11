#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <optional>
#include <vector>

// A preset is a local signal-chain setup: block order, block choice, and values.
// Amp and IR content is referenced by catalog id, not embedded.
struct Preset {
    juce::String id;
    juce::String name;
    juce::File file;
    juce::String createdAt;
    juce::String updatedAt;
    juce::NamedValueSet parameters;
    std::uint64_t chainLayout = 0;
    std::uint16_t chainBypass = 0;
    std::uint32_t routeHeader = 0;
    std::uint32_t routePathB = 0;
    std::uint8_t routeBypass = 0;
    juce::String ampProfileId;
    juce::String irProfileId;
};

class PresetManager : public juce::ChangeBroadcaster {
   public:
    static constexpr const char* fileExtension = ".spectrapreset";

    explicit PresetManager(juce::File directory = {});

    void refresh();
    int getCount() const noexcept;
    juce::StringArray getNames() const;
    const Preset* getPreset(int index) const;
    int getCurrentIndex() const;
    juce::String getCurrentId() const;

    bool savePreset(Preset& preset, juce::String* errorMessage = nullptr);
    bool deletePreset(int index, juce::String* errorMessage = nullptr);
    void setCurrentPresetId(const juce::String& presetId);
    void clearCurrentPreset();

   private:
    juce::File _directory;
    juce::String _currentPresetId;
    std::vector<Preset> _presets;

    juce::File directory() const;
    juce::File settingsFile() const;
    juce::File fileForId(const juce::String& presetId) const;
    bool writePreset(const Preset& preset, juce::String* errorMessage) const;
    std::optional<Preset> readPreset(const juce::File& file, juce::String* errorMessage) const;
    void loadCurrentId();
    void saveCurrentId() const;
    static juce::String timestamp();
    static void setError(juce::String* errorMessage, const juce::String& message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
