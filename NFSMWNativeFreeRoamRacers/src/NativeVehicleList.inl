// Validate actual readable storage, not an assumed LB configuration or release.
bool ReadablePointerArray(const void* data,std::size_t count) noexcept {
    const auto start=reinterpret_cast<std::uintptr_t>(data);
    if(!start||count>(std::numeric_limits<std::uintptr_t>::max()-start)/sizeof(void*))return false;
    const auto end=start+count*sizeof(void*);
    for(auto cursor=start;cursor<end;){
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))||region.State!=MEM_COMMIT||
           (region.Protect&PAGE_GUARD))return false;
        const DWORD access=region.Protect&0xFF;
        if(access!=PAGE_READONLY&&access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&
           access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)return false;
        const auto base=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if(region.RegionSize>std::numeric_limits<std::uintptr_t>::max()-base)return false;
        const std::uintptr_t next=base+region.RegionSize;
        if(next<=cursor)return false;
        cursor=std::min(end,next);
    }
    return true;
}
bool ReadNativeVehicleList(const void* const*& data,unsigned& count) noexcept {
    data=nullptr;count=0;unsigned capacity=0;
    const void* const* candidate=nullptr;unsigned observed=0;
    if(!SafeRead(reinterpret_cast<void*>(Address(kVehicleListData)),&candidate)||
       !SafeRead(reinterpret_cast<void*>(Address(kVehicleListData+4)),&capacity)||
       !SafeRead(reinterpret_cast<void*>(Address(kVehicleListCount)),&observed)||
       observed>capacity||!ReadablePointerArray(candidate,observed))return false;
    data=candidate;count=observed;return true;
}
