// Disposable, versioned value cache. Never serialize pointers or save/profile
// state. File hashing/loading runs on InitializePlugin before hooks are enabled;
// atomic cache writing uses a short-lived worker with a private snapshot.
struct CatalogDiskFrontend { unsigned key=0; int cost=0; unsigned customizable=0; };
struct CatalogDiskVehicle {
    std::array<char,32> model{};
    unsigned key=0,canonical=0;
    int cost=0;
    unsigned customizable=0;
    std::array<float,3> performance{};
};
struct CatalogDiskHeader {
    std::array<char,8> magic{'N','F','R','C','A','L','4','9'};
    unsigned version=1,frontendCount=0,vehicleCount=0,sourceFrontendCount=0;
    std::array<unsigned char,32> identity{},payloadHash{};
};
static_assert(sizeof(CatalogDiskFrontend)==12 && sizeof(CatalogDiskVehicle)==60 && sizeof(CatalogDiskHeader)==88);
std::vector<CatalogDiskFrontend> g_savedFrontend;
std::vector<CatalogDiskVehicle> g_savedVehicles;
unsigned g_savedFrontendSourceCount=0;
std::vector<std::wstring> g_calibrationSources;
std::wstring g_calibrationPath;
std::array<unsigned char,32> g_calibrationIdentity{};
bool g_calibrationPersistence=false,g_calibrationSaveQueued=false;

void InvalidateSavedCalibration() {
    g_savedFrontend.clear();g_savedVehicles.clear();g_savedFrontendSourceCount=0;
    g_calibrationSaveQueued=false;
}

bool HashCalibrationBytes(const void* data,std::size_t size,std::array<unsigned char,32>* result) {
    if(size>1024*1024) return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD length=32;
    const bool ok=CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
        CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)&&
        CryptHashData(hash,static_cast<const BYTE*>(data),static_cast<DWORD>(size),0)&&
        CryptGetHashParam(hash,HP_HASHVAL,result->data(),&length,0)&&length==32;
    if(hash) CryptDestroyHash(hash);
    if(provider) CryptReleaseContext(provider,0);
    return ok;
}

bool CalibrationIdentity(const std::vector<std::wstring>& paths,std::array<unsigned char,32>* identity) {
    std::string signatures="NFR-calibration-v1:";signatures+=kExpectedExecutableSha256;
    for(const auto& path:paths) {
        std::string hash;
        if(!ComputeSha256(path.c_str(),&hash)) return false;
        // Include ordered paths as well as bytes; a renamed/new override matters.
        signatures.append(reinterpret_cast<const char*>(path.data()),path.size()*sizeof(wchar_t));
        signatures+='\0';signatures+=hash;
    }
    return HashCalibrationBytes(signatures.data(),signatures.size(),identity);
}

bool DecodeCalibration(const std::vector<unsigned char>& bytes,const std::array<unsigned char,32>& identity,
                       std::vector<CatalogDiskFrontend>* frontend,std::vector<CatalogDiskVehicle>* vehicles,
                       unsigned* sourceCount) {
    if(bytes.size()<sizeof(CatalogDiskHeader)||bytes.size()>1024*1024) return false;
    CatalogDiskHeader header{};std::memcpy(&header,bytes.data(),sizeof(header));
    const CatalogDiskHeader defaults{};
    if(header.magic!=defaults.magic||header.version!=defaults.version||header.identity!=identity||
       !header.frontendCount||header.frontendCount>kCatalogCollectionLimit||
       !header.vehicleCount||header.vehicleCount>kCatalogModelLimit||
       header.sourceFrontendCount<header.frontendCount||header.sourceFrontendCount>kCatalogCollectionLimit) return false;
    const auto expected=sizeof(header)+header.frontendCount*sizeof(CatalogDiskFrontend)+header.vehicleCount*sizeof(CatalogDiskVehicle);
    if(bytes.size()!=expected) return false;
    std::array<unsigned char,32> digest{};
    if(!HashCalibrationBytes(bytes.data()+sizeof(header),bytes.size()-sizeof(header),&digest)||digest!=header.payloadHash) return false;
    std::vector<CatalogDiskFrontend> fe(header.frontendCount);
    std::vector<CatalogDiskVehicle> cars(header.vehicleCount);
    std::memcpy(fe.data(),bytes.data()+sizeof(header),fe.size()*sizeof(fe[0]));
    std::memcpy(cars.data(),bytes.data()+sizeof(header)+fe.size()*sizeof(fe[0]),cars.size()*sizeof(cars[0]));
    unsigned previous=0;
    for(const auto& item:fe) {
        if(!item.key||item.key<=previous||item.cost<0||item.customizable>1) return false;
        previous=item.key;
    }
    for(std::size_t i=0;i<cars.size();++i) {
        const auto& car=cars[i];
        if(!ValidCatalogModel(car.model)||!car.key||!car.canonical||car.cost<0||car.customizable>1||
           !ValidStockPerformance(true,car.performance)) return false;
        for(std::size_t j=0;j<i;++j)
            if(car.key==cars[j].key||_stricmp(car.model.data(),cars[j].model.data())==0) return false;
    }
    *frontend=std::move(fe);*vehicles=std::move(cars);*sourceCount=header.sourceFrontendCount;return true;
}

