#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "../src/VehicleLimitPlan.h"
#include "../src/PopulationLease.h"
#include "../src/ResourceApi.h"
#include "../third_party/minhook/include/MinHook.h"
#include <cstring>

namespace {

int failures = 0;
int checks = 0;
using Predicate=bool(__thiscall*)(void*);
Predicate originalPredicate=nullptr;
unsigned fixtureLive=32,fixtureReserve=1;
bool __fastcall HookPredicate(void* self,void*) {
    return limitbreaker::TrafficAllowed(originalPredicate(self),fixtureLive,32,fixtureReserve);
}

void Check(const bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << name << '\n';
    }
}

} // namespace

int main() {
    using namespace limitbreaker;
    PopulationLease lease;
    Check(!lease.Refresh(0,1,100),"lease requires owner");
    Check(!lease.Refresh(1,16,100),"lease bounded to fifteen");
    Check(lease.Refresh(1,6,100)&&lease.Active(2099)==6&&lease.Active(2100)==0,"lease expires at two seconds");
    Check(!lease.Refresh(2,1,101),"foreign active owner rejected");
    Check(lease.Refresh(1,3,102)&&lease.Active(2101)==3,"owner renews missing slots");
    Check(lease.Refresh(1,0,103)&&lease.Active(104)==0,"zero releases reservation");
    Check(lease.Refresh(2,2,104),"next owner after release");
    for(unsigned limit:{20u,24u,28u,32u})for(unsigned reserved=0;reserved<=15;++reserved)
        for(unsigned live=0;live<=limit;++live){
            Check(!TrafficAllowed(false,live,limit,reserved),"native rejection preserved");
            Check(TrafficAllowed(true,live,limit,reserved)==(!reserved||live<limit-reserved),"traffic reservation boundary");
        }
    auto* code=VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    // Same complete overwritten instruction sequence as 0x41EBD0; a bounded
    // fixture epilogue returns the ECX object's +94 field through AL.
    const unsigned char body[]={0x53,0x55,0x8B,0xD9,0x8B,0x83,0x94,0,0,0,0x5D,0x5B,0xC3};
    Check(code!=nullptr,"trampoline fixture allocation");
    if(code){
        std::memcpy(code,body,sizeof(body));FlushInstructionCache(GetCurrentProcess(),code,sizeof(body));
        std::array<unsigned,40> object{};object[0x94/4]=1;
        const auto predicate=reinterpret_cast<Predicate>(code);
        Check(predicate(object.data()),"thiscall fixture before hook");
        Check(MH_Initialize()==MH_OK,"fixture minhook initialize");
        const bool created=MH_CreateHook(code,&HookPredicate,reinterpret_cast<void**>(&originalPredicate))==MH_OK;
        Check(created,"native prologue trampoline created");
        if(created){
            Check(MH_EnableHook(code)==MH_OK,"fixture hook enabled");
            for(unsigned i=0;i<1000;++i){fixtureLive=i%2?30:32;Check(predicate(object.data())==(fixtureLive==30),"thiscall trampoline stack and reservation");}
            object[0x94/4]=0;Check(!predicate(object.data()),"hook preserves native false");
            MH_DisableHook(code);MH_RemoveHook(code);
        }
        MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);
    }
    Check(IsSupportedLimit(20), "limit 20 accepted");
    Check(IsSupportedLimit(24), "limit 24 accepted");
    Check(IsSupportedLimit(28), "validated limit 28 accepted");
    Check(IsSupportedLimit(32), "limit 32 accepted in staged expansion");
    Check(IsSupportedLimit(20) && IsSupportedLimit(24) &&
        IsSupportedLimit(28), "stock and four-vehicle release stages accepted");
    Check(!IsSupportedLimit(21) && !IsSupportedLimit(22) &&
        !IsSupportedLimit(23) && !IsSupportedLimit(25) &&
        !IsSupportedLimit(29) && !IsSupportedLimit(31),
        "non-stage vehicle limits rejected");
    Check(!IsSupportedLimit(19), "limit 19 rejected");
    Check(!IsSupportedLimit(33), "limit 33 rejected");
    Check(AudioMixerPoolBytesForMiB(kExpandedAudioMixerPoolMiB) == 0x00800000u,
        "engine audio arena is fixed at 8 MiB");
    Check(AudioMixerPoolMaskForMiB(kExpandedAudioMixerPoolMiB) == 0x007FC000u,
        "engine audio arena mask preserves native tail allocation");
    Check(IsKnownAudioMixerPoolOwnerValue(kStockAudioMixerPoolMask, 0x007FC000u),
        "stock audio mixer mask accepted as patch owner");
    Check(IsKnownAudioMixerPoolOwnerValue(0x007FC000u, 0x007FC000u),
        "requested audio mixer mask accepted as idempotent owner");
    Check(!IsKnownAudioMixerPoolOwnerValue(0x00600000u, 0x007FC000u),
        "foreign audio mixer mask rejected");
    Check(IsPreinitializedAudioArena(0, 0, 0),
        "cold audio arena accepted");
    Check(IsPreinitializedAudioArena(0xFFFFFFFFu, 0, 0),
        "native audio arena sentinel accepted");
    Check(!IsPreinitializedAudioArena(1, 0, 0),
        "initialized audio arena rejected");
    Check(sizeof(nfsmw_limitbreaker::ResourceLimitsV1) == 28,
        "resource capability ABI size remains stable");
    Check(nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded == 1,
        "resource audio capability bit remains stable");
    Check(kAudioMonitorIntervalMs == 2000,
        "audio diagnostics use a low-frequency monitor");
    Check(kAudioMinFreeLogStepBytes == 0x00040000u,
        "audio allocator telemetry is coalesced in 256 KiB steps");
    Check(DiagnosticThresholdFor(20) == 20, "stock diagnostic threshold");
    Check(DiagnosticThresholdFor(24) == 24, "validated diagnostic threshold");
    Check(DiagnosticThresholdFor(28) == 24, "expanded diagnostic threshold");
    Check(DiagnosticThresholdFor(32) == 24, "32-vehicle diagnostic threshold");
    Check(IsAddressInHalfOpenRange(0x00400000u, 0x00400000u, 0x00A79000u),
        "exception filter accepts image start");
    Check(IsAddressInHalfOpenRange(0x00687956u, 0x00400000u, 0x00A79000u),
        "exception filter accepts game fault");
    Check(!IsAddressInHalfOpenRange(0x00A79000u, 0x00400000u, 0x00A79000u),
        "exception filter rejects image end");
    Check(!IsAddressInHalfOpenRange(0x6DDD6433u, 0x00400000u, 0x00A79000u),
        "exception filter rejects foreign ASI fault");
    Check(kStockInlineEnd == 0x0092CD78u, "stock inline boundary");
    Check(kNextVector == 0x0092CD78u, "adjacent vector object boundary");
    Check(kNextVectorData == 0x0092CD7Cu, "adjacent vector boundary");
    Check(kAITargetInlineData == 0x0090D670u, "AI target inline start");
    Check(kAITargetInlineEnd == 0x0090D6C0u, "AI target inline boundary");
    Check(kVictimVectorVtableOperand == 0x00687850u,
        "victim vector vtable operand address");
    Check(kAIVehicleManagerVectorVtableOperand == 0x0043CA61u,
        "AI vehicle manager vector vtable operand address");
    Check(kAIVehicleManagerVectorOffset == 0xC4u,
        "AI vehicle manager vector member offset");
    Check(kAIVehicleManagerSingleton == 0x0090D5F4u,
        "AI vehicle manager singleton address");
    Check(VictimAllocationBytes(32) == 0x300u,
        "victim vector 32-record allocation size");
    Check(PointerAllocationBytes(64) == 0x100u,
        "AI vehicle manager 64-pointer storage size");
    Check(kReleaseLimit == 32,
        "release logical limit remains 32");
    Check(kStorageCapacity - kReleaseLimit == 32,
        "32 containment slots remain above the release logical limit");
    Check(kRelocatedVectors.size() == 14, "all fixed vectors included");
    Check(kRelocatedVectors[kVehicleAudioVectorIndex].object == 0x00913DB0u,
        "vehicle audio vector address");
    Check(kRelocatedVectors[kActiveVehicleComponentVectorIndex].object == 0x00913E70u,
        "active vehicle component vector address");
    Check(kRelocatedVectors[kVehicleComponentRegistryVectorIndex].object == 0x0092FE50u,
        "vehicle component registry vector address");
    const FixedVectorSpec& aiSpec = kRelocatedVectors[kAITargetVectorIndex];
    Check(IsExpectedEmptyFixedVectorState(aiSpec,
        kExpectedAITargetVectorVtable, 0, 0, 0),
        "AI target cold empty state accepted");
    Check(IsExpectedEmptyFixedVectorState(aiSpec, kExpectedAITargetVectorVtable,
        kAITargetInlineData, kStockLimit, 0), "AI target inline empty state accepted");
    Check(!IsExpectedEmptyFixedVectorState(aiSpec, kExpectedAITargetVectorVtable,
        kAITargetInlineData, kStockLimit, 1), "AI target active state rejected");
    Check(OperandFor(kPatchSites[0], 32) == 0x20, "positive operand");
    Check(OperandFor(kPatchSites[1], 32) == 0xE0, "negative operand");

    StorageBlock storage{};
    for (GuardedPointerStorage& vectorStorage : storage.vectors) {
        vectorStorage.guardBefore = kGuardBefore;
        vectorStorage.guardAfter = kGuardAfter;
    }
    GuardedPointerStorage& primaryStorage = storage.vectors[0];
    const std::uintptr_t data =
        reinterpret_cast<std::uintptr_t>(primaryStorage.pointers.data());
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data, data, 32, 32, 0) == InvariantStatus::Ok,
        "valid empty state");
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data, data, 32, 32, 32) == InvariantStatus::Ok,
        "valid full state");
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data, data, 32, 32, 33) == InvariantStatus::CountAboveLimit,
        "logical overflow detected");
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data, data, 32, 32, 65) == InvariantStatus::CountAboveStorage,
        "physical overflow detected");
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable + 4, kExpectedVectorVtable,
        data, data, 32, 32, 0) == InvariantStatus::VtableChanged,
        "vtable mutation detected");
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data + 4, data, 32, 32, 0) == InvariantStatus::DataPointerChanged,
        "data pointer mutation detected");
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data, data, 28, 32, 0) == InvariantStatus::CapacityChanged,
        "capacity mutation detected");
    primaryStorage.guardAfter = 0;
    Check(CheckInvariants(primaryStorage, kExpectedVectorVtable, kExpectedVectorVtable,
        data, data, 32, 32, 0) == InvariantStatus::GuardAfterChanged,
        "tail guard mutation detected");

    GuardedPointerStorage& aiStorage = storage.vectors[kAITargetVectorIndex];
    const std::uintptr_t aiData =
        reinterpret_cast<std::uintptr_t>(aiStorage.pointers.data());
    Check(CheckInvariants(aiStorage, kExpectedAITargetVectorVtable,
        kExpectedAITargetVectorVtable, aiData, aiData, 32, 32, 31) == InvariantStatus::Ok,
        "AI target companion state");

    if (failures == 0) {
        std::cout << "PASS " << checks << " native checks\n";
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
