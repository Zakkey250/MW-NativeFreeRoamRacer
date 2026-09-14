// frontend collections are present in menus, but unloaded in free roam.
// Keep value copies, never an Attrib Instance, layout or collection reference.
struct FrontendMetadata { std::uint32_t key=0; int cost=0; bool customizable=false; };
struct FrontendMetadataCache {
    std::uintptr_t database=0,klass=0;
    std::uint32_t expected=0,next=0,visitedCount=0;
    bool loaded=false,finished=false,faulted=false,ready=false;
    std::array<std::uint32_t,kCatalogCollectionLimit> visited{};
    std::vector<FrontendMetadata> values,pending;
} g_frontendCache;
bool RestoreSavedFrontend();
void InvalidateSavedCalibration();

bool ReadFrontendSource(std::uintptr_t* database,std::uintptr_t* cls,unsigned* count) noexcept {
    __try {
        if(!SafeRead(reinterpret_cast<void*>(Address(0x90DCBC)),database)||!*database) return false;
        auto* source=reinterpret_cast<CatalogClass*(__thiscall*)(void*,unsigned)>(Address(0x455BC0))(
            reinterpret_cast<void*>(*database),CatalogHash("frontend"));
        if(!source) return false;
        *cls=reinterpret_cast<std::uintptr_t>(source);
        *count=reinterpret_cast<unsigned(__thiscall*)(void*)>(Address(0x453FC0))(source);
        return *count<=kCatalogCollectionLimit;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

bool CopyFrontendMetadata(std::uint32_t key,CatalogRow* row) noexcept {
    if(!g_frontendCache.ready) return false;
    const auto& entries=g_frontendCache.values;
    const auto found=std::lower_bound(entries.begin(),entries.end(),key,
        [](const FrontendMetadata& item,unsigned value){return item.key<value;});
    if(found==entries.end()||found->key!=key) return false;
    row->cost=found->cost;row->customizable=found->customizable;return true;
}

void CaptureFrontendCatalog() {
    std::uintptr_t database=0,cls=0;unsigned count=0;
    auto& cache=g_frontendCache;
    if(!ReadFrontendSource(&database,&cls,&count)) {
        if(cache.database) {cache={};ResetDynamicCatalog();}
        return;
    }
    if(cache.database!=database||cache.klass!=cls) {
        cache={};cache.database=database;cache.klass=cls;ResetDynamicCatalog();
        RestoreSavedFrontend();
    }
    if(!count) {
        if(cache.loaded) Log(LogLevel::Info,
            "CATALOG_FRONTEND_UNLOADED retained=%u complete=%u nativeReferencesHeld=0",
            static_cast<unsigned>(cache.values.size()),cache.ready);
        cache.loaded=false;return;
    }
    auto* source=reinterpret_cast<CatalogClass*>(cls);
    if(cache.ready&&cache.finished&&cache.expected==count) {cache.loaded=true;return;}
    if(!cache.loaded || cache.expected!=count) {
        InvalidateSavedCalibration();
        cache.loaded=true;cache.finished=false;cache.faulted=false;cache.ready=false;
        cache.expected=count;cache.visitedCount=0;cache.pending.clear();
        cache.pending.reserve(count);
        if(!CatalogStep(source,0,true,&cache.next)) {cache.faulted=true;return;}
        Log(LogLevel::Info,"CATALOG_FRONTEND_BEGIN collections=%u budgetRows=64 budgetMs=2",count);
    }
    if(cache.finished||cache.faulted) return;
    const auto started=GetTickCount64();
    // No physics estimates, files, car allocation or vault loading here.
    for(unsigned batch=0;cache.next&&batch<64;++batch) {
        if(batch&&GetTickCount64()-started>=2) return;
        const auto end=cache.visited.begin()+cache.visitedCount;
        if(cache.visitedCount>=kCatalogCollectionLimit||
           std::find(cache.visited.begin(),end,cache.next)!=end) {
            cache.faulted=true;Log(LogLevel::Error,"CATALOG_FRONTEND_ABORT reason=iterator-bound-or-cycle");return;
        }
        const auto key=cache.next;cache.visited[cache.visitedCount++]=key;
        CatalogRow row{};
        CatalogCollection* collection=nullptr;
        // FindCollection is wrapped by the same guarded reader used by rows.
        if(FindFrontendCollection(CatalogHash("frontend"),key,&collection) &&
           ReadCatalogFrontend(key,collection,&row))
            cache.pending.push_back({key,row.cost,row.customizable});
        if(!CatalogStep(source,key,false,&cache.next)) {
            cache.faulted=true;Log(LogLevel::Error,"CATALOG_FRONTEND_ABORT reason=iterator-invalid");return;
        }
    }
    if(cache.next) return;
    std::sort(cache.pending.begin(),cache.pending.end(),
        [](const FrontendMetadata& a,const FrontendMetadata& b){return a.key<b.key;});
    cache.values.swap(cache.pending);cache.pending.clear();
    cache.finished=true;cache.ready=!cache.values.empty();
    ResetDynamicCatalog();
    Log(LogLevel::Info,"CATALOG_FRONTEND_READY accepted=%u source=%u valueCopiesOnly=1",
        static_cast<unsigned>(cache.values.size()),cache.visitedCount);
}
