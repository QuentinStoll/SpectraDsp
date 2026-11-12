#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

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

    void clearAll() noexcept {
        slots.fill(Stage::Empty);
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

// One parallel fork beside the serial chain. splitAfter/mergeAfter are movable
// indices: blocks (splitAfter, mergeAfter] stay on path A, path B runs beside them.
struct ParallelRoute {
    static constexpr int pathBCount = 6;
    static constexpr int pathBBase = 16;

    int splitAfter = -1;
    int mergeAfter = -1;
    std::array<Stage, pathBCount> pathB{};
    float mix = 0.5f;

    constexpr bool active() const noexcept {
        return mergeAfter >= 0 && mergeAfter < movableSlotCount && splitAfter < mergeAfter && splitAfter >= -1;
    }

    constexpr Stage pathBAt(int index) const noexcept {
        if (index < 0 || index >= pathBCount) {
            return Stage::Empty;
        }
        return pathB[static_cast<size_t>(index)];
    }

    int occupiedPathB() const noexcept {
        int count = 0;
        for (const auto stage : pathB) {
            if (stage != Stage::Empty) {
                ++count;
            }
        }
        return count;
    }

    void placePathB(Stage stage, int index) noexcept {
        if (index < 0 || index >= pathBCount || !isEffectStage(stage)) {
            return;
        }
        pathB[static_cast<size_t>(index)] = stage;
    }

    void clearPathB(int index) noexcept {
        if (index < 0 || index >= pathBCount) {
            return;
        }
        pathB[static_cast<size_t>(index)] = Stage::Empty;
    }

    void clearPathB() noexcept { pathB.fill(Stage::Empty); }

    constexpr std::uint32_t packedHeader() const noexcept {
        std::uint32_t value = 0;
        if (splitAfter >= 0) {
            value |= static_cast<std::uint32_t>(splitAfter + 1) & 0x0Fu;
        }
        if (mergeAfter >= 0) {
            value |= (static_cast<std::uint32_t>(mergeAfter + 1) & 0x0Fu) << 4;
        }
        return value;
    }

    constexpr std::uint32_t packedPathB() const noexcept {
        std::uint32_t value = 0;
        for (int index = 0; index < pathBCount; ++index) {
            value |= (static_cast<std::uint32_t>(pathB[static_cast<size_t>(index)]) & 0x1Fu)
                     << (5 * index);
        }
        return value;
    }

    static constexpr ParallelRoute fromPacked(std::uint32_t header, std::uint32_t pathBPacked) noexcept {
        ParallelRoute route;
        const auto splitCode = header & 0x0Fu;
        const auto mergeCode = (header >> 4) & 0x0Fu;
        route.splitAfter = splitCode == 0 ? -1 : static_cast<int>(splitCode) - 1;
        route.mergeAfter = mergeCode == 0 ? -1 : static_cast<int>(mergeCode) - 1;
        for (int index = 0; index < pathBCount; ++index) {
            route.pathB[static_cast<size_t>(index)] =
                static_cast<Stage>((pathBPacked >> (5 * index)) & 0x1Fu);
        }
        if (!route.active()) {
            route.splitAfter = -1;
            route.mergeAfter = -1;
        }
        for (auto& stage : route.pathB) {
            if (!isKnownStage(stage) || stage == Stage::DelayTapeExtreme) {
                stage = Stage::Empty;
            }
        }
        return route;
    }
};

inline constexpr bool isPathBSlot(int chainSlot) noexcept {
    return chainSlot >= ParallelRoute::pathBBase &&
           chainSlot < ParallelRoute::pathBBase + ParallelRoute::pathBCount;
}

inline constexpr int pathBIndexForSlot(int chainSlot) noexcept {
    return isPathBSlot(chainSlot) ? chainSlot - ParallelRoute::pathBBase : -1;
}

inline constexpr int pathBSlotForIndex(int index) noexcept {
    if (index < 0 || index >= ParallelRoute::pathBCount) {
        return -1;
    }
    return ParallelRoute::pathBBase + index;
}

// One visual row of the chain. Columns are the movable slots on that row.
constexpr int forkRowCount = 2;
constexpr int forkColumns = 6;

inline constexpr int forkRowForMovable(int movableIndex) noexcept {
    return movableIndex >= forkColumns ? 1 : 0;
}

inline constexpr int forkColumnForMovable(int movableIndex) noexcept {
    return movableIndex >= forkColumns ? movableIndex - forkColumns : movableIndex;
}

inline constexpr int movableForFork(int row, int column) noexcept {
    return row * forkColumns + column;
}

// splitGap and mergeGap are gaps between columns. The fork covers columns
// [splitGap, mergeGap). Lane B has exactly that many slots, one per column.
struct Bifurcation {
    int id = 0;
    int row = 0;
    int splitGap = 0;
    int mergeGap = 1;
    float mix = 0.5f;
    std::array<Stage, forkColumns> laneB{};

    constexpr int columnCount() const noexcept { return mergeGap - splitGap; }

    constexpr bool covers(int column) const noexcept {
        return column >= splitGap && column < mergeGap;
    }

    constexpr Stage laneAt(int index) const noexcept {
        if (index < 0 || index >= columnCount()) {
            return Stage::Empty;
        }
        return laneB[static_cast<size_t>(index)];
    }
};

struct ForkGraph {
    std::vector<Bifurcation> forks;
};

struct BlockRef {
    bool laneB = false;
    int row = 0;
    int column = 0;
    int forkId = -1;
};

enum class DropKind {
    Forbidden,
    Slot,
    NewFork
};

struct DropPreview {
    DropKind kind = DropKind::Forbidden;
    bool swap = false;
    BlockRef target{};
    int splitGap = 0;
    int mergeGap = 0;
};

inline void sortForks(ForkGraph& graph) {
    std::sort(graph.forks.begin(), graph.forks.end(), [](const Bifurcation& left, const Bifurcation& right) {
        if (left.row != right.row) {
            return left.row < right.row;
        }
        return left.splitGap < right.splitGap;
    });
}

inline int clampInt(int value, int minimum, int maximum) noexcept {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

inline bool forkIntervalFits(const ForkGraph& graph, int row, int splitGap, int mergeGap, int ignoreId = -1) {
    if (row < 0 || row >= forkRowCount || splitGap < 0 || mergeGap > forkColumns || mergeGap <= splitGap) {
        return false;
    }
    for (const auto& fork : graph.forks) {
        if (fork.row != row || fork.id == ignoreId) {
            continue;
        }
        const bool leftOf = mergeGap <= fork.splitGap - 1;
        const bool rightOf = splitGap >= fork.mergeGap + 1;
        if (!leftOf && !rightOf) {
            return false;
        }
    }
    return true;
}

inline int clampSplit(const ForkGraph& graph, int forkId, int proposed) {
    const Bifurcation* fork = nullptr;
    const Bifurcation* previous = nullptr;
    for (const auto& candidate : graph.forks) {
        if (candidate.id == forkId) {
            fork = &candidate;
        }
    }
    if (fork == nullptr) {
        return proposed;
    }
    for (const auto& candidate : graph.forks) {
        if (candidate.row == fork->row && candidate.id != forkId && candidate.mergeGap <= fork->splitGap) {
            if (previous == nullptr || candidate.mergeGap > previous->mergeGap) {
                previous = &candidate;
            }
        }
    }
    auto limit = previous != nullptr ? previous->mergeGap + 1 : 0;
    auto maxSplit = fork->mergeGap - 1;
    for (int index = 0; index < fork->columnCount(); ++index) {
        if (fork->laneAt(index) != Stage::Empty) {
            maxSplit = fork->splitGap + index;
            break;
        }
    }
    return clampInt(proposed, limit, maxSplit);
}

inline int clampMerge(const ForkGraph& graph, int forkId, int proposed) {
    const Bifurcation* fork = nullptr;
    const Bifurcation* next = nullptr;
    for (const auto& candidate : graph.forks) {
        if (candidate.id == forkId) {
            fork = &candidate;
        }
    }
    if (fork == nullptr) {
        return proposed;
    }
    for (const auto& candidate : graph.forks) {
        if (candidate.row == fork->row && candidate.id != forkId && candidate.splitGap >= fork->mergeGap) {
            if (next == nullptr || candidate.splitGap < next->splitGap) {
                next = &candidate;
            }
        }
    }
    auto limit = next != nullptr ? next->splitGap - 1 : forkColumns;
    auto minMerge = fork->splitGap + 1;
    for (int index = 0; index < fork->columnCount(); ++index) {
        if (fork->laneAt(index) != Stage::Empty) {
            minMerge = fork->splitGap + index + 1;
        }
    }
    return clampInt(proposed, minMerge, limit);
}

inline bool createBifurcation(ForkGraph& graph, int row, int splitGap, int mergeGap, int* createdId = nullptr) {
    if (!forkIntervalFits(graph, row, splitGap, mergeGap)) {
        return false;
    }
    Bifurcation fork;
    fork.row = row;
    fork.splitGap = splitGap;
    fork.mergeGap = mergeGap;
    fork.mix = 0.5f;
    int nextId = 1;
    for (const auto& existing : graph.forks) {
        nextId = nextId > existing.id + 1 ? nextId : existing.id + 1;
    }
    fork.id = nextId;
    graph.forks.push_back(fork);
    sortForks(graph);
    if (createdId != nullptr) {
        *createdId = fork.id;
    }
    return true;
}

inline bool moveSplit(ForkGraph& graph, int forkId, int proposedGap) {
    for (auto& fork : graph.forks) {
        if (fork.id != forkId) {
            continue;
        }
        const auto gap = clampSplit(graph, forkId, proposedGap);
        if (!forkIntervalFits(graph, fork.row, gap, fork.mergeGap, forkId)) {
            return false;
        }
        const auto shift = gap - fork.splitGap;
        if (shift > 0) {
            for (int index = 0; index < forkColumns - shift; ++index) {
                fork.laneB[static_cast<size_t>(index)] = fork.laneB[static_cast<size_t>(index + shift)];
            }
            for (int index = forkColumns - shift; index < forkColumns; ++index) {
                fork.laneB[static_cast<size_t>(index)] = Stage::Empty;
            }
        } else if (shift < 0) {
            for (int index = forkColumns - 1; index >= -shift; --index) {
                fork.laneB[static_cast<size_t>(index)] = fork.laneB[static_cast<size_t>(index + shift)];
            }
            for (int index = 0; index < -shift; ++index) {
                fork.laneB[static_cast<size_t>(index)] = Stage::Empty;
            }
        }
        fork.splitGap = gap;
        return true;
    }
    return false;
}

inline bool moveMerge(ForkGraph& graph, int forkId, int proposedGap) {
    for (auto& fork : graph.forks) {
        if (fork.id != forkId) {
            continue;
        }
        const auto gap = clampMerge(graph, forkId, proposedGap);
        if (!forkIntervalFits(graph, fork.row, fork.splitGap, gap, forkId)) {
            return false;
        }
        fork.mergeGap = gap;
        return true;
    }
    return false;
}

inline Bifurcation* findForkAt(ForkGraph& graph, int row, int column) {
    for (auto& fork : graph.forks) {
        if (fork.row == row && fork.covers(column)) {
            return &fork;
        }
    }
    return nullptr;
}

inline const Bifurcation* findForkAt(const ForkGraph& graph, int row, int column) {
    for (const auto& fork : graph.forks) {
        if (fork.row == row && fork.covers(column)) {
            return &fork;
        }
    }
    return nullptr;
}

inline Stage stageAt(const Layout& layout, const ForkGraph& graph, BlockRef ref) {
    if (!ref.laneB) {
        return layout.atSlot(chainSlotForMovableIndex(movableForFork(ref.row, ref.column)));
    }
    if (const auto* fork = findForkAt(graph, ref.row, ref.column)) {
        return fork->laneAt(ref.column - fork->splitGap);
    }
    return Stage::Empty;
}

inline void writeStage(Layout& layout, ForkGraph& graph, BlockRef ref, Stage stage) {
    if (!ref.laneB) {
        const auto slot = chainSlotForMovableIndex(movableForFork(ref.row, ref.column));
        if (stage == Stage::Empty) {
            layout.clear(slot);
        } else {
            layout.place(stage, slot);
        }
        return;
    }
    if (auto* fork = findForkAt(graph, ref.row, ref.column)) {
        const auto index = ref.column - fork->splitGap;
        if (index >= 0 && index < forkColumns) {
            fork->laneB[static_cast<size_t>(index)] = isEffectStage(stage) ? stage : Stage::Empty;
        }
    }
}

// Occupied targets swap with the source. Empty targets move the block.
inline bool moveBlock(Layout& layout, ForkGraph& graph, BlockRef from, BlockRef to) {
    if (from.laneB == to.laneB && from.row == to.row && from.column == to.column) {
        return false;
    }
    const auto source = stageAt(layout, graph, from);
    if (!isEffectStage(source)) {
        return false;
    }
    const auto destination = stageAt(layout, graph, to);
    writeStage(layout, graph, to, source);
    writeStage(layout, graph, from, destination);
    return true;
}

inline DropPreview previewDrop(const Layout& layout, const ForkGraph& graph, int row, int column, bool ontoLaneB) {
    DropPreview preview;
    if (row < 0 || row >= forkRowCount || column < 0 || column >= forkColumns) {
        return preview;
    }
    preview.target.row = row;
    preview.target.column = column;
    preview.target.laneB = ontoLaneB;
    if (!ontoLaneB) {
        preview.kind = DropKind::Slot;
        preview.swap = isEffectStage(stageAt(layout, graph, preview.target));
        return preview;
    }
    if (const auto* fork = findForkAt(graph, row, column)) {
        preview.kind = DropKind::Slot;
        preview.target.forkId = fork->id;
        preview.swap = fork->laneAt(column - fork->splitGap) != Stage::Empty;
        return preview;
    }
    const int splitGap = column;
    const int mergeGap = column + 1;
    if (!forkIntervalFits(graph, row, splitGap, mergeGap)) {
        return preview;
    }
    preview.kind = DropKind::NewFork;
    preview.splitGap = splitGap;
    preview.mergeGap = mergeGap;
    return preview;
}

inline bool removeFork(ForkGraph& graph, int forkId) {
    const auto before = graph.forks.size();
    graph.forks.erase(std::remove_if(graph.forks.begin(),
                                     graph.forks.end(),
                                     [forkId](const Bifurcation& fork) { return fork.id == forkId; }),
                      graph.forks.end());
    return graph.forks.size() != before;
}

inline ParallelRoute routeFromFork(const Bifurcation& fork) {
    ParallelRoute route;
    if (fork.columnCount() <= 0) {
        return route;
    }
    const auto first = movableForFork(fork.row, fork.splitGap);
    const auto last = movableForFork(fork.row, fork.mergeGap - 1);
    route.splitAfter = first - 1;
    route.mergeAfter = last;
    route.mix = fork.mix;
    for (int index = 0; index < fork.columnCount() && index < ParallelRoute::pathBCount; ++index) {
        route.placePathB(fork.laneAt(index), index);
    }
    return route;
}

inline ForkGraph graphFromRoute(const ParallelRoute& route) {
    ForkGraph graph;
    if (!route.active()) {
        return graph;
    }
    const auto first = route.splitAfter + 1;
    auto last = route.mergeAfter;
    if (first < 0 || first >= movableSlotCount) {
        return graph;
    }
    const auto row = forkRowForMovable(first);
    const auto rowLast = movableForFork(row, forkColumns - 1);
    if (last > rowLast) {
        last = rowLast;
    }
    if (last < first) {
        return graph;
    }
    Bifurcation fork;
    fork.id = 1;
    fork.row = row;
    fork.splitGap = forkColumnForMovable(first);
    fork.mergeGap = forkColumnForMovable(last) + 1;
    fork.mix = route.mix;
    for (int index = 0; index < fork.columnCount() && index < ParallelRoute::pathBCount; ++index) {
        fork.laneB[static_cast<size_t>(index)] = route.pathBAt(index);
    }
    graph.forks.push_back(fork);
    return graph;
}
}  // namespace SignalChain
