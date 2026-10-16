#pragma once

#include <array>
#include <cstdint>

namespace SignalChain {
constexpr int rowCount = 2;
constexpr int columnsPerRow = 8;
constexpr int slotCount = rowCount * columnsPerRow;

// One chain that wraps onto a second row when the first row runs out of width.
// Both rows run left to right. The upper row ends on a down-link; the lower
// row starts with the continuation of that signal and ends on the output.
constexpr int inputSlot = 0;
constexpr int downSlot = 7;
constexpr int returnSlot = 8;
constexpr int outputSlot = 15;
constexpr int movableSlotCount = 12;

inline constexpr bool isMovableSlot(int chainSlot) noexcept {
    return (chainSlot >= 1 && chainSlot <= 6) || (chainSlot >= 9 && chainSlot <= 14);
}

inline constexpr int movableIndexForSlot(int chainSlot) noexcept {
    if (chainSlot >= 1 && chainSlot <= 6) {
        return chainSlot - 1;
    }
    if (chainSlot >= 9 && chainSlot <= 14) {
        return chainSlot - 3;
    }
    return -1;
}

inline constexpr int chainSlotForMovableIndex(int index) noexcept {
    if (index < 0 || index >= movableSlotCount) {
        return -1;
    }
    return index < 6 ? index + 1 : index + 3;
}

inline constexpr int visualRow(int chainSlot) noexcept {
    return chainSlot <= downSlot ? 0 : 1;
}

inline constexpr int visualColumn(int chainSlot) noexcept {
    if (chainSlot <= downSlot) {
        return chainSlot;
    }
    return chainSlot - returnSlot;
}

inline constexpr int chainSlotAt(int row, int column) noexcept {
    if (column < 0 || column >= columnsPerRow) {
        return -1;
    }
    if (row == 0) {
        return column;
    }
    if (row == 1) {
        return returnSlot + column;
    }
    return -1;
}

static_assert(chainSlotAt(0, 2) == 2);
static_assert(chainSlotAt(1, 0) == returnSlot);
static_assert(chainSlotAt(1, columnsPerRow - 1) == outputSlot);
static_assert(visualColumn(downSlot) == columnsPerRow - 1);
static_assert(visualColumn(returnSlot) == 0);

enum class Stage : int {
    Empty = 0,
    Amp = 1,
    Cab = 2,
    Eq = 3,
    Pedal = 4,
    PitchHarmonizer = 5,
    PitchOctaver = 6,
    ReverbPlate = 7,
    ReverbHall = 8,
    ReverbShimmer = 9,
    ReverbSpring = 10,
    ReverbGranular = 11,
    DelayTape = 12,
    DelayPingPong = 13,
    DelayDark = 14,
    DelayTapeExtreme = 15,
    DelayReverse = 16,
    ChorusEnsemble = 17,
    ChorusLead = 18,
    Phaser4 = 19,
    Phaser8 = 20,
    FlangerSubtle = 21,
    FlangerHard = 22,
    CompBlack = 23,
    CompBrutal = 24,
    CompClear = 25,
    EqParametric = 26,
    EqTone = 27,
    EqDynamic = 28,
    NoiseGate = 29,
    Tuner = 30,
    PitchDrop = 31
};

constexpr int effectStageCount = static_cast<int>(Stage::Tuner);

inline constexpr bool isKnownStage(Stage stage) noexcept {
    const auto value = static_cast<int>(stage);
    return value >= static_cast<int>(Stage::Empty) && value <= static_cast<int>(Stage::PitchDrop);
}

inline constexpr bool isEffectStage(Stage stage) noexcept {
    const auto value = static_cast<int>(stage);
    return value >= static_cast<int>(Stage::Amp) && value <= static_cast<int>(Stage::PitchDrop) &&
           value != static_cast<int>(Stage::DelayTapeExtreme);
}

struct Layout {
    std::array<Stage, movableSlotCount> slots{{Stage::Empty,
                                               Stage::Amp,
                                               Stage::Empty,
                                               Stage::Cab,
                                               Stage::Empty,
                                               Stage::Eq,
                                               Stage::Empty,
                                               Stage::Empty,
                                               Stage::Empty,
                                               Stage::Empty,
                                               Stage::Empty,
                                               Stage::Empty}};

    constexpr Stage atSlot(int chainSlot) const noexcept {
        const auto index = movableIndexForSlot(chainSlot);
        if (index < 0) {
            return Stage::Empty;
        }
        return slots[static_cast<size_t>(index)];
    }

