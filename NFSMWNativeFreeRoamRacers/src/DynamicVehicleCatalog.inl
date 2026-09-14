// Enumerate the current game's VLT. Never infer a pvehicle key from MODEL:
// add-on authors can use unrelated collection names and frontend references.
struct CatalogRow {
    std::array<char,32> model{};
    const char* stage="collection";
    std::uint32_t key=0,canonicalKey=0;
    int cost=0,type=-1;
    bool customizable=false;
    std::array<float,3> performance{};
};
struct OwnedCatalogRow {
    std::array<char,32> model{};
    InstalledVehicle vehicle{};
    std::uint32_t canonicalKey=0;
};
std::deque<OwnedCatalogRow> g_ownedCatalog;
std::uintptr_t g_catalogDatabase=0,g_catalogClass=0,g_catalogTypeTable=0;
std::uint32_t g_catalogNext=0,g_catalogExpected=0;
bool g_catalogStarted=false,g_catalogFaulted=false;
constexpr unsigned kCatalogCollectionLimit=8192,kCatalogModelLimit=1024;
std::array<std::uint32_t,kCatalogCollectionLimit> g_catalogVisited{};

bool CatalogKeyVisited(std::uint32_t key) noexcept {
    const auto end=g_catalogVisited.begin()+std::min<std::size_t>(g_catalogCursor,kCatalogCollectionLimit);
    return std::find(g_catalogVisited.begin(),end,key)!=end;
}

bool ValidCatalogModel(const std::array<char,32>& model) noexcept {
    if(!model[0]) return false;
    for(char ch:model) {
        if(!ch) return true;
        if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-')) return false;
    }
    return false;
}

bool PreferCatalogKey(std::uint32_t proposed,std::uint32_t existing,std::uint32_t canonical) noexcept {
    if(proposed==existing) return false;
    if(existing==canonical) return false;
    return proposed==canonical || proposed<existing;
}

void ResetDynamicCatalog() {
    // Only called on the management thread. Racer records own a string copy.
    g_catalog.clear();g_palette.clear();g_vehicleBag.clear();g_palettePlayerKey=0;
    g_ownedCatalog.clear();g_catalogCursor=0;g_catalogFinished=false;
    g_catalogNext=0;g_catalogExpected=0;g_catalogStarted=false;g_catalogFaulted=false;
}

using CatalogClass=NFSPluginSDK::MW05::Attrib::Class;
using CatalogCollection=NFSPluginSDK::MW05::Attrib::Collection;

std::uint32_t CatalogHash(const char* name) {
    return reinterpret_cast<std::uint32_t(__cdecl*)(const char*)>(Address(0x454640))(name);
}
void* CatalogData(CatalogCollection* collection,const char* name) {
    return reinterpret_cast<void*(__thiscall*)(void*,std::uint32_t,int)>(Address(0x454190))(collection,CatalogHash(name),0);
}

