// Native SetLevel (00678AD0) interpolates the two references selected by
// SetPercent (00678730): count==3 uses [1,2], otherwise [0,1]. The chosen
// numeric career level is NOT a reference-array index. Never guess a target.
constexpr PerformanceLevels kPerformanceClassKeys{
    0xBD38D1CA,0x36350867,0xAFA210F0,0x07A7A3E5,0xF1F5FBC7,0xC92A0142,0xB1669F64};
struct PerformanceReferenceIssue {
    unsigned type=0,level=0,index=0;
    std::uint32_t collection=0;
    const char* reason="unknown";
};
struct PerformanceReferenceArray {
    std::uint32_t valueType=0,count=0;
    std::array<NFSPluginSDK::MW05::Attrib::RefSpec,3> refs{};
};

// Owned keys only, no native pointers. Invalid content stays excluded until
// catalog generation changes or the process restarts, including world re-entry.
struct InvalidPerformanceModels {
    std::uint64_t generation=~std::uint64_t{0};
    std::array<std::uint32_t,kCatalogCollectionLimit> keys{};
    std::size_t count=0;
    void Sync(std::uint64_t value) noexcept {if(generation!=value){generation=value;count=0;}}
    bool Contains(std::uint32_t key,std::uint64_t value) noexcept {
        Sync(value);
        return count==keys.size()||std::find(keys.begin(),keys.begin()+count,key)!=keys.begin()+count;
    }
    bool Reject(std::uint32_t key,std::uint64_t value) noexcept {
        if(Contains(key,value))return false;
        keys[count++]=key;return true;
    }
};
InvalidPerformanceModels g_invalidPerformanceModels;

template<class ReadArray,class Resolve>
bool ValidatePerformanceReferences(const CareerPerformancePlan& plan,ReadArray readArray,
                                   Resolve resolve,PerformanceReferenceIssue* issue) {
    for(unsigned type=0;type<7;++type) {
        *issue={type,plan.chosen[type],0,0,"bad-level"};
        if(plan.maximum[type]>6||plan.chosen[type]>plan.maximum[type])return false;
        // Stock RemovePart does not enter SetPercent's interpolation path.
        if(!plan.chosen[type])continue;
        PerformanceReferenceArray array{};
        if(!readArray(type,&array,issue))return false;
        issue->reason="bad-array";
        if(array.valueType!=0x2B936EB7||array.count<2||array.count>0xFFFF)return false;
        const unsigned first=array.count==3?1:0;
        for(unsigned index=first;index<=first+1;++index) {
            issue->index=index;
            const auto& ref=array.refs[index];issue->collection=ref.mCollectionKey;
            issue->reason="bad-class";
            if(ref.mClassKey!=kPerformanceClassKeys[type])return false;
            issue->reason="missing-row";
            // RefSpec::mCollectionPtr is a lazy cache, not proof of existence.
            // Native Instance(RefSpec), 00456CCA..00456CFD, resolves a null
            // cache by class/key. Check that target without writing the cache.
            if(!ref.mCollectionKey)return false;
            if(!resolve(ref,issue))return false;
        }
    }
    return true;
}

bool ReadPerformanceReferenceArray(NFSPluginSDK::MW05::Attrib::Instance* instance,unsigned type,
                                  PerformanceReferenceArray* out,PerformanceReferenceIssue* issue) noexcept {
    // Attrib::Attribute is four DWORDs (00452B30); its destructor only releases
    // the temporary instance lock. Never retain this object between samples.
    std::array<std::uintptr_t,4> attribute{};
    __try {
        __try {
            issue->reason="bad-array";
            if(!reinterpret_cast<bool(__thiscall*)(void*,unsigned,void*)>(Address(0x00454760))(
                    instance,kPerformanceClassKeys[type],attribute.data()))return false;
            out->valueType=reinterpret_cast<unsigned(__thiscall*)(void*)>(Address(0x00458660))(attribute.data());
            if(out->valueType!=0x2B936EB7)return false;
            out->count=reinterpret_cast<unsigned(__thiscall*)(void*)>(Address(0x00452D40))(attribute.data());
            if(out->count<2||out->count>0xFFFF)return false;
            // Match native endpoint choice; don't touch unused stock entry [0]
            // of three-reference arrays, and don't use native default storage.
            const unsigned first=out->count==3?1:0;
            for(unsigned index=first;index<=first+1;++index) {
                issue->index=index;
                const void* data=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(Address(0x00454C30))(
                    attribute.data(),index);
                if(!SafeRead(data,&out->refs[index]))return false;
            }
            return true;
        } __finally {
            reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00452BD0))(attribute.data());
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {issue->reason="exception";return false;}
}

bool ResolvePerformanceReference(const NFSPluginSDK::MW05::Attrib::RefSpec& ref,
                                 PerformanceReferenceIssue* issue) noexcept {
    using namespace NFSPluginSDK::MW05::Attrib;
    __try {
        issue->reason="missing-row";
        auto* found=reinterpret_cast<Collection*(__cdecl*)(unsigned,unsigned)>(Address(0x00455FD0))(
            ref.mClassKey,ref.mCollectionKey);
        if(!found)return false;
        issue->reason="stale-reference";
        if(ref.mCollectionPtr&&found!=ref.mCollectionPtr)return false;
        Class* cls=nullptr;std::uint32_t classKey=0,rowKey=0;
        issue->reason="bad-class";
        if(!SafeRead(&found->mClass,&cls)||!cls||!SafeRead(&cls->mKey,&classKey)||classKey!=ref.mClassKey)return false;
        issue->reason="bad-row";
        if(!SafeRead(&found->mKey,&rowKey)||rowKey!=ref.mCollectionKey)return false;
        HashMap table{};
        if(!SafeRead(&found->mTable,&table)||table.mNumEntries>table.mTableSize||
           (table.mTableSize&&!table.mTable))return false;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {issue->reason="exception";return false;}
}

bool ValidateNativePerformanceReferences(std::uint32_t key,NFSPluginSDK::MW05::Attrib::Instance* instance,
                                        const CareerPerformancePlan& plan) noexcept {
    PerformanceReferenceIssue issue{};
    const bool valid=ValidatePerformanceReferences(plan,
        [instance](unsigned type,PerformanceReferenceArray* out,PerformanceReferenceIssue* problem){
            return ReadPerformanceReferenceArray(instance,type,out,problem);},ResolvePerformanceReference,&issue);
    if(!valid&&g_invalidPerformanceModels.Reject(key,g_catalogGeneration))
        Log(LogLevel::Warning,"PERFORMANCE_REFERENCE_REJECT key=%08X type=%u level=%u index=%u collection=%08X reason=%s generation=%llu allocation=0 autoRepair=0 cachedUntilCatalogChange=1",
            key,issue.type,issue.level,issue.index,issue.collection,issue.reason,g_catalogGeneration);
    return valid;
}
