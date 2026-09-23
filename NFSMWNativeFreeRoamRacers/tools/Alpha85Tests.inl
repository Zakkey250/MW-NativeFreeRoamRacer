namespace alpha85fixture {
using Collection=NFSPluginSDK::MW05::Attrib::Collection;
using Ref=NFSPluginSDK::MW05::Attrib::RefSpec;
PerformanceReferenceArray array{};
Collection* found=nullptr;
unsigned releases=0,elementCalls=0;
bool fault=false;
bool __fastcall Attribute(void*,void*,unsigned,void* output) {
    static_cast<std::uintptr_t*>(output)[0]=1;
    if(fault)RaiseException(0xC0000005,0,0,nullptr);
    return true;
}
unsigned __fastcall Type(void*,void*){return array.valueType;}
unsigned __fastcall Count(void*,void*){return array.count;}
void* __fastcall Element(void*,void*,unsigned i){++elementCalls;return i<3?&array.refs[i]:nullptr;}
void __fastcall Release(void*,void*){++releases;}
Collection* __cdecl Find(unsigned,unsigned){return found;}
}

template<class Expect>
__declspec(noinline) void RunAlpha85NativeAdapterTests(Expect expect) {
    using namespace alpha85fixture;
    auto* code=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x70000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    expect(code!=nullptr,"alpha85 native adapter fixture allocated");if(!code)return;
    const auto savedBase=g_base;g_base=reinterpret_cast<std::uintptr_t>(code);
    auto bind=[&](unsigned address,auto fn){
        unsigned char thunk[]={0xB8,0,0,0,0,0xFF,0xE0};
        const auto target=reinterpret_cast<std::uintptr_t>(fn);std::memcpy(thunk+1,&target,4);
        std::memcpy(reinterpret_cast<void*>(Address(address)),thunk,sizeof(thunk));};
    bind(0x00454760,&Attribute);bind(0x00458660,&Type);bind(0x00452D40,&Count);
    bind(0x00454C30,&Element);bind(0x00452BD0,&Release);bind(0x00455FD0,&Find);
    FlushInstructionCache(GetCurrentProcess(),code,0x70000);
    PerformanceReferenceIssue issue{};PerformanceReferenceArray output{};
    NFSPluginSDK::MW05::Attrib::Instance instance{};
    array={};array.valueType=0x2B936EB7;array.count=2;releases=elementCalls=0;fault=false;
    expect(ReadPerformanceReferenceArray(&instance,1,&output,&issue)&&releases==1&&elementCalls==2,
        "alpha85 production array adapter balances temporary lock on valid two-element read");
    array.count=3;elementCalls=0;
    expect(ReadPerformanceReferenceArray(&instance,1,&output,&issue)&&releases==2&&elementCalls==2,
        "alpha85 production three-element adapter reads only interpolation endpoints");
    array.count=1;elementCalls=0;
    expect(!ReadPerformanceReferenceArray(&instance,1,&output,&issue)&&releases==3&&elementCalls==0,
        "alpha85 malformed count rejected before element access and releases lock");
    fault=true;
    expect(!ReadPerformanceReferenceArray(&instance,1,&output,&issue)&&releases==4&&std::string(issue.reason)=="exception",
        "alpha85 production SEH adapter rejects native exception and releases lock");fault=false;
    Collection row{};alignas(4) unsigned classData[3]={kPerformanceClassKeys[1],0,0};
    row.mClass=reinterpret_cast<NFSPluginSDK::MW05::Attrib::Class*>(classData);row.mKey=0x26A10B03;
    Ref ref{kPerformanceClassKeys[1],row.mKey,&row};found=&row;
    expect(ResolvePerformanceReference(ref,&issue),"alpha85 production resolver accepts matching class key row key and native pointer");
    found=nullptr;expect(!ResolvePerformanceReference(ref,&issue),"alpha85 production resolver rejects nonexistent row");
    found=&row;ref.mCollectionPtr=nullptr;
    expect(ResolvePerformanceReference(ref,&issue)&&ref.mCollectionPtr==nullptr,
        "alpha86 production resolver accepts valid lazy reference without mutating source cache");
    found=nullptr;
    expect(!ResolvePerformanceReference(ref,&issue),"alpha86 lazy reference with missing target still rejected");
    found=&row;Collection other{};ref.mCollectionPtr=&other;
    expect(!ResolvePerformanceReference(ref,&issue),"alpha86 nonnull stale cache still rejected");
    ref.mCollectionPtr=&row;classData[0]^=1;
    expect(!ResolvePerformanceReference(ref,&issue),"alpha85 production resolver rejects returned wrong class");
    classData[0]^=1;row.mKey^=1;
    expect(!ResolvePerformanceReference(ref,&issue),"alpha85 production resolver rejects returned wrong collection key");
    row.mKey^=1;row.mTable.mTableSize=2;
    expect(!ResolvePerformanceReference(ref,&issue),"alpha85 production resolver rejects null nonempty attribute table");
    row.mTable.mTableSize=0;row.mTable.mNumEntries=1;
    expect(!ResolvePerformanceReference(ref,&issue),"alpha85 production resolver rejects inconsistent table counts");
    row.mTable.mNumEntries=0;
    const auto savedInvalid=std::make_unique<InvalidPerformanceModels>(g_invalidPerformanceModels);
    const auto savedGeneration=g_catalogGeneration;
    g_invalidPerformanceModels={};g_catalogGeneration=8601;
    array.count=2;array.refs[0]={kPerformanceClassKeys[1],row.mKey,nullptr};array.refs[1]=array.refs[0];
    CareerPerformancePlan plan{};plan.maximum[1]=4;plan.chosen[1]=2;
    expect(ValidateNativePerformanceReferences(86,&instance,plan)&&g_invalidPerformanceModels.count==0&&
        array.refs[0].mCollectionPtr==nullptr&&array.refs[1].mCollectionPtr==nullptr,
        "alpha86 actual array plus resolver pipeline accepts unresolved valid endpoints without exclusion or writes");
    found=nullptr;
    expect(!ValidateNativePerformanceReferences(87,&instance,plan)&&g_invalidPerformanceModels.Contains(87,8601),
        "alpha86 actual pipeline still caches genuinely missing row rejection");
    found=&row;
    expect(ValidateNativePerformanceReferences(86,&instance,plan)&&!g_invalidPerformanceModels.Contains(86,8601),
        "alpha86 missing other model never poisons valid lazy model");
    g_invalidPerformanceModels=*savedInvalid;g_catalogGeneration=savedGeneration;
    found=nullptr;g_base=savedBase;VirtualFree(code,0,MEM_RELEASE);
}

