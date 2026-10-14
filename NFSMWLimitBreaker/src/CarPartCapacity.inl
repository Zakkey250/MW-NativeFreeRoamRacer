// The native eModel pool is separate from the skin bitmap and Cars byte arena.
// Reserve before InitCarRender, retaining the native allocator/freeing path.
// 4096 slots add 72 KiB versus stock; no texture/geometry banks are rewritten.
void InstallCarPartCapacity(const std::wstring& ini) {
    const auto requested=GetPrivateProfileIntW(L"VehicleResources",L"PartModelSlots",4096,ini.c_str());
    if(requested==0)return;
    if(requested<1024||requested>4096){Log(L"PART_CAPACITY inactive=invalid-config requested=%u",requested);return;}
    constexpr std::uintptr_t site=0x7376C0,operand=0x7376C8,pool=0x9B091C;
    const BYTE prefix[]={0x6A,0,0x68,0xC4,0x48,0x8B,0,0x68};
    const BYTE suffix[]={0x6A,0x18,0xE8,0x0D,0xEC,0xD2,0xFF};
    if(memcmp(reinterpret_cast<void*>(site),prefix,sizeof(prefix))||
       memcmp(reinterpret_cast<void*>(operand+4),suffix,sizeof(suffix))){
        Log(L"PART_CAPACITY inactive=owner-conflict native_preserved=1");return;
    }
    if(At<std::uintptr_t>(pool)){Log(L"PART_CAPACITY inactive=already-created restart_required=1");return;}
    const auto current=At<std::uint32_t>(operand);
    if(current>=requested){Log(L"PART_CAPACITY preserved=%u requested=%u",current,requested);return;}
    if(current!=1024){Log(L"PART_CAPACITY inactive=unknown-owner slots=%u",current);return;}
    const std::uint32_t slots=requested;
    if(WriteBytes(operand,&slots,sizeof(slots)))Log(L"PART_CAPACITY installed=1 slots=%u additional_bytes=%u native_allocator=1",slots,(slots-1024)*24);
}
