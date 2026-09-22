// Native 00507460 unloads an 80134010 solid, but 00507534 clears only
// the supplied owner's table. Other loaded packs can cache that SAME pointer.
// Unlink exact aliases before native destruction; never retain freed objects,
// choose a replacement by hash, or suppress the original destructor.
constexpr std::uintptr_t kSolidUnload=0x00507460;
constexpr std::uintptr_t kSolidPackHead=0x0091A0D0;
using SolidUnloadFn=void(__cdecl*)(void*,void*);
SolidUnloadFn g_originalSolidUnload=nullptr;
struct SolidAliasPatch {std::uintptr_t slot=0,pack=0;};
struct SolidKeyPointer {
    std::uint32_t key=0;std::uintptr_t pointer=0;
    bool operator==(const SolidKeyPointer&) const = default;
};
static_assert(sizeof(SolidKeyPointer)==8);

bool SolidWritable(std::uintptr_t address,std::size_t bytes) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if(!address||bytes>UINTPTR_MAX-address||
       !VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))||
       region.State!=MEM_COMMIT||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const DWORD access=region.Protect&0xFF;
    return (access==PAGE_READWRITE||access==PAGE_WRITECOPY||
            access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY)&&
        address+bytes<=reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize;
}

// Resolve the actual header from this exact unload chunk, not a global
// same-key lookup (which may return a different live duplicate).
bool ReadUnloadingSolid(std::uintptr_t chunk,std::uintptr_t* solid,std::uint32_t* key) noexcept {
    *solid=0;*key=0;
    SolidKeyPointer top{};
    if(!SafeRead(reinterpret_cast<void*>(chunk),&top))return false;
    if(top.key!=0x80134010)return true; // Native routine is a no-op too.
    const auto size=top.pointer;
    if(size>0x10000000u||chunk>UINTPTR_MAX-8u-size)return false;
    const auto end=chunk+8+size;
    std::uintptr_t found=0;
    unsigned children=0;
    for(auto cursor=chunk+8;cursor<end;) {
        SolidKeyPointer child{};
        if(++children>65536||end-cursor<8||!SafeRead(reinterpret_cast<void*>(cursor),&child)||
           child.pointer>end-cursor-8)return false;
        if(child.key==0x00134011) {
            if(cursor>UINTPTR_MAX-0x17u)return false;
            const auto aligned=(cursor+0x17u)&~std::uintptr_t(15);
            if(found||aligned>cursor+8+child.pointer||cursor+8+child.pointer-aligned<0x20)return false;
            found=aligned;
        }
        cursor+=8+child.pointer;
    }
    if(!found||!SafeRead(reinterpret_cast<void*>(found+0x10),key))return false;
    *solid=found;return true;
}

// Complete read-only preflight before changing any table. Binary-search the
// native sorted key arrays, then inspect all equal-key entries. Bounds protect
// malformed/cyclic lists without any vehicle-count or LB-version assumptions.
bool CollectSolidAliases(std::uintptr_t head,std::uintptr_t solid,std::uint32_t key,
                         std::uintptr_t owner,std::vector<SolidAliasPatch>* patches) {
    std::uintptr_t pack=0,previous=head;
    if(!SafeRead(reinterpret_cast<void*>(head),&pack))return false;
    unsigned packs=0;
    while(pack!=head) {
        std::uintptr_t next=0,back=0,table=0;std::uint32_t count=0;
        if(!pack||pack>UINTPTR_MAX-0x90u||++packs>4096||
           !SafeRead(reinterpret_cast<void*>(pack),&next)||
           !SafeRead(reinterpret_cast<void*>(pack+4),&back)||back!=previous||
           !SafeRead(reinterpret_cast<void*>(pack+0xC),&count)||count>65536||
           !SafeRead(reinterpret_cast<void*>(pack+0x74),&table)||
           (count&&(!table||table>UINTPTR_MAX-count*sizeof(SolidKeyPointer))))return false;
        if(pack!=owner) {
            std::uint32_t low=0,high=count;
            while(low<high) {
                const auto mid=low+(high-low)/2;std::uint32_t candidate=0;
                if(!SafeRead(reinterpret_cast<void*>(table+mid*8),&candidate))return false;
                if(candidate<key)low=mid+1;else high=mid;
            }
            for(auto index=low;index<count;++index) {
                SolidKeyPointer entry{};const auto address=table+index*8;
                if(!SafeRead(reinterpret_cast<void*>(address),&entry))return false;
                if(entry.key!=key)break;
                if(entry.pointer!=solid)continue;
                if(patches->size()>=4096||!SolidWritable(address+4,4)||!SolidWritable(pack+0x7C,2))return false;
                patches->push_back({address+4,pack});
            }
        }
        previous=pack;pack=next;
    }
    std::uintptr_t tail=0;
    return SafeRead(reinterpret_cast<void*>(head+4),&tail)&&tail==previous;
}