std::vector<unsigned char> EncodeCalibration(const std::array<unsigned char,32>& identity) {
    if(!g_frontendCache.ready||g_frontendCache.values.empty()||g_catalog.empty()) return {};
    CatalogDiskHeader header{};header.identity=identity;
    header.frontendCount=static_cast<unsigned>(g_frontendCache.values.size());
    header.vehicleCount=static_cast<unsigned>(g_catalog.size());
    header.sourceFrontendCount=g_frontendCache.expected;
    std::vector<unsigned char> bytes(sizeof(header)+header.frontendCount*sizeof(CatalogDiskFrontend)+header.vehicleCount*sizeof(CatalogDiskVehicle));
    auto* next=bytes.data()+sizeof(header);
    for(const auto& item:g_frontendCache.values) {
        const CatalogDiskFrontend row{item.key,item.cost,item.customizable?1u:0u};
        std::memcpy(next,&row,sizeof(row));next+=sizeof(row);
    }
    for(const auto& item:g_catalog) {
        CatalogDiskVehicle row{};strcpy_s(row.model.data(),row.model.size(),item.installed->model);
        row.key=item.installed->key;row.cost=item.installed->cost;row.customizable=item.installed->customizable?1u:0u;
        const auto found=std::find_if(g_ownedCatalog.begin(),g_ownedCatalog.end(),
            [&](const OwnedCatalogRow& owned){return &owned.vehicle==item.installed;});
        if(found==g_ownedCatalog.end()) return {};
        row.canonical=found->canonicalKey;row.performance=item.performance;
        std::memcpy(next,&row,sizeof(row));next+=sizeof(row);
    }
    if(!HashCalibrationBytes(bytes.data()+sizeof(header),bytes.size()-sizeof(header),&header.payloadHash)) return {};
    std::memcpy(bytes.data(),&header,sizeof(header));return bytes;
}

void PrepareCatalogPersistence() {
    wchar_t exe[MAX_PATH]{},module[MAX_PATH]{};HMODULE self=nullptr;
    if(!GetModuleFileNameW(nullptr,exe,MAX_PATH)||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&PrepareCatalogPersistence),&self)||!GetModuleFileNameW(self,module,MAX_PATH)) return;
    std::wstring root=exe;root.resize(root.find_last_of(L"\\/")+1);
    std::wstring scripts=module;scripts.resize(scripts.find_last_of(L"\\/")+1);
    // These contain the vehicle definitions, frontend layouts and CarTypeInfo.
    g_calibrationSources={root+L"GLOBAL\\attributes.bin",root+L"GLOBAL\\fe_attrib.bin"};
    for(const auto* relative:{L"GLOBAL\\GlobalB.lzc",L"GLOBAL\\GLOBALB.BUN",
        L"scripts\\NFSMWUnlimiter.asi",L"scripts\\NFSMWUnlimiter.ini",L"scripts\\NFSMWUnlimiterSettings.ini"}) {
        const auto path=root+relative;
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) g_calibrationSources.push_back(path);
    }
    // Per-car Unlimiter settings can change supported customization behavior.
    WIN32_FIND_DATAW data{};
    const auto unlimiterData=root+L"scripts\\UnlimiterData\\";
    HANDLE search=FindFirstFileW((unlimiterData+L"*.ini").c_str(),&data);
    if(search!=INVALID_HANDLE_VALUE) {
        do {if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) g_calibrationSources.push_back(unlimiterData+data.cFileName);}
        while(g_calibrationSources.size()<4096&&FindNextFileW(search,&data));
        FindClose(search);
    }
    std::sort(g_calibrationSources.begin(),g_calibrationSources.end());
    if(!CalibrationIdentity(g_calibrationSources,&g_calibrationIdentity)) {
        Log(LogLevel::Warning,"CALIBRATION_CACHE_DISABLED reason=source-read-failed runtimeCalibration=available");return;
    }
    const auto directory=scripts+L"NativeFreeRoamRacers";
    CreateDirectoryW(directory.c_str(),nullptr);
    g_calibrationPath=directory+L"\\VehicleCatalog.cache.bin";g_calibrationPersistence=true;
    HANDLE file=CreateFileW(g_calibrationPath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) {Log(LogLevel::Info,"CALIBRATION_CACHE_MISS reason=not-readable buildInMenu=1");return;}
    LARGE_INTEGER size{};std::vector<unsigned char> bytes;DWORD read=0;
    if(GetFileSizeEx(file,&size)&&size.QuadPart>=sizeof(CatalogDiskHeader)&&size.QuadPart<=1024*1024) {
        bytes.resize(static_cast<std::size_t>(size.QuadPart));
        if(!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)||read!=bytes.size()) bytes.clear();
    }
    CloseHandle(file);
    if(!DecodeCalibration(bytes,g_calibrationIdentity,&g_savedFrontend,&g_savedVehicles,&g_savedFrontendSourceCount)) {
        Log(LogLevel::Info,"CALIBRATION_CACHE_MISS reason=changed-data-or-invalid-cache buildInMenu=1");return;
    }
    Log(LogLevel::Info,"CALIBRATION_CACHE_LOADED frontend=%u vehicles=%u sourceSHA256Matched=1",
        static_cast<unsigned>(g_savedFrontend.size()),static_cast<unsigned>(g_savedVehicles.size()));
}

