namespace native_freeroam { namespace {
struct CatalogFixture {
    std::array<std::uint8_t,16> klass{};
    std::array<std::uint8_t,16> frontendClass{};
    std::array<CatalogCollection,5> rows{};
    CatalogCollection frontend{};
    NFSPluginSDK::MW05::Attrib::RefSpec ref{2,700,nullptr};
    std::array<std::uint8_t,64> layout{};
    std::array<std::uint8_t,0x5C> frontendLayout{};
    int cost=23456;std::uint8_t customizable=1;
    unsigned index=0,constructed=0,destroyed=0,estimates=0,reportedCount=5;
    bool failEstimate=false,iteratorCycle=false,longCycle=false,missingFrontendLayout=false;
    unsigned frontendCount=1;
    bool frontendCycle=false;
} g_cf;
unsigned __cdecl FixtureCatalogHash(const char* text) {
    if(!std::strcmp(text,"pvehicle")) return 1;
    if(!std::strcmp(text,"frontend")) return 2;
    if(!std::strcmp(text,"racers")) return 10;
    if(!std::strcmp(text,"Cost")) return 4;
    if(!std::strcmp(text,"IsCustomizable")) return 5;
    if(!std::strcmp(text,"a3")) return 50;
    return 999; // CUSTOM77 model is deliberately unrelated to row key 30.
}
CatalogClass* __fastcall FixtureCatalogClass(void*,void*,unsigned key) {
    return key==1?reinterpret_cast<CatalogClass*>(g_cf.klass.data()):
        key==2?reinterpret_cast<CatalogClass*>(g_cf.frontendClass.data()):nullptr;
}
unsigned __fastcall FixtureCatalogCount(void* cls,void*) {return cls==g_cf.frontendClass.data()?g_cf.frontendCount:g_cf.reportedCount;}
unsigned __fastcall FixtureCatalogFirst(void* cls,void*) {return cls==g_cf.frontendClass.data()?700:10;}
unsigned __fastcall FixtureCatalogNext(void* cls,void*,unsigned key) {
    if(cls==g_cf.frontendClass.data()) return g_cf.frontendCycle?key:0;
    return g_cf.iteratorCycle?key:(key>=50?(g_cf.longCycle?10:0):key+10);
}
CatalogCollection* __fastcall FixtureCatalogCollection(void*,void*,unsigned key) {
    if(key<10||key>50||key%10) return nullptr;
    g_cf.index=key/10-1;return &g_cf.rows[g_cf.index];
}
void* __fastcall FixtureCatalogData(void* row,void*,unsigned field,int) {
    // Regression: the frontend fixed-layout fields are NOT supplied by this
    // generic accessor. alpha47's happy-path fixture incorrectly exposed them.
    if(row!=&g_cf.frontend && field==2) return &g_cf.ref;
    return nullptr;
}
CatalogCollection* __cdecl FixtureCatalogFind(unsigned cls,unsigned key) {
    return cls==2&&key==700&&g_cf.frontendCount?&g_cf.frontend:nullptr;
}
void __fastcall FixtureCatalogConstruct(void* instance,void*,unsigned key,int,int) {
    ++g_cf.constructed;
    auto* value=static_cast<NFSPluginSDK::MW05::Attrib::Instance*>(instance);
    value->mCollection=&g_cf.rows[key/10-1];value->mLayoutPtr=g_cf.layout.data();
    const char* model=key==20?"TRAFFIC":key==50?"A3":"CUSTOM77";
    std::memcpy(g_cf.layout.data()+0x1C,&model,4);
}
void __fastcall FixtureCatalogDestroy(void*,void*) {++g_cf.destroyed;}
void __fastcall FixtureFrontendConstruct(void* instance,void*,unsigned,int,int) {
    ++g_cf.constructed;
    auto* value=static_cast<NFSPluginSDK::MW05::Attrib::Instance*>(instance);
    value->mCollection=&g_cf.frontend;
    value->mLayoutPtr=g_cf.missingFrontendLayout?nullptr:g_cf.frontendLayout.data();
    std::memcpy(g_cf.frontendLayout.data()+0x4C,&g_cf.cost,4);
    g_cf.frontendLayout[0x58]=g_cf.customizable;
}
bool __cdecl FixtureCatalogEstimate(void*,float* result) {
    ++g_cf.estimates;result[0]=0;result[1]=1;result[2]=2;return !g_cf.failEstimate;
}
} }