template<class Expect>
__declspec(noinline) void RunAlpha85Tests(Expect expect) {
    RunAlpha85NativeAdapterTests(expect);
    using Ref=NFSPluginSDK::MW05::Attrib::RefSpec;
    using Collection=NFSPluginSDK::MW05::Attrib::Collection;
    CareerPerformancePlan plan{{3,4,3,4,4,3,3},{1,2,1,2,2,1,1}};
    const auto original=plan;
    std::array<PerformanceReferenceArray,7> arrays{};
    for(unsigned t=0;t<7;++t) {
        arrays[t].valueType=0x2B936EB7;arrays[t].count=2;
        for(unsigned i=0;i<3;++i)arrays[t].refs[i]={kPerformanceClassKeys[t],100+t*3+i,reinterpret_cast<Collection*>(0x1000+t*64+i*4)};
    }
    const auto validArrays=arrays;
    PerformanceReferenceIssue issue{};unsigned reads=0,resolves=0,constructs=0;
    auto read=[&](unsigned t,PerformanceReferenceArray* out,PerformanceReferenceIssue*){++reads;*out=arrays[t];return true;};
    auto resolve=[&](const Ref& ref,PerformanceReferenceIssue* p){++resolves;
        if(ref.mCollectionKey==0x3A41650A){p->reason="missing-row";return false;}return true;};
    auto attempt=[&]{if(!ValidatePerformanceReferences(plan,read,resolve,&issue))return false;++constructs;return true;};
    expect(attempt()&&reads==7&&resolves==14&&constructs==1,"alpha85 valid seven-system interpolation reaches construction once");
    arrays[1].refs[1].mCollectionKey=0x3A41650A;arrays[1].refs[1].mCollectionPtr=nullptr;
    expect(!attempt()&&constructs==1&&issue.type==1&&issue.level==2&&issue.index==1&&
        std::string(issue.reason)=="missing-row","alpha85 original dangling second brakes reference rejected before construct");
    arrays[1].refs[1].mCollectionKey=0x26A10B03;arrays[1].refs[1].mCollectionPtr=reinterpret_cast<Collection*>(0x1044);
    expect(attempt()&&constructs==2&&plan.chosen==original.chosen,"alpha85 corrected fn2_top accepted without changing brakes level2");
    for(unsigned t=0;t<7;++t) {
        arrays=validArrays;arrays[t].refs[1].mClassKey^=1;
        expect(!attempt()&&issue.type==t&&std::string(issue.reason)=="bad-class","alpha85 wrong class rejected for each system");
        arrays=validArrays;arrays[t].refs[0].mCollectionKey=0;
        expect(!attempt()&&issue.type==t,"alpha85 null base key rejected for each system");
        arrays=validArrays;arrays[t].refs[1].mCollectionPtr=nullptr;
        expect(attempt(),"alpha86 valid lazy top reference accepted for each system");
        arrays=validArrays;arrays[t].count=3;arrays[t].refs[0]={};
        expect(attempt(),"alpha85 three-entry interpolation uses1and2 not unused stock0");
        arrays[t].refs[2].mCollectionPtr=nullptr;arrays[t].refs[2].mCollectionKey=0x3A41650A;
        expect(!attempt()&&issue.index==2,"alpha85 three-entry missing top rejected");
    }
    arrays=validArrays;
    for(unsigned count:{0u,1u,0xffffffffu}) {
        arrays[1].count=count;expect(!attempt()&&std::string(issue.reason)=="bad-array","alpha85 malformed array length rejected");
    }
    arrays=validArrays;arrays[1].count=4;
    expect(attempt(),"alpha85 longer arrays retain native first-two endpoint rule");
    arrays=validArrays;arrays[1].valueType=0;
    expect(!attempt()&&std::string(issue.reason)=="bad-array","alpha85 non-reference attribute rejected");
    arrays=validArrays;plan.chosen[1]=5;
    expect(!attempt()&&std::string(issue.reason)=="bad-level","alpha85 above-maximum level rejected");
    plan=original;plan.chosen[1]=4;
    expect(attempt(),"alpha85 level4 valid with two references numeric level not used as index");
    plan={};reads=0;resolves=0;
    expect(attempt()&&reads==0&&resolves==0,"alpha85 valid stock-only plan does not enter interpolation");
    plan=original;
    auto missing=[](const Ref&,PerformanceReferenceIssue* p){p->reason="missing-row";return false;};
    expect(!ValidatePerformanceReferences(plan,read,missing,&issue),"alpha85 database lookup failure rejects nonnull stale cached reference");
    auto fault=[](unsigned,PerformanceReferenceArray*,PerformanceReferenceIssue* p){p->reason="exception";return false;};
    expect(!ValidatePerformanceReferences(plan,fault,resolve,&issue)&&std::string(issue.reason)=="exception",
        "alpha85 read exception result fails closed");
    expect(plan.chosen==original.chosen&&plan.maximum==original.maximum,"alpha85 no career levels changed by validation");

    // JESKO-style no-op endpoints: use the native effective plan before
    // allocating a car, never weaken post-spawn exact verification.
    arrays=validArrays;plan=original;plan.chosen=plan.maximum;
    arrays[2].refs[1]=arrays[2].refs[0];unsigned mask=0;
    expect(ValidatePerformanceReferences(plan,read,resolve,&issue,&mask)&&mask==4,
        "hotfix identical valid chassis endpoints at maximum produce no-op candidate");
    expect(NormalizeNoOpPerformance(&plan,mask,2,0)&&plan.chosen[2]==0&&plan.maximum[2]==3&&plan.chosen[4]==4,
        "hotfix only no-op chassis changes while maximum and other upgrades remain intact");
    plan.chosen[2]=2;
    expect(ValidatePerformanceReferences(plan,read,resolve,&issue,&mask)&&mask==0,
        "hotfix partial interpolation is not normalized");
    plan.chosen[2]=3;arrays[2].count=3;arrays[2].refs[2]=arrays[2].refs[1];
    expect(ValidatePerformanceReferences(plan,read,resolve,&issue,&mask)&&mask==0,
        "hotfix three-entry endpoints are not normalized");
    arrays=validArrays;arrays[2].refs[1]=arrays[2].refs[0];arrays[6].refs[1]=arrays[6].refs[0];
    expect(ValidatePerformanceReferences(plan,read,resolve,&issue,&mask)&&mask==4,
        "hotfix mandatory NOS is never normalized to zero");
    expect(!NormalizeNoOpPerformance(&plan,mask,2,1)&&plan.chosen[2]==3,
        "hotfix non-stock native level is not normalized");
    arrays[4].refs[1].mCollectionKey=0x3A41650A;
    expect(!ValidatePerformanceReferences(plan,read,resolve,&issue,&mask)&&mask==0&&plan.chosen[2]==3,
        "hotfix invalid later reference discards all no-op candidates atomically");
    arrays=validArrays;plan=original;

    auto cache=std::make_unique<InvalidPerformanceModels>();
    expect(cache->Reject(42,1)&&!cache->Reject(42,1)&&cache->Contains(42,1)&&!cache->Contains(43,1),
        "alpha85 repeated rejected key is logged once and unrelated models unaffected");
    expect(!cache->Contains(42,2)&&cache->count==0,"alpha85 content generation change clears exclusions");
    for(unsigned k=1;k<=kCatalogCollectionLimit;++k)cache->Reject(k,2);
    expect(cache->Contains(99,2)&&cache->Contains(99999,2)&&!cache->Reject(99999,2),
        "alpha85 bounded full rejection cache fails closed without eviction retry storm");
    const auto savedInvalid=std::make_unique<InvalidPerformanceModels>(g_invalidPerformanceModels);
    const auto savedGeneration=g_catalogGeneration;
    const auto savedCatalog=g_catalog;const auto savedPalette=g_palette,savedBag=g_vehicleBag;
    const auto savedRetry=g_performanceRetry;
    const auto savedExpanded=g_vehicleVarietyExpanded;
    g_invalidPerformanceModels={};g_catalogGeneration=123;g_performanceRetry={};
    const InstalledVehicle cars[]={{"VALID",1,100},{"INVALID",2,101}};
    g_catalog.clear();for(unsigned i=0;i<2;++i)g_catalog.push_back({&cars[i],int(i),{1,1,1}});
    g_palette={0,1};g_vehicleBag={0,1};g_invalidPerformanceModels.Reject(2,123);
    // Cached invalid branch executes before native surface/profile access.
    expect(ReadCareerPerformancePlan(2,&plan)==PerformanceEligibility::InvalidReference,
        "alpha85 cached invalid model never reenters native performance reader");
    unsigned badReads=0;
    auto eligible=[&](unsigned key,CareerPerformancePlan* out){
        if(key==2){++badReads;return ReadCareerPerformancePlan(key,out);}
        *out=original;return PerformanceEligibility::Ready;};
    for(bool expanded:{false,true}) {
        g_vehicleVarietyExpanded=expanded;
        for(unsigned i=0;i<20;++i){const auto* car=SelectPreparedSpawnVehicle(&plan,eligible,100);
            expect(car&&car->installed->key==1,"alpha85 invalid candidate filtered from shuffle bag across both model budgets");}
    }
    expect(badReads==40&&g_invalidPerformanceModels.count==1,"alpha85 retries retain one rejection without native allocation");
    g_invalidPerformanceModels=*savedInvalid;g_catalogGeneration=savedGeneration;
    g_catalog=savedCatalog;g_palette=savedPalette;g_vehicleBag=savedBag;
    g_performanceRetry=savedRetry;g_vehicleVarietyExpanded=savedExpanded;ResetSpawnEligibility();
}
