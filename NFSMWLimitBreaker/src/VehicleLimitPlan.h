#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace limitbreaker {

constexpr std::uint32_t kStockLimit = 20;
constexpr std::uint32_t kCrashDiagnosticThreshold = 24;
constexpr std::uint32_t kValidatedLimit = 28;
constexpr std::uint32_t kReleaseLimit = 32;
constexpr std::uint32_t kStorageCapacity = 64;
static_assert(kStockLimit <= kValidatedLimit);
static_assert(kValidatedLimit <= kReleaseLimit);
static_assert(kReleaseLimit < kStorageCapacity);
constexpr std::uint32_t kGuardBefore = 0x4C425052u; // "LBPR"
constexpr std::uint32_t kGuardAfter = 0x4C42504Fu;  // "LBPO"
constexpr std::size_t kVictimRecordSize = 0x18u;
constexpr std::uintptr_t kVictimVectorVtableOperand = 0x00687850u;
constexpr std::uint32_t kExpectedVictimVectorVtable = 0x008AA234u;
constexpr std::uintptr_t kAIVehicleManagerVectorVtableOperand = 0x0043CA61u;
constexpr std::uint32_t kExpectedAIVehicleManagerVectorVtable = 0x008918D8u;
constexpr std::uintptr_t kAIVehicleManagerSingleton = 0x0090D5F4u;
constexpr std::uintptr_t kAIVehicleManagerVectorOffset = 0xC4u;
constexpr std::size_t kPointerRecordSize = sizeof(std::uint32_t);
constexpr std::size_t kVictimPoolSlots = 16u;
constexpr std::size_t kAIVehicleManagerPoolSlots = 4u;

// Optional, restart-gated engine-audio arena expansion.  This is deliberately
// independent from the vehicle-vector relocation below: the stable 32/64
// vehicle implementation does not depend on it, and consumers must negotiate
// the capability through NFSMWLB_QueryResourceLimitsV1 before using more
// engine-audio slots.
constexpr std::uintptr_t kAudioMixerPoolMaskInstruction = 0x004CC3A9u;
constexpr std::uintptr_t kAudioMixerPoolMaskOperand = 0x004CC3AAu;
constexpr std::uint8_t kExpectedAudioMixerPoolMaskOpcode = 0x25u;
constexpr std::uint32_t kStockAudioMixerPoolMask = 0x00563000u;
constexpr std::uint32_t kAudioMixerPoolTailBytes = 0x00004000u;
constexpr std::uint32_t kExpandedAudioMixerPoolMiB = 8u;
constexpr std::uint64_t kAudioMonitorIntervalMs = 2000u;
constexpr std::uint32_t kAudioMinFreeLogStepBytes = 0x00040000u;
constexpr std::uintptr_t kAudioMixerPoolBaseGlobal = 0x00912840u;
constexpr std::uintptr_t kAudioMixerPoolSizeGlobal = 0x00912848u;
constexpr std::uintptr_t kAudioMixerArenaGlobal = 0x008EE558u;

constexpr std::uint32_t AudioMixerPoolBytesForMiB(
    const std::uint32_t mebibytes) {
    return mebibytes << 20;
}

constexpr std::uint32_t AudioMixerPoolMaskForMiB(
    const std::uint32_t mebibytes) {
    return AudioMixerPoolBytesForMiB(mebibytes) - kAudioMixerPoolTailBytes;
}

constexpr bool IsKnownAudioMixerPoolOwnerValue(
    const std::uint32_t mask, const std::uint32_t requestedMask) {
    return mask == kStockAudioMixerPoolMask || mask == requestedMask;
}

constexpr bool IsPreinitializedAudioArena(const std::uintptr_t arena,
    const std::uintptr_t base, const std::uint32_t bytes) {
    return (arena == 0 || arena == 0xFFFFFFFFu) && base == 0 && bytes == 0;
}

constexpr std::uintptr_t kVehicleVector = 0x0092CD18u;
constexpr std::uintptr_t kVehicleVectorStride = 0x60u;
constexpr std::size_t kVehicleVectorCount = 10;
constexpr std::uintptr_t kVehicleData = kVehicleVector + 4;
constexpr std::uintptr_t kVehicleCapacity = kVehicleVector + 8;
constexpr std::uintptr_t kVehicleCount = kVehicleVector + 12;
constexpr std::uintptr_t kStockInlineData = kVehicleVector + 16;
constexpr std::uintptr_t kStockInlineEnd = kStockInlineData + kStockLimit * sizeof(std::uint32_t);
constexpr std::uintptr_t kNextVector = kVehicleVector + 0x60;
constexpr std::uintptr_t kNextVectorData = kVehicleVector + 0x64;
constexpr std::uintptr_t kNextVectorCapacity = kVehicleVector + 0x68;
constexpr std::uintptr_t kNextVectorCount = kVehicleVector + 0x6C;
constexpr std::uintptr_t kNextVectorInlineData = kVehicleVector + 0x70;
constexpr std::uintptr_t kLastVector = kVehicleVector + 9 * 0x60;
constexpr std::uint32_t kExpectedVectorVtable = 0x008918D8u;

