#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

class SpectraEngine {
   public:
    SpectraEngine();
    ~SpectraEngine();

    bool open(const juce::File& libraryFile = {});
    void close();
    bool isLoaded() const noexcept;
    int abiVersion() const noexcept;

    bool unlock(const juce::String& accessToken);
    void lock();

    bool decryptModel(const void* cipher, size_t size, juce::MemoryBlock& plain) const;
    bool decryptIr(const void* cipher, size_t size, juce::MemoryBlock& plain) const;

    bool prepareIrSweep(double sampleRate, std::vector<float>& sweep, std::size_t& recordFrames);
    bool sealIr(const float* recorded,
                std::size_t frames,
                double sampleRate,
                std::array<std::uint8_t, 32>& key,
                juce::MemoryBlock& sealed);
    bool openIr(const uint8_t* key,
                size_t keySize,
                const void* sealed,
                size_t sealedSize,
                juce::MemoryBlock& plain);
    bool canOpenModel() const noexcept;
    bool openModel(const uint8_t* key,
                   size_t keySize,
                   const void* sealed,
                   size_t sealedSize,
                   juce::MemoryBlock& plain);

   private:
    struct Functions;

    std::unique_ptr<Functions> _functions;
    juce::DynamicLibrary _library;
    int _abiVersion = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectraEngine)
};