bool DetachSolidAliases(std::uintptr_t head,std::uintptr_t chunk,std::uintptr_t owner,
                       unsigned* detached) {
    *detached=0;
    std::uintptr_t solid=0;std::uint32_t key=0;
    if(!ReadUnloadingSolid(chunk,&solid,&key))return false;
    if(!solid)return true;
    std::vector<SolidAliasPatch> patches; // Allocate only when an alias exists.
    if(!CollectSolidAliases(head,solid,key,owner,&patches))return false;
    for(const auto& patch:patches) {
        // Same two operations as native owner invalidation at 00507534/558.
        // 00506D00 rebuilds dependencies via 00506C90 before iterating solids.
        *reinterpret_cast<std::uintptr_t*>(patch.slot)=0;
        *reinterpret_cast<std::uint16_t*>(patch.pack+0x7C)=0xFFFF;
        ++*detached;
    }
    return true;
}

bool TryDetachSolidAliases(void* chunk,void* owner,unsigned* detached) noexcept {
    try {
        return DetachSolidAliases(Address(kSolidPackHead),reinterpret_cast<std::uintptr_t>(chunk),
            reinterpret_cast<std::uintptr_t>(owner),detached);
    } catch(...) {return false;}
}
bool SafeDetachSolidAliases(void* chunk,void* owner,unsigned* detached) noexcept {
    __try {return TryDetachSolidAliases(chunk,owner,detached);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void __cdecl SolidUnloadHook(void* chunk,void* owner) {
    unsigned detached=0;
    const bool complete=SafeDetachSolidAliases(chunk,owner,&detached);
    static unsigned repairs=0,failures=0;
    if(!complete) {
        // No guessed repair or destructor suppression on unsupported data.
        // Stop producing more racers; the game's lifecycle still runs once.
        g_faulted=true;
        if(++failures<=4)Log(LogLevel::Error,"SOLID_LIFETIME incomplete=1 furtherSpawnsStopped=1 nativeUnloadForwarded=1");
    } else if(detached&&((++repairs<=8)||(repairs%128==0))) {
        Log(LogLevel::Info,"SOLID_LIFETIME detached=%u unloadsWithAliases=%u exactPointer=1 ownerPreserved=1 nativeUnloadForwarded=1",
            detached,repairs);
    }
    g_originalSolidUnload(chunk,owner);
}

bool ValidateSolidLifetimeSurface() noexcept {
    struct Guard {std::uintptr_t address;unsigned size;std::array<std::uint8_t,12> bytes;};
    const Guard guards[]={
        {kSolidUnload,11,{0x53,0x8B,0x5C,0x24,0x08,0x81,0x3B,0x10,0x40,0x13,0x80}},
        {0x00507534,7,{0xC7,0x40,0x04,0,0,0,0}},
        {0x00507558,6,{0x66,0xC7,0x46,0x7C,0xFF,0xFF}},
        {0x00506D00,12,{0x51,0x56,0x8B,0x74,0x24,0x0C,0x56,0xE8,0x84,0xFF,0xFF,0xFF}},
    };
    for(const auto& guard:guards) {
        std::array<std::uint8_t,12> actual{};
        if(!SafeRead(reinterpret_cast<void*>(Address(guard.address)),&actual)||
           !std::equal(guard.bytes.begin(),guard.bytes.begin()+guard.size,actual.begin()))return false;
    }
    return true;
}
