    {
        // Execute the production native enumeration path against relocated
        // x86 ABI fixtures, not a running game or development catalog.
        const auto previousBase=g_base;
        auto* arena=VirtualAlloc(nullptr,0x600000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        expect(arena!=nullptr,"alpha47 allocates isolated native catalog harness");
        if(arena) {
            g_base=reinterpret_cast<std::uintptr_t>(arena);g_cf={};
            const auto jump=[](unsigned address,auto fn) {
                auto* dst=reinterpret_cast<unsigned char*>(Address(address));dst[0]=0xE9;
                const auto relative=static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(fn)-reinterpret_cast<std::uintptr_t>(dst)-5);
                std::memcpy(dst+1,&relative,4);
            };
            jump(0x454640,&FixtureCatalogHash);jump(0x455BC0,&FixtureCatalogClass);
            jump(0x453FC0,&FixtureCatalogCount);jump(0x456B00,&FixtureCatalogFirst);
            jump(0x456B20,&FixtureCatalogNext);jump(0x455960,&FixtureCatalogCollection);
            jump(0x454190,&FixtureCatalogData);jump(0x455FD0,&FixtureCatalogFind);
            jump(0x4E4EA0,&FixtureCatalogConstruct);jump(0x45A430,&FixtureCatalogDestroy);
            jump(0x67C570,&FixtureCatalogEstimate);
            jump(0x51E1A0,&FixtureFrontendConstruct);
            FlushInstructionCache(GetCurrentProcess(),arena,0x600000);
            for(unsigned i=0;i<5;++i) {
                g_cf.rows[i].mKey=10*(i+1);g_cf.rows[i].mClass=reinterpret_cast<CatalogClass*>(g_cf.klass.data());
                if(i)g_cf.rows[i].mParent=&g_cf.rows[0];
            }
            std::array<unsigned char,3*0xD0> types{};
            const char* names[]={"TRAFFIC","CUSTOM77","A3"};
            for(unsigned i=0;i<3;++i) {
                std::memcpy(types.data()+i*0xD0,names[i],std::strlen(names[i])+1);
                types[i*0xD0+0x94]=i?0:1;types[i*0xD0+0x95]=0xFF;
            }
            const auto typePointer=reinterpret_cast<std::uintptr_t>(types.data());
            const auto db=reinterpret_cast<std::uintptr_t>(g_cf.klass.data());const int typeCount=3;
            std::memcpy(reinterpret_cast<void*>(Address(0x90DCBC)),&db,4);
            std::memcpy(reinterpret_cast<void*>(Address(0x9B09D8)),&typePointer,4);
            std::memcpy(reinterpret_cast<void*>(Address(0x9B1334)),&typeCount,4);
            ResetDynamicCatalog();g_catalogDatabase=0;g_catalogClass=0;g_catalogTypeTable=0;
            g_frontendCache={};CaptureFrontendCatalog();
            expect(g_frontendCache.ready&&g_frontendCache.values.size()==1&&g_cf.estimates==0,
                "alpha49 menu calibration copies frontend values without physics estimates");
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalog.size()==2&&g_catalogCursor==5,"alpha47 production traversal excludes traffic/templates and deduplicates aliases");
            expect(g_cf.constructed==g_cf.destroyed&&g_cf.estimates==3,"alpha47 every native metadata reference released including excluded traffic");
            expect(BuildVehiclePalette(0xABCDEF,"CUSTOM77"),"alpha47 unrelated add-on collection key remains selectable");
            expect(g_catalog[0].installed->cost==23456&&g_catalog[0].installed->customizable,
                "alpha48 reads actual frontend layout without generic named-field access");
            g_cf.reportedCount=2;ResetDynamicCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalogCursor==5&&g_catalog.size()==2,
                "alpha48 collection snapshot undercount does not abort valid enumeration");
            g_cf.reportedCount=20;ResetDynamicCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalogCursor==5,"alpha48 native zero key completes scan even below reported count");
            g_cf.reportedCount=5;g_cf.longCycle=true;ResetDynamicCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFaulted&&!g_catalogFinished,"alpha48 non-adjacent iterator cycle detected");
            g_cf.longCycle=false;ResetDynamicCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            CatalogRow invalid{};g_cf.ref.mClassKey=99;
            expect(!ReadCatalogRow(reinterpret_cast<CatalogClass*>(g_cf.klass.data()),30,&invalid),"alpha47 foreign frontend class rejected");g_cf.ref.mClassKey=2;
            g_cf.missingFrontendLayout=true;
            expect(!ReadCatalogRow(reinterpret_cast<CatalogClass*>(g_cf.klass.data()),30,&invalid)&&g_cf.constructed==g_cf.destroyed,
                "alpha48 missing frontend layout releases reference and skips record");g_cf.missingFrontendLayout=false;
            g_cf.cost=-1;
            expect(!ReadCatalogRow(reinterpret_cast<CatalogClass*>(g_cf.klass.data()),30,&invalid)&&g_cf.constructed==g_cf.destroyed,
                "alpha48 negative native frontend cost rejected with balanced references");g_cf.cost=23456;
            g_cf.customizable=3;
            expect(!ReadCatalogRow(reinterpret_cast<CatalogClass*>(g_cf.klass.data()),30,&invalid)&&g_cf.constructed==g_cf.destroyed,
                "alpha48 malformed customization flag rejected with balanced references");g_cf.customizable=1;
            g_cf.failEstimate=true;
            expect(!ReadCatalogRow(reinterpret_cast<CatalogClass*>(g_cf.klass.data()),30,&invalid)&&g_cf.constructed==g_cf.destroyed,
                "alpha47 failed native estimate releases reference and skips vehicle");g_cf.failEstimate=false;
            g_cf.iteratorCycle=true;ResetDynamicCatalog();
            AdvanceVehicleCatalog();
            expect(g_catalogFaulted&&!g_catalogFinished,"alpha47 repeated iterator key fails closed without loop");
            g_cf.iteratorCycle=false;
            const std::uintptr_t nullDb=0;std::memcpy(reinterpret_cast<void*>(Address(0x90DCBC)),&nullDb,4);
            AdvanceVehicleCatalog();
            expect(g_catalog.empty()&&g_palette.empty()&&!g_catalogStarted,"alpha47 unavailable database drops stale selection");
            std::memcpy(reinterpret_cast<void*>(Address(0x90DCBC)),&db,4);
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalog.size()==2,"alpha47 database restoration rebuilds population catalog");
            g_cf.frontendCount=0;CaptureFrontendCatalog();ResetDynamicCatalog();
            const auto beforeUnloadReads=g_cf.constructed;
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalog.size()==2&&g_frontendCache.ready&&!g_frontendCache.loaded&&
                g_catalog[0].installed->cost==23456&&g_catalog[0].installed->customizable,
                "alpha49 actual zero-frontend free-roam state still builds priced playable catalog");
            expect(g_cf.constructed-beforeUnloadReads==7&&g_cf.constructed==g_cf.destroyed,
                "alpha49 unloaded frontend uses value copies with no native frontend references");
            g_cf.frontendCount=1;g_cf.cost=45678;g_cf.customizable=0;CaptureFrontendCatalog();
            expect(g_frontendCache.values[0].cost==23456,
                "alpha49 unchanged source reuses completed calibration across safehouse returns");
            g_frontendCache.ready=false;CaptureFrontendCatalog();
            // Explicit invalidation models a changed source identity/count.
            g_frontendCache.loaded=false;CaptureFrontendCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalog.size()==2&&g_catalog[0].installed->cost==45678&&
                !g_catalog[0].installed->customizable,
                "alpha49 safehouse return recalibrates changed metadata and invalidates old palette");
            g_cf.frontendCount=0;CaptureFrontendCatalog();g_frontendCache={};ResetDynamicCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(!g_catalogStarted&&!g_catalogFinished&&g_catalog.empty(),
                "alpha49 missing calibration waits instead of latching an empty finished catalog");
            g_cf.frontendCount=1;CaptureFrontendCatalog();
            for(unsigned i=0;i<6;++i) AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalog.size()==2,
                "alpha49 calibration becoming available recovers generation without game restart");
            const std::array<unsigned char,32> identity{1,2,3};
            auto encoded=EncodeCalibration(identity);
            std::vector<CatalogDiskFrontend> decodedFrontend;
            std::vector<CatalogDiskVehicle> decodedVehicles;unsigned decodedSource=0;
            expect(DecodeCalibration(encoded,identity,&decodedFrontend,&decodedVehicles,&decodedSource)&&
                decodedFrontend.size()==1&&decodedVehicles.size()==2&&decodedSource==1,
                "alpha49 persistent cache round trip retains actual metadata and performance");
            auto wrongIdentity=identity;wrongIdentity[0]^=1;
            expect(!DecodeCalibration(encoded,wrongIdentity,&decodedFrontend,&decodedVehicles,&decodedSource),
                "alpha49 changed source fingerprint invalidates disk calibration");
            auto damaged=encoded;damaged.back()^=1;
            expect(!DecodeCalibration(damaged,identity,&decodedFrontend,&decodedVehicles,&decodedSource),
                "alpha49 cache corruption rejected by payload SHA256");
            damaged=encoded;damaged.pop_back();
            expect(!DecodeCalibration(damaged,identity,&decodedFrontend,&decodedVehicles,&decodedSource),
                "alpha49 truncated cache rejected");
            damaged=encoded;damaged.push_back(0);
            expect(!DecodeCalibration(damaged,identity,&decodedFrontend,&decodedVehicles,&decodedSource),
                "alpha49 trailing cache data rejected");
            damaged=encoded;damaged[8]=2;
            expect(!DecodeCalibration(damaged,identity,&decodedFrontend,&decodedVehicles,&decodedSource),
                "alpha49 incompatible cache version rejected");
            // A valid checksum must not make malformed row fields trusted.
            for(unsigned mutation=0;mutation<4;++mutation) {
                damaged=encoded;
                CatalogDiskHeader header{};std::memcpy(&header,damaged.data(),sizeof(header));
                const auto offset=sizeof(header)+header.frontendCount*sizeof(CatalogDiskFrontend);
                CatalogDiskVehicle corrupt{};std::memcpy(&corrupt,damaged.data()+offset,sizeof(corrupt));
                if(mutation==0) corrupt.model.fill('X');
                if(mutation==1) corrupt.customizable=2;
                if(mutation==2) corrupt.performance[1]=NAN;
                if(mutation==3) corrupt.cost=-1;
                std::memcpy(damaged.data()+offset,&corrupt,sizeof(corrupt));
                HashCalibrationBytes(damaged.data()+sizeof(header),damaged.size()-sizeof(header),&header.payloadHash);
                std::memcpy(damaged.data(),&header,sizeof(header));
                expect(!DecodeCalibration(damaged,identity,&decodedFrontend,&decodedVehicles,&decodedSource),
                    "alpha49 valid-checksum malformed cache row rejected");
            }
            // Exercise actual file hashing + worker save only on owned temp files.
            wchar_t tempDirectory[MAX_PATH]{},sourceFile[MAX_PATH]{},cacheFile[MAX_PATH]{};
            const bool temporary=GetTempPathW(MAX_PATH,tempDirectory)&&
                GetTempFileNameW(tempDirectory,L"nfc",0,sourceFile)&&GetTempFileNameW(tempDirectory,L"nfc",0,cacheFile);
            expect(temporary,"alpha49 allocates isolated disk-cache test files");
            if(temporary) {
                const std::vector<std::wstring> sources{sourceFile};std::array<unsigned char,32> sourceIdentity{};
                expect(CalibrationIdentity(sources,&sourceIdentity),"alpha49 source fingerprint reads actual file bytes");
                const auto fileBytes=EncodeCalibration(sourceIdentity);
                WriteCalibrationCache(new CalibrationSaveJob{fileBytes,sources,cacheFile,sourceIdentity});
                HANDLE cached=CreateFileW(cacheFile,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
                std::vector<unsigned char> readBack(fileBytes.size());DWORD countRead=0;
                const bool roundTrip=cached!=INVALID_HANDLE_VALUE&&
                    ReadFile(cached,readBack.data(),static_cast<DWORD>(readBack.size()),&countRead,nullptr)&&countRead==readBack.size()&&readBack==fileBytes;
                if(cached!=INVALID_HANDLE_VALUE) CloseHandle(cached);
                expect(roundTrip,"alpha49 worker atomically saves exact calibration payload");
                HANDLE changed=CreateFileW(sourceFile,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
                DWORD countWritten=0;const char marker='x';
                const bool wrote=changed!=INVALID_HANDLE_VALUE&&WriteFile(changed,&marker,1,&countWritten,nullptr)&&countWritten==1;
                if(changed!=INVALID_HANDLE_VALUE) CloseHandle(changed);
                std::array<unsigned char,32> changedIdentity{};
                expect(wrote&&CalibrationIdentity(sources,&changedIdentity)&&sourceIdentity!=changedIdentity,
                    "alpha49 real source modification changes calibration identity");
                std::string beforeFile,afterFile;ComputeSha256(cacheFile,&beforeFile);
                WriteCalibrationCache(new CalibrationSaveJob{std::vector<unsigned char>{1,2},sources,cacheFile,sourceIdentity});
                ComputeSha256(cacheFile,&afterFile);
                expect(beforeFile==afterFile,"alpha49 source change during save leaves previous cache untouched");
                expect(DeleteFileW(sourceFile)&&DeleteFileW(cacheFile),"alpha49 owned temporary files cleaned up");
            }
            g_savedFrontend=decodedFrontend;g_savedVehicles=decodedVehicles;g_savedFrontendSourceCount=decodedSource;
            ResetDynamicCatalog();g_frontendCache={};CaptureFrontendCatalog();
            const auto cachedEstimates=g_cf.estimates;AdvanceVehicleCatalog();
            expect(g_catalogFinished&&g_catalog.size()==2&&g_cf.estimates==cachedEstimates,
                "alpha49 restart reuses saved price and performance without native re-estimation");
            ResetDynamicCatalog();g_savedVehicles[0].model.fill(0);strcpy_s(g_savedVehicles[0].model.data(),32,"REMOVED");
            expect(!RestoreSavedCatalog()&&g_savedVehicles.empty()&&g_catalog.empty(),
                "alpha49 live model mismatch discards disk vehicles before publishing partial catalog");
            InvalidateSavedCalibration();g_frontendCache.loaded=false;g_frontendCache.ready=false;CaptureFrontendCatalog();
            g_cf.frontendCount=0;CaptureFrontendCatalog();g_cf.frontendCount=1;g_cf.frontendCycle=true;g_frontendCache.ready=false;CaptureFrontendCatalog();
            expect(g_frontendCache.faulted&&!g_frontendCache.finished,
                "alpha49 menu iterator cycle fails closed within bounded work");
            ResetDynamicCatalog();AdvanceVehicleCatalog();
            expect(!g_catalogStarted,"alpha49 incomplete recalibration never uses stale previous metadata");
            g_cf.frontendCycle=false;
            std::memcpy(reinterpret_cast<void*>(Address(0x90DCBC)),&nullDb,4);CaptureFrontendCatalog();
            expect(!g_frontendCache.ready&&g_frontendCache.values.empty(),
                "alpha49 missing database clears copied values instead of leaking across sessions");
            g_frontendCache={};
            ResetDynamicCatalog();g_catalogDatabase=0;g_catalogClass=0;g_catalogTypeTable=0;
            g_base=previousBase;VirtualFree(arena,0,MEM_RELEASE);
        }
        ResetDynamicCatalog();
        CatalogRow row{};strcpy_s(row.model.data(),row.model.size(),"ADDON_X");
        row.key=900;row.canonicalKey=500;row.cost=12345;row.type=76;row.customizable=true;
        row.performance={0,1,2};
        expect(AddCatalogRow(row)&&g_catalog.size()==1,"alpha47 arbitrary add-on key and type accepted without compiled catalog");
        const auto* stable=g_catalog[0].installed;
        row.model.fill('X');
        expect(!AddCatalogRow(row)&&g_catalog.size()==1,"alpha47 unterminated model rejected");
        for(const char* bad:{"../BAD","C:\\BAD","BAD NAME","","BAD/NAME"}) {
            row.model.fill(0);strcpy_s(row.model.data(),row.model.size(),bad);
            expect(!AddCatalogRow(row),"alpha47 invalid/path-like model rejected");
        }
        strcpy_s(row.model.data(),row.model.size(),"ADDON_X");
        row.cost=-1;expect(!AddCatalogRow(row),"alpha47 invalid price rejected");row.cost=999;
        row.type=-1;expect(!AddCatalogRow(row),"alpha47 non-playable type rejected");row.type=76;
        row.performance={NAN,1,2};expect(!AddCatalogRow(row),"alpha47 invalid native ratings rejected");row.performance={0,1,2};
        row.key=0;expect(!AddCatalogRow(row),"alpha47 zero collection rejected");
        row.key=901;expect(!AddCatalogRow(row)&&g_catalog.size()==1,"alpha47 alias cannot duplicate model");
        row.key=100;expect(AddCatalogRow(row)&&stable->key==100,"alpha47 noncanonical aliases select deterministic lowest key");
        row.key=500;expect(AddCatalogRow(row)&&stable->key==500,"alpha47 canonical key preferred when present");
        row.key=1;expect(!AddCatalogRow(row)&&stable->key==500,"alpha47 canonical row cannot be replaced by alias");
        bool capacityAccepted=true;
        for(unsigned i=1;i<kCatalogModelLimit;++i) {
            sprintf_s(row.model.data(),row.model.size(),"ADDON_%u",i);row.key=1000+i;
            capacityAccepted=AddCatalogRow(row)&&capacityAccepted;
        }
        expect(capacityAccepted,"alpha47 accepts 1024 model records independent of development roster");
        expect(stable==g_catalog[0].installed && std::strcmp(stable->model,"ADDON_X")==0 && stable->cost==999,
            "alpha47 deque retains model strings and metadata across all insertions");
        strcpy_s(row.model.data(),row.model.size(),"OVER_LIMIT");
        expect(!AddCatalogRow(row)&&g_catalog.size()==kCatalogModelLimit,"alpha47 bounded catalog prevents unbounded allocation");
        RankVehicles(g_catalog);
        expect(BuildVehiclePalette(0xDEADBEEF,"ADDON_X")&&g_palette.size()==g_settings.vehicleVariety,
            "alpha47 native player model matches palette even when instance key is unrelated");
        g_catalogFinished=true;g_catalogStarted=true;g_catalogFaulted=true;
        ResetDynamicCatalog();
        expect(g_ownedCatalog.empty()&&g_catalog.empty()&&g_palette.empty()&&g_vehicleBag.empty()&&
            !g_catalogFinished&&!g_catalogStarted&&!g_catalogFaulted&&!g_catalogCursor&&!g_palettePlayerKey,
            "alpha47 source reset removes stale palette and iterator state");
        const Settings defaults{};
        expect(defaults.maximumRacers==6 && defaults.populationRadiusMeters==600 && defaults.markerRadiusMeters==200 &&
            defaults.freeRoamAudioSlots==4 && defaults.spawnIntervalSeconds==5 && defaults.anchorMaximumMeters==700 &&
            defaults.encounterPowerScales==std::array<float,3>{1.5f,1.75f,2},"alpha47 missing INI matches accepted current settings");
    }
