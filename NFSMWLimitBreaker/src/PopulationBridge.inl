// Called on the requesting MOD's simulation thread. The hook only reads a
// short lease; it neither kills vehicles nor changes logical/storage limits.
SRWLOCK g_populationLock=SRWLOCK_INIT;
PopulationLease g_populationLease;
volatile LONG g_populationActive=0;
using TrafficCapacityFn=bool(__thiscall*)(void*);
TrafficCapacityFn g_originalTrafficCapacity=nullptr;
constexpr std::uintptr_t kTrafficCapacityFunction=0x0041EBD0;
bool __fastcall TrafficCapacityHook(void* self,void*) {
    const bool native=g_originalTrafficCapacity(self);
    AcquireSRWLockShared(&g_populationLock);
    const auto reserved=g_populationLease.Active(GetTickCount64());
    ReleaseSRWLockShared(&g_populationLock);
    return TrafficAllowed(native,At<std::uint32_t>(kVehicleCount),g_logicalLimit,reserved);
}
bool InstallPopulationBridge() {
    const unsigned char expected[]={0x53,0x55,0x8B,0xD9,0x8B,0x83,0x94,0,0,0};
    if(!BytesEqual(kTrafficCapacityFunction,expected,sizeof(expected)))return false;
    const auto init=MH_Initialize();
    if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    auto* target=reinterpret_cast<void*>(kTrafficCapacityFunction);
    if(MH_CreateHook(target,&TrafficCapacityHook,reinterpret_cast<void**>(&g_originalTrafficCapacity))!=MH_OK)return false;
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return false;}
    InterlockedExchange(&g_populationActive,1);
    return true;
}
bool PopulationStorageStatus(nfsmw_population::Status& out) {
    if(At<std::uint8_t>(kTrafficCapacityFunction)!=0xE9||
       kTrafficCapacityFunction+5+At<std::int32_t>(kTrafficCapacityFunction+1)!=reinterpret_cast<std::uintptr_t>(&TrafficCapacityHook))return false;
    if(!g_storage||!BudgetPatchesMatch(g_logicalLimit)||
        !VictimVectorVtableOperandMatches(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_victimVtable.data())))||
        !ManagerVectorVtableOperandMatches(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_managerVtable.data())))||
        g_victimAllocationFailures||g_managerAllocationFailures)return false;
    out.limit=g_logicalLimit;out.live=At<std::uint32_t>(kVehicleCount);out.freeSlots=out.limit;
    for(std::size_t i=0;i<kRelocatedVectors.size();++i){
        const auto& spec=kRelocatedVectors[i];const auto count=At<std::uint32_t>(spec.object+12);
        const auto& storage=g_storage->vectors[i];
        if(At<std::uint32_t>(spec.object)!=spec.expectedVtable||
            At<std::uintptr_t>(spec.object+4)!=reinterpret_cast<std::uintptr_t>(storage.pointers.data())||
            At<std::uint32_t>(spec.object+8)!=out.limit||count>out.limit||
            storage.guardBefore!=kGuardBefore||storage.guardAfter!=kGuardAfter)return false;
        out.freeSlots=std::min(out.freeSlots,out.limit-count);
    }
    const auto manager=At<std::uintptr_t>(kAIVehicleManagerSingleton);
    if(manager){
        const auto vector=manager+kAIVehicleManagerVectorOffset;
        if(At<std::uintptr_t>(vector)!=reinterpret_cast<std::uintptr_t>(g_managerVtable.data()))return false;
        const auto data=At<std::uintptr_t>(vector+4),capacity=At<std::uint32_t>(vector+8),count=At<std::uint32_t>(vector+12);
        if(!(data==0&&capacity==0&&count==0)&&
            (!StorageBelongsToPool(g_managerPool,data)||capacity>kStorageCapacity||count>capacity))return false;
    }
    return true;
}
std::uint32_t ReservePopulation(std::uint32_t owner,std::uint32_t missing,nfsmw_population::Status* output) noexcept {
    __try {
        if(!output||output->size!=sizeof(*output)||!owner||missing>15)return nfsmw_population::Unsafe;
        *output={};
        if(!InterlockedCompareExchange(&g_populationActive,0,0))return nfsmw_population::Unavailable;
        // A provider invariant failure must not relax the consumer's safety cap.
        if(!PopulationStorageStatus(*output))return nfsmw_population::Unsafe;
        AcquireSRWLockExclusive(&g_populationLock);
        const auto before=g_populationLease.Active(GetTickCount64());
        const bool accepted=g_populationLease.Refresh(owner,missing,GetTickCount64());
        const auto after=g_populationLease.Active(GetTickCount64());
        ReleaseSRWLockExclusive(&g_populationLock);
        if(!accepted)return nfsmw_population::Busy;
        output->capabilities=nfsmw_population::requiredCapabilities;output->reserved=after;
        if(before!=after)Log(L"POPULATION_RESERVATION owner=%08lX missing=%lu live=%lu limit=%lu lease_ms=2000",
            static_cast<unsigned long>(owner),static_cast<unsigned long>(after),
            static_cast<unsigned long>(output->live),static_cast<unsigned long>(output->limit));
        return nfsmw_population::Available;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return nfsmw_population::Unsafe;}
}
