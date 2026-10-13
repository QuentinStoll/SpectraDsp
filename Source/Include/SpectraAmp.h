#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

// Clear-model amp engine. The on-disk JSON stays intact; this type only owns the
// realtime instance and the sample-rate adapter around it.
class SpectraAmp {
   public:
    enum class LoadStatus {
        NotNeural,
        Loaded,
        Invalid
    };

    SpectraAmp();
    ~SpectraAmp();

    SpectraAmp(const SpectraAmp&) = delete;
    SpectraAmp& operator=(const SpectraAmp&) = delete;

    LoadStatus load(const void* data, std::size_t size);
    void clear();
    void prepare(double sampleRate, int maxBlockSize);
    void setInstanceCount(int count);

    // Audio thread. Writes numSamples wet frames. False leaves the caller on the dry signal.
    bool render(int instance, const float* input, float* wet, int numSamples) noexcept;

    int latencySamples() const noexcept;
    bool isLoaded() const noexcept;

   private:
    struct Engine;
    struct Instance;

    std::unique_ptr<Engine> build(int instanceCount) const;
    void configure(Engine& engine) const;
    bool publish(std::unique_ptr<Engine> engine);
    void retire();
    bool renderEngine(Engine& engine, int instance, const float* input, float* wet, int numSamples) noexcept;

    std::string _source;
    double _hostRate = 0.0;
    int _maxBlock = 0;

    std::unique_ptr<Engine> _owned[2]{};
    std::atomic<std::uint32_t> _readers[2]{};
    std::atomic<int> _active{-1};
    std::atomic<int> _latency{0};
};