// Every live vehicle also registers its ISimable in this fixed-capacity AI
// target lookup vector.  Its 20 inline slots end immediately before the AI
// goal factory registry, so allowing it to overflow corrupts that registry.
constexpr std::uintptr_t kAITargetVector = 0x0090D660u;
constexpr std::uintptr_t kAITargetData = kAITargetVector + 4;
constexpr std::uintptr_t kAITargetCapacity = kAITargetVector + 8;
constexpr std::uintptr_t kAITargetCount = kAITargetVector + 12;
constexpr std::uintptr_t kAITargetInlineData = kAITargetVector + 16;
constexpr std::uintptr_t kAITargetInlineEnd =
    kAITargetInlineData + kStockLimit * sizeof(std::uint32_t);
constexpr std::uint32_t kExpectedAITargetVectorVtable = 0x008915B8u;

// Two per-vehicle audio/physics registries and one vehicle-component registry
// also use non-growing 20-pointer inline storage.  Runtime dumps at 22 live
// vehicles proved that all three can cross their stock capacity.
constexpr std::uintptr_t kVehicleAudioVector = 0x00913DB0u;
constexpr std::uint32_t kExpectedVehicleAudioVectorVtable = 0x00897774u;
constexpr std::uintptr_t kActiveVehicleComponentVector = 0x00913E70u;
constexpr std::uint32_t kExpectedActiveVehicleComponentVectorVtable = 0x008977A4u;
constexpr std::uintptr_t kVehicleComponentRegistryVector = 0x0092FE50u;
constexpr std::uint32_t kExpectedVehicleComponentRegistryVtable = 0x008AA454u;

struct FixedVectorSpec {
    const char* name;
    std::uintptr_t object;
    std::uint32_t expectedVtable;
};

constexpr std::size_t kAITargetVectorIndex = kVehicleVectorCount;
constexpr std::size_t kVehicleAudioVectorIndex = kAITargetVectorIndex + 1;
constexpr std::size_t kActiveVehicleComponentVectorIndex = kVehicleAudioVectorIndex + 1;
constexpr std::size_t kVehicleComponentRegistryVectorIndex =
    kActiveVehicleComponentVectorIndex + 1;

constexpr std::array<FixedVectorSpec, kVehicleComponentRegistryVectorIndex + 1>
kRelocatedVectors{{
    {"pvehicle-0", kVehicleVector + 0 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-1", kVehicleVector + 1 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-2", kVehicleVector + 2 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-3", kVehicleVector + 3 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-4", kVehicleVector + 4 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-5", kVehicleVector + 5 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-6", kVehicleVector + 6 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-7", kVehicleVector + 7 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-8", kVehicleVector + 8 * kVehicleVectorStride, kExpectedVectorVtable},
    {"pvehicle-9", kVehicleVector + 9 * kVehicleVectorStride, kExpectedVectorVtable},
    {"ai-targets", kAITargetVector, kExpectedAITargetVectorVtable},
    {"vehicle-audio", kVehicleAudioVector, kExpectedVehicleAudioVectorVtable},
    {"active-vehicle-components", kActiveVehicleComponentVector,
        kExpectedActiveVehicleComponentVectorVtable},
    {"vehicle-component-registry", kVehicleComponentRegistryVector,
        kExpectedVehicleComponentRegistryVtable},
}};

constexpr std::uintptr_t VectorDataAddress(const FixedVectorSpec& spec) {
    return spec.object + 4;
}

constexpr std::uintptr_t VectorCapacityAddress(const FixedVectorSpec& spec) {
    return spec.object + 8;
}

constexpr std::uintptr_t VectorCountAddress(const FixedVectorSpec& spec) {
    return spec.object + 12;
}

constexpr std::uintptr_t VectorInlineData(const FixedVectorSpec& spec) {
    return spec.object + 16;
}

struct PatchSite {
    const char* name;
    std::uintptr_t address;
    std::array<std::uint8_t, 3> stock;
    std::size_t size;
    std::size_t operandOffset;
    bool negativeOperand;
};

