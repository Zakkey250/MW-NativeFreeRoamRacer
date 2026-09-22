    {
        using namespace nfsmw_population;
        for(unsigned limit:{1u,16u,20u,28u,32u,33u,36u,48u,64u,128u,256u,512u,65536u,0xFFFFFFFFu}){
            for(unsigned live:{0u,limit-1,limit}){
                Status s{sizeof(Status),requiredCapabilities,limit,live,limit-live,1};
                expect(PopulationDecision(Available,s,1)==(live==limit?PopulationAccess::Full:PopulationAccess::Ready),"alpha70 provider capacity not gated by release or configured maximum");
                if(live<limit){s.freeSlots=0;expect(PopulationDecision(Available,s,1)==PopulationAccess::Full,"alpha70 companion vector exhaustion respected");}
                s.capabilities=3;expect(PopulationDecision(Available,s,1)==PopulationAccess::Unsafe,"alpha70 large capacity never bypasses capabilities");
            }
        }
        Status s{sizeof(Status),requiredCapabilities,36,12,24,0};
        expect(PopulationDecision(Available,s,0)==PopulationAccess::Ready,"alpha70 exact expanded LB36 idle report accepted");
        s.live=37;expect(PopulationDecision(Available,s,0)==PopulationAccess::Unsafe,"alpha70 live overflow rejected");
        s.live=12;s.freeSlots=25;expect(PopulationDecision(Available,s,0)==PopulationAccess::Unsafe,"alpha70 vacancy overflow rejected");
        s.limit=0;expect(PopulationDecision(Available,s,0)==PopulationAccess::Unsafe,"alpha70 zero limit rejected");
        expect(PopulationDecision(Unavailable,s,0)==PopulationAccess::Legacy,"alpha70 no LB retains legacy branch");
        expect(PopulationDecision(Busy,s,0)==PopulationAccess::Unsafe&&PopulationDecision(Unsafe,s,0)==PopulationAccess::Unsafe,"alpha70 busy or unsafe provider cannot fall back around safeguards");

        auto** dataSlot=reinterpret_cast<const void* const**>(Address(kVehicleListData));
        auto* countSlot=reinterpret_cast<unsigned*>(Address(kVehicleListCount));
        auto* capacitySlot=reinterpret_cast<unsigned*>(Address(kVehicleListData+4));
        const auto oldData=*dataSlot;const auto oldCount=*countSlot,oldCapacity=*capacitySlot;
        ConstructionPointers protection;unsigned count=0;
        for(unsigned n:{1u,28u,32u,36u,48u,64u,256u,257u,512u}){
            std::vector<FakeVehicle> cars(n);std::vector<const void*> native(n);
            for(unsigned i=0;i<n;++i){cars[i]={table,i+1,kDriverTraffic,&simable};native[i]=&cars[i];}
            *dataSlot=native.data();*countSlot=n;*capacitySlot=n;
            expect(CaptureConstructionLiveSet(protection,count)&&count==n,"alpha70 native protection list grows beyond old 32 and 256 boundaries");
            expect(BeginConstructionProtection(protection.data(),count),"alpha70 dynamic construction protection begins");
            expect(IsExternalConstructionProtectedVehicle(&cars.front())&&IsExternalConstructionProtectedVehicle(&cars.back()),"alpha70 first and last live members remain protected");
            expect(!BeginConstructionProtection(protection.data(),count),"alpha70 overlapping construction rejected");
            EndConstructionProtection("alpha70-test");
            expect(!IsExternalConstructionProtectedVehicle(&cars.back()),"alpha70 end clears dynamic protection");
            *capacitySlot=n-1;
            expect(!CaptureConstructionLiveSet(protection,count)&&count==0,"alpha70 native count cannot exceed actual capacity");
        }
        const void* invalid[1]={nullptr};
        std::vector<FakeVehicle> concurrentCars(512);
        std::vector<const void*> concurrentSet(512);
        for(unsigned i=0;i<512;++i){concurrentCars[i]={table,i+1,kDriverTraffic,&simable};concurrentSet[i]=&concurrentCars[i];}
        std::atomic<bool> reading{true},falsePositive{false};
        std::thread reader([&]{while(reading.load()){
            IsExternalConstructionProtectedVehicle(concurrentSet.front());
            if(IsExternalConstructionProtectedVehicle(&invalid))falsePositive.store(true);
        }});
        bool concurrentOk=true;
        for(unsigned i=0;i<100;++i){
            const unsigned n=i%2?36:512;
            const bool began=BeginConstructionProtection(concurrentSet.data(),n);
            concurrentOk&=began;
            if(began){concurrentOk&=IsExternalConstructionProtectedVehicle(concurrentSet[n-1]);EndConstructionProtection("alpha70-concurrency");}
        }
        reading.store(false);reader.join();
        expect(concurrentOk&&!falsePositive.load(),"alpha70 protected readers coexist with dynamic publication and reuse");
        expect(!BeginConstructionProtection(invalid,1)&&!g_externalConstructionProtectionClaimed.load(),"alpha70 invalid protection frees its claim");
        expect(!ReadablePointerArray(reinterpret_cast<void*>(0xFFFFFFFCu),2),"alpha70 pointer byte span overflow rejected");
        auto* guarded=VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(guarded){
            DWORD old=0;VirtualProtect(static_cast<std::uint8_t*>(guarded)+4096,4096,PAGE_NOACCESS,&old);
            expect(ReadablePointerArray(guarded,1024)&&!ReadablePointerArray(guarded,1025),"alpha70 committed unreadable boundary rejected without dereference");
            VirtualFree(guarded,0,MEM_RELEASE);
        }else expect(false,"alpha70 guard-page fixture allocation");
        *dataSlot=oldData;*countSlot=oldCount;*capacitySlot=oldCapacity;
    }