bool RestoreSavedFrontend() {
    if(g_savedFrontend.empty()) return false;
    for(const auto& row:g_savedFrontend) g_frontendCache.values.push_back({row.key,row.cost,row.customizable!=0});
    g_frontendCache.expected=g_savedFrontendSourceCount;
    g_frontendCache.ready=true;g_frontendCache.finished=true;return true;
}

bool RestoreSavedCatalog() {
    if(g_savedVehicles.empty()) return false;
    std::vector<CatalogRow> rows;rows.reserve(g_savedVehicles.size());
    for(const auto& saved:g_savedVehicles) {
        CatalogRow row{};row.key=saved.key;row.canonicalKey=saved.canonical;
        row.model=saved.model;row.cost=saved.cost;row.customizable=saved.customizable!=0;row.performance=saved.performance;
        char liveModel[32]{};row.type=FindPlayableType(row.model.data());
        if(row.type<0||!InspectVehicleAttributes(row.key,liveModel,nullptr)||_stricmp(liveModel,row.model.data())!=0) {
            g_savedVehicles.clear();g_calibrationSaveQueued=false;
            Log(LogLevel::Warning,"CALIBRATION_CACHE_REJECTED reason=live-vehicle-mismatch freshScan=1");return false;
        }
        rows.push_back(row);
    }
    for(const auto& row:rows) AddCatalogRow(row);
    RankVehicles(g_catalog);g_catalogFinished=true;
    Log(LogLevel::Info,"CATALOG_READY accepted=%u mode=persistent-calibration stockEstimates=0 liveModelValidation=1",
        static_cast<unsigned>(g_catalog.size()));return true;
}

struct CalibrationSaveJob {
    std::vector<unsigned char> bytes;
    std::vector<std::wstring> sources;
    std::wstring path;
    std::array<unsigned char,32> identity{};
};
DWORD WINAPI WriteCalibrationCache(void* context) noexcept {
    auto* job=static_cast<CalibrationSaveJob*>(context);
    try {
        std::array<unsigned char,32> identity{};
        if(CalibrationIdentity(job->sources,&identity)&&identity==job->identity) {
            const auto temporary=job->path+L".tmp";
            HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            DWORD written=0;bool ok=false;
            if(file!=INVALID_HANDLE_VALUE) {
                ok=WriteFile(file,job->bytes.data(),static_cast<DWORD>(job->bytes.size()),&written,nullptr)&&written==job->bytes.size();
                if(ok) ok=FlushFileBuffers(file)!=FALSE;
                CloseHandle(file);
            }
            if(ok) ok=MoveFileExW(temporary.c_str(),job->path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
            Log(ok?LogLevel::Info:LogLevel::Warning,ok?"CALIBRATION_CACHE_SAVED atomic=1":"CALIBRATION_CACHE_SAVE_FAILED runtimeRemainsAvailable=1");
        } else Log(LogLevel::Warning,"CALIBRATION_CACHE_SAVE_SKIPPED sourceChanged=1 restartRequired=1");
    } catch(...) {Log(LogLevel::Warning,"CALIBRATION_CACHE_SAVE_FAILED exception=1");}
    delete job;return 0;
}

void SaveCalibratedCatalog() {
    if(!g_calibrationPersistence||g_calibrationSaveQueued) return;
    auto bytes=EncodeCalibration(g_calibrationIdentity);if(bytes.empty()) return;
    // Reuse the successful calibration across future worlds in this process too.
    if(!DecodeCalibration(bytes,g_calibrationIdentity,&g_savedFrontend,&g_savedVehicles,&g_savedFrontendSourceCount)) return;
    auto* job=new CalibrationSaveJob{std::move(bytes),g_calibrationSources,g_calibrationPath,g_calibrationIdentity};
    const HANDLE thread=CreateThread(nullptr,0,WriteCalibrationCache,job,0,nullptr);
    if(thread) {CloseHandle(thread);g_calibrationSaveQueued=true;}
    else {delete job;Log(LogLevel::Warning,"CALIBRATION_CACHE_SAVE_FAILED worker=0");}
}