constexpr std::array<PatchSite, 5> kPatchSites{{
    {"vehicle-demand-compare", 0x00687817u, {0x83, 0xF8, 0x14}, 3, 2, false},
    {"vehicle-demand-excess", 0x0068781Cu, {0x8D, 0x50, 0xEC}, 3, 2, true},
    {"victim-vector-reserve", 0x00687854u, {0x6A, 0x14, 0x00}, 2, 1, false},
    {"traffic-capacity-compare", 0x0041EC13u, {0x83, 0xF8, 0x14}, 3, 2, false},
    {"traffic-capacity-scale", 0x00426330u, {0x83, 0xC2, 0x14}, 3, 2, false},
}};

struct GuardedPointerStorage {
    std::uint32_t guardBefore;
    std::array<std::uintptr_t, kStorageCapacity> pointers;
    std::uint32_t guardAfter;
};

struct StorageBlock {
    std::array<GuardedPointerStorage, kRelocatedVectors.size()> vectors;
};

enum class InvariantStatus {
    Ok,
    VtableChanged,
    DataPointerChanged,
    CapacityChanged,
    CountAboveLimit,
    CountAboveStorage,
    GuardBeforeChanged,
    GuardAfterChanged,
};

constexpr bool IsSupportedLimit(const std::uint32_t limit) {
    return limit == kStockLimit || limit == 24u || limit == 28u ||
        limit == kReleaseLimit;
}

constexpr std::uint32_t DiagnosticThresholdFor(const std::uint32_t limit) {
    return limit < kCrashDiagnosticThreshold ? limit : kCrashDiagnosticThreshold;
}

constexpr bool IsAddressInHalfOpenRange(const std::uintptr_t address,
    const std::uintptr_t begin, const std::uintptr_t end) {
    return begin < end && address >= begin && address < end;
}

constexpr std::size_t VictimAllocationBytes(const std::uint32_t capacity) {
    return static_cast<std::size_t>(capacity) * kVictimRecordSize;
}

constexpr std::size_t PointerAllocationBytes(const std::uint32_t capacity) {
    return static_cast<std::size_t>(capacity) * kPointerRecordSize;
}

constexpr bool IsExpectedEmptyFixedVectorState(
    const FixedVectorSpec& spec,
    const std::uint32_t vtable,
    const std::uintptr_t data,
    const std::uint32_t capacity,
    const std::uint32_t count) {
    if (vtable != spec.expectedVtable || count != 0) return false;
    return (data == 0 && capacity == 0) ||
        (data == VectorInlineData(spec) && capacity == kStockLimit);
}

constexpr std::uint8_t OperandFor(const PatchSite& site, const std::uint32_t limit) {
    return site.negativeOperand
        ? static_cast<std::uint8_t>(0u - limit)
        : static_cast<std::uint8_t>(limit);
}

inline InvariantStatus CheckInvariants(
    const GuardedPointerStorage& storage,
    const std::uint32_t liveVtable,
    const std::uint32_t expectedVtable,
    const std::uintptr_t liveData,
    const std::uintptr_t expectedData,
    const std::uint32_t liveCapacity,
    const std::uint32_t logicalLimit,
    const std::uint32_t liveCount) {
    if (liveVtable != expectedVtable) return InvariantStatus::VtableChanged;
    if (liveData != expectedData) return InvariantStatus::DataPointerChanged;
    if (liveCapacity != logicalLimit) return InvariantStatus::CapacityChanged;
    if (liveCount > kStorageCapacity) return InvariantStatus::CountAboveStorage;
    if (liveCount > logicalLimit) return InvariantStatus::CountAboveLimit;
    if (storage.guardBefore != kGuardBefore) return InvariantStatus::GuardBeforeChanged;
    if (storage.guardAfter != kGuardAfter) return InvariantStatus::GuardAfterChanged;
    return InvariantStatus::Ok;
}

inline const wchar_t* InvariantStatusName(const InvariantStatus status) {
    switch (status) {
    case InvariantStatus::Ok: return L"ok";
    case InvariantStatus::VtableChanged: return L"vtable-changed";
    case InvariantStatus::DataPointerChanged: return L"data-pointer-changed";
    case InvariantStatus::CapacityChanged: return L"capacity-changed";
    case InvariantStatus::CountAboveLimit: return L"count-above-logical-limit";
    case InvariantStatus::CountAboveStorage: return L"count-above-storage-capacity";
    case InvariantStatus::GuardBeforeChanged: return L"guard-before-changed";
    case InvariantStatus::GuardAfterChanged: return L"guard-after-changed";
    }
    return L"unknown";
}

} // namespace limitbreaker