    constexpr int slotFor(Stage stage) const noexcept {
        if (!isEffectStage(stage)) {
            return -1;
        }
        for (int index = 0; index < movableSlotCount; ++index) {
            if (slots[static_cast<size_t>(index)] == stage) {
                return chainSlotForMovableIndex(index);
            }
        }
        return -1;
    }

    int count(Stage stage) const noexcept {
        int found = 0;
        for (const auto candidate : slots) {
            if (candidate == stage) {
                ++found;
            }
        }
        return found;
    }

    bool contains(Stage stage) const noexcept {
        return slotFor(stage) >= 0;
    }

    bool occupies(int chainSlot) const noexcept {
        if (chainSlot == inputSlot || chainSlot == downSlot || chainSlot == returnSlot || chainSlot == outputSlot) {
            return true;
        }
        return atSlot(chainSlot) != Stage::Empty;
    }

    void place(Stage stage, int chainSlot) noexcept {
        const int target = movableIndexForSlot(chainSlot);
        if (!isEffectStage(stage) || target < 0) {
            return;
        }

        slots[static_cast<size_t>(target)] = stage;
    }

    void clear(int chainSlot) noexcept {
        const int target = movableIndexForSlot(chainSlot);
        if (target < 0) {
            return;
        }

        slots[static_cast<size_t>(target)] = Stage::Empty;
    }

    void moveSlot(int fromSlot, int toSlot) noexcept {
        const int from = movableIndexForSlot(fromSlot);
        const int to = movableIndexForSlot(toSlot);
        if (from < 0 || to < 0 || from == to) {
            return;
        }

        const auto displaced = slots[static_cast<size_t>(to)];
        slots[static_cast<size_t>(to)] = slots[static_cast<size_t>(from)];
        slots[static_cast<size_t>(from)] = displaced;
    }

    constexpr bool isValid() const noexcept {
        for (const auto stage : slots) {
            if (!isKnownStage(stage)) {
                return false;
            }
        }
        return true;
    }

    constexpr void retireRemovedStages() noexcept {
        for (auto& stage : slots) {
            if (stage == Stage::DelayTapeExtreme) {
                stage = Stage::Empty;
            }
        }
    }

    std::array<Stage, movableSlotCount> processingOrder() const noexcept {
        std::array<Stage, movableSlotCount> order{};
        int count = 0;
        for (const auto stage : slots) {
            if (stage != Stage::Empty) {
                order[static_cast<size_t>(count++)] = stage;
            }
        }
        return order;
    }

    int occupiedCount() const noexcept {
        int count = 0;
        for (const auto stage : slots) {
            if (stage != Stage::Empty) {
                ++count;
            }
        }
        return count;
    }

    // Five bits per slot (bits 0-59) plus a version nibble (bits 60-63).
    // Version 0 is the legacy 4-bit layout used by older sessions.
    constexpr std::uint64_t packed() const noexcept {
        constexpr int bitsPerSlot = 5;
        constexpr std::uint64_t version = 1;
        std::uint64_t value = version << 60;
        for (int index = 0; index < movableSlotCount; ++index) {
            value |= (static_cast<std::uint64_t>(slots[static_cast<size_t>(index)]) & 0x1F)
                     << (bitsPerSlot * index);
        }
        return value;
    }

    static constexpr Layout fromPacked(std::uint64_t packed) noexcept {
        constexpr int bitsPerSlot = 5;
        Layout layout;
        const auto version = packed >> 60;
        if (version == 0) {
            for (int index = 0; index < movableSlotCount; ++index) {
                const auto nibble = (packed >> (4 * index)) & 0x0F;
                if (nibble > static_cast<std::uint64_t>(Stage::Pedal)) {
                    return Layout{};
                }
                layout.slots[static_cast<size_t>(index)] = static_cast<Stage>(nibble);
            }
            return layout.isValid() ? layout : Layout{};
        }

        if (version != 1) {
            return Layout{};
        }

        for (int index = 0; index < movableSlotCount; ++index) {
            layout.slots[static_cast<size_t>(index)] =
                static_cast<Stage>((packed >> (bitsPerSlot * index)) & 0x1F);
        }
        layout.retireRemovedStages();
        return layout.isValid() ? layout : Layout{};
    }
};

inline constexpr std::uint64_t defaultPacked = Layout{}.packed();

static_assert(Layout::fromPacked(Layout{}.packed()).slotFor(Stage::Amp) == 2);
static_assert(Layout::fromPacked(Layout{}.packed()).slotFor(Stage::Cab) == 4);
static_assert(Layout::fromPacked(Layout{}.packed()).slotFor(Stage::Eq) == 6);
static_assert(Layout::fromPacked(0xFFFFFFFFu).slotFor(Stage::Amp) == 2);
}  // namespace SignalChain