bool ReadCatalogSource(std::uintptr_t* database,std::uintptr_t* carClass,
                       std::uintptr_t* typeTable,std::uint32_t* count) noexcept {
    __try {
        if(!SafeRead(reinterpret_cast<void*>(Address(0x90DCBC)),database)||!*database ||
           !SafeRead(reinterpret_cast<void*>(Address(0x9B09D8)),typeTable)||!*typeTable) return false;
        auto* cls=reinterpret_cast<CatalogClass*(__thiscall*)(void*,std::uint32_t)>(Address(0x455BC0))(
            reinterpret_cast<void*>(*database),CatalogHash("pvehicle"));
        if(!cls) return false;
        *carClass=reinterpret_cast<std::uintptr_t>(cls);
        *count=reinterpret_cast<unsigned(__thiscall*)(void*)>(Address(0x453FC0))(cls);
        return *count>0 && *count<=kCatalogCollectionLimit;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

bool CatalogStep(CatalogClass* cls,std::uint32_t current,bool first,std::uint32_t* next) noexcept {
    __try {
        *next=first?reinterpret_cast<unsigned(__thiscall*)(void*)>(Address(0x456B00))(cls):
            reinterpret_cast<unsigned(__thiscall*)(void*,unsigned)>(Address(0x456B20))(cls,current);
        return first || !*next || *next!=current;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

bool ReadCatalogFrontend(std::uint32_t key,CatalogCollection* expected,CatalogRow* row) noexcept {
    // Match FECarRecord::GetCost (00581730) and its customization flag reader
    // (0058164F). These are fields in Gen::frontend's fixed 0x5C-byte layout,
    // not guaranteed to be exposed through Collection::GetData by name.
    NFSPluginSDK::MW05::Attrib::Instance instance{};
    bool initialized=false,success=false;
    __try {
        __try {
            reinterpret_cast<void(__thiscall*)(void*,unsigned,int,int)>(Address(0x51E1A0))(&instance,key,0,0);
            initialized=true;
            row->stage="frontend-layout";
            if(instance.mCollection==expected && instance.mLayoutPtr) {
                const auto* layout=static_cast<const unsigned char*>(instance.mLayoutPtr);
                row->stage="frontend-price";
                if(SafeRead(layout+0x4C,&row->cost) && row->cost>=0) {
                    std::uint8_t customizable=0;
                    row->stage="frontend-customizable";
                    if(SafeRead(layout+0x58,&customizable) && customizable<=1) {
                        row->customizable=customizable!=0;success=true;
                    }
                }
            }
        } __finally {
            if(initialized) reinterpret_cast<void(__thiscall*)(void*)>(Address(0x45A430))(&instance);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    return success;
}

bool FindFrontendCollection(unsigned cls,unsigned key,CatalogCollection** result) noexcept {
    __try {
        *result=reinterpret_cast<CatalogCollection*(__cdecl*)(unsigned,unsigned)>(Address(0x455FD0))(cls,key);
        return *result!=nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

#include "FrontendCatalogCache.inl"

bool ReadCatalogRow(CatalogClass* cls,std::uint32_t key,CatalogRow* row) noexcept {
    using namespace NFSPluginSDK::MW05::Attrib;
    __try {
        auto* collection=reinterpret_cast<CatalogCollection*(__thiscall*)(void*,unsigned)>(Address(0x455960))(cls,key);
        if(!collection) return false;
        row->stage="racer-parent";
        // Only descendants of racers, not police, traffic, templates or props.
        auto* parent=collection->mParent;
        const auto racersKey=CatalogHash("racers");
        bool racer=false;
        for(unsigned depth=0;parent&&depth<32;++depth) {
            if(parent->mClass!=cls) return false;
            if(parent->mKey==racersKey) {racer=true;break;}
            parent=parent->mParent;
        }
        if(!racer) return false;
        row->stage="frontend-reference";
        RefSpec ref{};
        if(!SafeRead(CatalogData(collection,"frontend"),&ref) ||
           ref.mClassKey!=CatalogHash("frontend") || !ref.mCollectionKey) return false;
        CatalogCollection* frontend=nullptr;
        row->stage="frontend-lookup";
        if(FindFrontendCollection(ref.mClassKey,ref.mCollectionKey,&frontend)) {
            if(!ReadCatalogFrontend(ref.mCollectionKey,frontend,row)) return false;
        } else {
            row->stage="frontend-cache";
            if(!CopyFrontendMetadata(ref.mCollectionKey,row)) return false;
        }
        row->key=key;
        row->stage="model";
        if(!InspectVehicleAttributes(key,row->model.data(),nullptr)||!ValidCatalogModel(row->model)) return false;
        row->type=FindPlayableType(row->model.data());
        row->stage="playable-type";
        if(row->type<0) return false;
        char lower[32]{};
        for(unsigned i=0;i<31&&row->model[i];++i) {
            const char c=row->model[i];lower[i]=c>='A'&&c<='Z'?char(c+'a'-'A'):c;
        }
        row->canonicalKey=CatalogHash(lower);
        row->stage="native-performance";
        // A bounded native estimate, with its Instance reference released.
        return ValidStockPerformance(InspectVehicleAttributes(key,row->model.data(),row->performance.data()),row->performance);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

bool AddCatalogRow(const CatalogRow& row) {
    if(!ValidCatalogModel(row.model)||!row.key||row.cost<0||row.type<0||
       !ValidStockPerformance(true,row.performance)) return false;
    for(auto& owned:g_ownedCatalog) {
        if(_stricmp(owned.model.data(),row.model.data())!=0) continue;
        if(!PreferCatalogKey(row.key,owned.vehicle.key,row.canonicalKey)) return false;
        owned.vehicle.key=row.key;owned.vehicle.cost=row.cost;owned.vehicle.customizable=row.customizable;
        for(auto& ranked:g_catalog) if(ranked.installed==&owned.vehicle) {
            ranked.type=row.type;ranked.performance=row.performance;break;
        }
        return true;
    }
    if(g_ownedCatalog.size()>=kCatalogModelLimit) return false;
    g_ownedCatalog.emplace_back();auto& owned=g_ownedCatalog.back();
    owned.model=row.model;owned.canonicalKey=row.canonicalKey;
    owned.vehicle={owned.model.data(),row.key,row.cost,row.customizable};
    g_catalog.push_back({&owned.vehicle,row.type,row.performance});
    return true;
}

bool RestoreSavedCatalog();
void SaveCalibratedCatalog();

void AdvanceVehicleCatalog() {
    // Never finalize an empty catalog simply because fe_attrib was unloaded.
    // A complete menu snapshot is required; no stale/partial price ranking.
    if(!g_frontendCache.ready || (g_frontendCache.loaded&&!g_frontendCache.finished)) return;
    std::uintptr_t database=0,cls=0,table=0;std::uint32_t count=0;
    if(!ReadCatalogSource(&database,&cls,&table,&count)) {
        if(g_catalogStarted) ResetDynamicCatalog();
        g_catalogDatabase=0;g_catalogClass=0;g_catalogTypeTable=0;
        return;
    }
    if(database!=g_catalogDatabase||cls!=g_catalogClass||table!=g_catalogTypeTable) {
        ResetDynamicCatalog();g_catalogDatabase=database;g_catalogClass=cls;g_catalogTypeTable=table;
    }
    if(g_catalogFinished||g_catalogFaulted) return;
    if(!g_catalogStarted && RestoreSavedCatalog()) return;
    auto* source=reinterpret_cast<CatalogClass*>(cls);
    if(!g_catalogStarted) {
        g_catalogExpected=count;
        if(!CatalogStep(source,0,true,&g_catalogNext)) {g_catalogFaulted=true;return;}
        g_catalogStarted=true;
        Log(LogLevel::Info,"CATALOG_DYNAMIC_BEGIN collections=%u mode=loaded-vlt fixedAssetHashes=0",count);
    }
    // GetNumCollections is a snapshot, not an iterator end token: native
    // startup/traffic may add collections while this multi-frame scan runs.
    // Keep a hard resource bound AND detect longer cycles, but wait for key 0.
    if(g_catalogNext && g_catalogCursor<kCatalogCollectionLimit) {
        const auto key=g_catalogNext;CatalogRow row{};
        if(CatalogKeyVisited(key)) {
            g_catalogFaulted=true;Log(LogLevel::Error,"CATALOG_DYNAMIC_ABORT reason=iterator-cycle key=%08X",key);return;
        }
        g_catalogVisited[g_catalogCursor]=key;
        const bool valid=ReadCatalogRow(source,key,&row);
        if(valid && AddCatalogRow(row))
            Log(LogLevel::Info,"CATALOG_DYNAMIC_CANDIDATE model=%s key=%08X type=%d cost=%d customizable=%u",
                row.model.data(),key,row.type,row.cost,row.customizable);
        else if(!valid)
            Log(LogLevel::Info,"CATALOG_DYNAMIC_SKIP key=%08X stage=%s",key,row.stage);
        ++g_catalogCursor;
        if(!CatalogStep(source,key,false,&g_catalogNext)) {
            g_catalogFaulted=true;Log(LogLevel::Error,"CATALOG_DYNAMIC_ABORT reason=iterator-invalid");
        }
        return; // At most one collection/stock estimate per management tick.
    }
    if(g_catalogNext) {
        g_catalogFaulted=true;Log(LogLevel::Error,"CATALOG_DYNAMIC_ABORT reason=collection-limit");return;
    }
    RankVehicles(g_catalog);g_catalogFinished=true;
    Log(LogLevel::Info,"CATALOG_READY accepted=%u source=%u initialCount=%u finalCount=%u mode=loaded-vlt ranking=price50-performance50",
        static_cast<unsigned>(g_catalog.size()),static_cast<unsigned>(g_catalogCursor),g_catalogExpected,count);
    for(std::size_t i=0;i<g_catalog.size();++i)
        Log(LogLevel::Info,"CATALOG_RANK rank=%u model=%s score=%.5f",static_cast<unsigned>(i+1),g_catalog[i].installed->model,g_catalog[i].score);
    if(!g_catalog.empty()) SaveCalibratedCatalog();
}

#include "PersistentVehicleCatalog.inl"
