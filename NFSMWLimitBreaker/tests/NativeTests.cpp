#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "../src/VehicleLimitPlan.h"
#include "../src/PopulationLease.h"
#include "../src/ResourceApi.h"
#include "../third_party/minhook/include/MinHook.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include "../src/SkinResources.h"
#include "../src/SkinSlotOwnership.h"
#include "../src/CarPartPolicy.h"
#include "../../integration/AppearanceSafety.h"

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

int main(int argc,char** argv) {
    Check(car_part_capacity::Bytes(8192)==196656,"8192 model slots fit the native allocation");
    Check(car_part_capacity::Bytes(16384)==393264,"maximum model reservation has no arithmetic overflow");
    Check(!car_part_capacity::Valid(0)&&!car_part_capacity::Valid(1023)&&!car_part_capacity::Valid(16385),"invalid model capacities rejected");
    for(unsigned count=2;count<=16;++count)for(unsigned seed=0;seed<32;++seed){
        unsigned allowed=0;
        for(unsigned slot=0;slot<count;++slot)allowed+=mwappearance::RaceExterior(slot,count,seed);
        Check(allowed==(count<6?count:count/2),"race appearance quota includes preserved player");
        Check(mwappearance::RaceExterior(0,count,seed),"player exterior never replaced");
    }
    Check(mwappearance::RoamingExterior(3,3),"four live roaming cars retain unrestricted exteriors");
    Check(!mwappearance::RoamingExterior(4,4),"fifth spawn waits for natural exterior turnover");
    Check(mwappearance::RoamingExterior(4,1)&&!mwappearance::RoamingExterior(4,2),"five-car quota admits at most two customized exteriors");
    Check(mwappearance::RoamingExterior(5,2)&&!mwappearance::RoamingExterior(5,3),"six-car quota admits at most three customized exteriors");
    struct RacerFixture{unsigned pointer,key;bool customExterior;};
    const std::array<RacerFixture,4> managed{{{1,10,true},{2,20,true},{3,30,true},{4,40,false}}};
    const auto liveAppearance=mwappearance::CountRoaming(managed,[](const RacerFixture& r){return r.pointer==1||(r.pointer==4&&r.key==40);});
    Check(liveAppearance.live==2&&liveAppearance.customized==1,"missing and recycled generations excluded from roaming budget");
    std::array<std::int16_t,139> ownParts{},stockParts{};
    for(unsigned i=0;i<139;++i){ownParts[i]=std::int16_t(i+100);stockParts[i]=std::int16_t(i+1000);}
    mwappearance::StockGeometry(ownParts,stockParts);
    for(unsigned i=0;i<139;++i)Check(ownParts[i]==(i<76?stockParts[i]:std::int16_t(i+100)),"paint vinyl colors and decal textures survive stock exterior");
    bool rejected=false;try{skin_resources::Build({});}catch(const std::exception&){rejected=true;}
    Check(rejected,"truncated skin pack refused");
    rejected=false;try{skin_resources::Decode(skin_resources::Bytes(16));}catch(const std::exception&){rejected=true;}
    Check(rejected,"invalid compressed skin size refused");
    if(argc>1){
        std::string name=argv[1];std::wstring path(name.begin(),name.end());
        auto source=skin_resources::Read(path.c_str()),original=source;
        auto bundle=skin_resources::Build(source);
        std::cout << "Source external skin slots 13..20 indexed=" << skin_resources::HasExternalSlots(source) << "\n";
        Check(!skin_resources::HasExternalSlots(bundle),"private LB aliases cannot validate external DUMMY slots");
        Check(source==original,"installed texture bytes unchanged");
        Check(skin_resources::Word(bundle,0)==0xB3300000,"native resident pack type");
        Check(skin_resources::Word(bundle,4)==bundle.size()-8,"native resident pack length");
        unsigned count=0;for(auto block:skin_resources::Fields(bundle,8,bundle.size()))if(block.tag==0xB3310000)
          for(auto field:skin_resources::Fields(bundle,block.at,block.end))if(field.tag==0x33310002){count=unsigned((field.end-field.at)/8);unsigned previous=0;
            for(auto at=field.at;at<field.end;at+=8){auto key=skin_resources::Word(bundle,at);Check(key>previous,"private texture keys sorted and unique");previous=key;}}
        Check(count==48,"private sixteen-slot texture triplets");
        if(argc>2){std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(bundle.data()),bundle.size());Check(bool(output),"native pack fixture output");}
    }
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
    Check(kRelocatedVectors.size() == 15, "all fixed vectors including renderer included");
    const auto& renderSpec=kRelocatedVectors[kCarRenderConnectionVectorIndex];
    Check(renderSpec.object==0x009B37C8u && renderSpec.expectedVtable==0x008B51A8u,
        "race-end render registry identity from crash dump");
    Check(VectorDataAddress(renderSpec)==0x009B37CCu && VectorCountAddress(renderSpec)==0x009B37D4u,
        "race-end native renderer follows relocated data pointer and count");
    Check(!IsExpectedEmptyFixedVectorState(renderSpec,renderSpec.expectedVtable,renderSpec.object+16,20,30),
        "never relocate already overflowing crash state");
    Check(sizeof(StorageBlock)<=4096,"all fifteen guarded buffers fit allocation page");
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
    auto& renderStorage=storage.vectors[kCarRenderConnectionVectorIndex];
    const auto renderData=reinterpret_cast<std::uintptr_t>(renderStorage.pointers.data());
    for(unsigned cycle=0;cycle<100;++cycle) {
        for(unsigned count=0;count<=32;++count) {
            if(count) renderStorage.pointers[count-1]=0x10000u+(count-1)*0x100u;
            Check(CheckInvariants(renderStorage,renderSpec.expectedVtable,renderSpec.expectedVtable,
                renderData,renderData,32,32,count)==InvariantStatus::Ok,
                "renderer event/free-roam 0..32 registry occupancy retains guards");
        }
        Check(renderStorage.pointers[20]==0x11400u && renderStorage.pointers[29]==0x11D00u,
            "crash indices 20 and 29 are within owned renderer storage");
        renderStorage.pointers.fill(0);
    }
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

    Check(skin_slots::SupplementalFreeBits(0,20)==0xFF000u,"verified external allocator receives slots 13..20");
    Check(skin_slots::SupplementalFreeBits(0xFF000u,20)==0,"occupied extra slots cannot be reallocated");
    Check(skin_slots::SupplementalFreeBits(0xA000u,20)==0xF5000u,"mixed extra-slot ownership is preserved");
    Check((skin_slots::SupplementalFreeBits(0,20)&0xFFFu)==0,"native first twelve free bits stay untouched");
    Check(skin_slots::SupplementalFreeBits(0,12)==0&&skin_slots::SupplementalFreeBits(0,32)==0,"unknown or unverified allocator capacity fails closed");
    if (failures == 0) {
        std::cout << "PASS " << checks << " native checks\n";
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
