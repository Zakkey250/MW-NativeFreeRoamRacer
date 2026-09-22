// Frame geometry is intentionally stale after RefreshManaged retires cars.
// Never derive allocation protection from it. Read native membership AFTER
// retirement and preparation, immediately before the native constructor.
using ConstructionPointers=std::vector<const void*>;
ConstructionPointers g_constructionScratch; // Grow to live membership, reuse across spawns.
enum class ConstructionLifecycle { Ready, RetirementPending, Invalid };
ConstructionLifecycle ValidateConstructionLifecycles(const ConstructionPointers& pointers,unsigned count,const void** blocked=nullptr) noexcept {
    if(blocked)*blocked=nullptr;
    if(!count||count>pointers.size())return ConstructionLifecycle::Invalid;
    for(unsigned i=0;i<count;++i){
        if(blocked)*blocked=pointers[i];
        void* simable=GetSimablePointer(const_cast<void*>(pointers[i]));
        void** table=nullptr;void* kill=nullptr;std::uint8_t pending=255;
        // 006851DE: mov byte ptr [ISimable+1C],1; then enqueue at 00676540.
        // This is retirement requested, NOT immediate destruction. Never drop
        // such an entry from protection and construct in the same native epoch.
        if(!simable||!SafeRead(simable,&table)||
           reinterpret_cast<std::uintptr_t>(table)!=Address(kISimableVtable)||
           !SafeRead(table+2,&kill)||reinterpret_cast<std::uintptr_t>(kill)!=Address(kKillSimable)||
           !SafeRead(static_cast<const std::uint8_t*>(simable)+0x1C,&pending)||pending>1)
            return ConstructionLifecycle::Invalid;
        if(pending)return ConstructionLifecycle::RetirementPending;
    }
    if(blocked)*blocked=nullptr;
    return ConstructionLifecycle::Ready;
}
bool ConstructionContains(const ConstructionPointers& pointers,unsigned count,const void* vehicle) noexcept {
    return count<=pointers.size()&&std::find(pointers.begin(),pointers.begin()+count,vehicle)!=pointers.begin()+count;
}
bool CaptureConstructionLiveSet(ConstructionPointers& pointers,unsigned& count) noexcept {
    pointers.clear();count=0;
    const void* const* data=nullptr;unsigned nativeCount=0;
    if(!ReadNativeVehicleList(data,nativeCount)||!nativeCount)return false;
    try {pointers.resize(nativeCount);} catch (...) {return false;}
    for(unsigned i=0;i<nativeCount;++i){
        const void* vehicle=nullptr;
        if(!SafeRead(data+i,&vehicle)||!IsExpectedVehicle(const_cast<void*>(vehicle))||
           ConstructionContains(pointers,i,vehicle))return false;
        pointers[i]=vehicle;
    }
    const void* const* finalData=nullptr;unsigned finalCount=0;
    if(!ReadNativeVehicleList(finalData,finalCount)||finalData!=data||finalCount!=nativeCount)return false;
    for(unsigned i=0;i<nativeCount;++i){
        const void* vehicle=nullptr;
        if(!SafeRead(data+i,&vehicle)||vehicle!=pointers[i])return false;
    }
    count=nativeCount;return true;
}
