    {
        const auto oldBase=g_base;auto oldRacers=g_racers;g_racers.clear();g_battle={};
        auto* arena=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x400000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        expect(arena!=nullptr,"alpha33 private relocated native map fixture allocated");
        if(arena) {
            g_base=reinterpret_cast<std::uintptr_t>(arena)-(0x0057A000-kPreferredBase);
            auto** mapTable=reinterpret_cast<void**>(Address(kIVehicleVtable));
            mapTable[kSlotVehicleKey]=reinterpret_cast<void*>(&FakeKey);
            mapTable[kSlotDriverClass]=reinterpret_cast<void*>(&FakeDriver);
            mapTable[kSlotSimable]=reinterpret_cast<void*>(&FakeSimable);
            const unsigned char activeBytes[]={0x8B,0x91,0xAC,0,0,0,0x33,0xC0,0x85,0xD2,0x0F,0x95,0xC0,0xC3};
            auto* nativeActive=reinterpret_cast<void*>(Address(0x00688200));
            std::memcpy(nativeActive,activeBytes,sizeof(activeBytes));mapTable[34]=nativeActive;
            // stdcall wrapper, ESI preserved. [ESI] supplies the vehicle exactly
            // as in Minimap::UpdateRacers; map CALL is at the relocated real VA.
            const unsigned char callerBytes[]={0x56,0x8B,0x74,0x24,0x08,0x90,0x90,0x90,0x90,
                0x8B,0x0E,0x8B,0x11,0xFF,0x92,0x88,0,0,0,0x84,0xC0,0x5E,0xC2,0x04,0};
            auto* entry=reinterpret_cast<void*>(Address(0x0057A157));
            std::memcpy(entry,callerBytes,sizeof(callerBytes));
            FlushInstructionCache(GetCurrentProcess(),arena,0x400000);
            using MapCaller=int(__stdcall*)(void**);
            std::array<unsigned char,0x100> car{};
            put(car,0,reinterpret_cast<std::uintptr_t>(mapTable));put(car,4,0x1234u);put(car,8,unsigned(kDriverRacer));
            put(car,12,reinterpret_cast<std::uintptr_t>(&simable));put(car,0xAC,1u);
            void* listEntry=car.data();const auto carBefore=car;
            ManagedRacer owned{};owned.pointer=car.data();owned.simable=&simable;owned.vehicleKey=0x1234;g_racers.push_back(owned);
            battle::Sample s{};s.player.identity={1,11,111};s.rival.identity={2,22,222};
            s.player.forward=s.rival.forward={0,0,1};s.rival.position.z=10;
            g_battle.model.Start(s);g_battle.rival=s.rival.identity;g_encounterAdapterThread=GetCurrentThreadId();
            expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==1,"alpha33 original indirect map CALL returns active vehicle");
            // Arms Assist can check before OR after the map hook is installed.
            expect(std::memcmp(nativeActive,activeBytes,12)==0,"alpha33 Arms Assist first-12-byte guard passes before map install");
            auto* site=reinterpret_cast<void*>(Address(0x0057A164));
            const bool patched=PatchEncounterMapCall(site,reinterpret_cast<void*>(&EncounterMapActiveHook));
            expect(patched,"alpha33 map-only six-byte call substitution installs");
            expect(std::memcmp(nativeActive,activeBytes,sizeof(activeBytes))==0,"alpha33 shared IsActive remains byte-identical including Arms Assist guard after install");
            if(patched) {
                auto* bytes=static_cast<unsigned char*>(site);std::uint32_t rel=0;std::memcpy(&rel,bytes+2,4);
                const auto target=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(site)+6)+rel;
                expect(bytes[0]==0x90&&bytes[1]==0xE8&&target==reinterpret_cast<std::uintptr_t>(&EncounterMapActiveHook),"alpha33 NOP/CALL retains native return address 0057A16A");
                const auto count=g_encounterMapSuppressed.load();
                expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==0&&g_encounterMapSuppressed==count+1,"alpha33 actual map wrapper hides bystander exactly once");
                expect(car==carBefore&&reinterpret_cast<EncounterMapActiveFn>(nativeActive)(car.data())==1,"alpha33 Arms Assist direct active query still sees live car and no vehicle mutation");
                g_battle.rival={reinterpret_cast<std::uintptr_t>(car.data()),reinterpret_cast<std::uintptr_t>(&simable),0x1234};
                expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==1,"alpha33 opponent native arrow preserved through actual wrapper");
                g_battle.rival=s.rival.identity;put(car,4,0x9999u);
                expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==1,"alpha33 recycled identity preserves original map return");put(car,4,0x1234u);
                g_encounterAdapterThread=0;
                expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==1,"alpha33 foreign thread bypasses battle filter");g_encounterAdapterThread=GetCurrentThreadId();
                put(car,0xAC,0u);expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==0,"alpha33 originally inactive remains inactive");put(car,0xAC,1u);
                g_battle={};expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==1,"alpha33 battle end restores other native arrows");
                // A different vtable method still runs; do not hardcode IsActive.
                const unsigned char alternate[]={0xB8,0x37,0,0,0,0xC3};
                std::memcpy(arena+0x200,alternate,sizeof(alternate));mapTable[34]=arena+0x200;FlushInstructionCache(GetCurrentProcess(),arena+0x200,6);
                expect(reinterpret_cast<MapCaller>(entry)(&listEntry)==0x37,"alpha33 dynamic virtual implementation result preserved");
                expect(!PatchEncounterMapCall(site,reinterpret_cast<void*>(&EncounterMapActiveHook)),"alpha33 already-owned call site refused without overwriting it");
                expect(std::memcmp(arena+0x157,callerBytes,13)==0&&std::memcmp(arena+0x16A,callerBytes+19,6)==0,"alpha33 both neighboring instruction sequences preserved");
            }
            unsigned char badSite[6]={0xE9,0,0,0,0,0};const auto badBefore=std::array<unsigned char,6>{0xE9,0,0,0,0,0};
            expect(!PatchEncounterMapCall(badSite,reinterpret_cast<void*>(&EncounterMapActiveHook))&&std::memcmp(badSite,badBefore.data(),6)==0,"alpha33 third-party call-site modification fails closed");
            VirtualFree(arena,0,MEM_RELEASE);
        }
        g_base=oldBase;g_racers=oldRacers;g_battle={};
    }
