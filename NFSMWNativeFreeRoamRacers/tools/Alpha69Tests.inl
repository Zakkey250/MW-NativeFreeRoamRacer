    {
        auto** simTable=reinterpret_cast<void**>(Address(kISimableVtable));
        const auto oldKill=simTable[2];simTable[2]=reinterpret_cast<void*>(Address(kKillSimable));
        struct LifecycleSimable {void** table;std::uint8_t padding[24];std::uint8_t pending;};
        static_assert(offsetof(LifecycleSimable,pending)==0x1C);
        LifecycleSimable humanSim{simTable,{},0},anchorSim{simTable,{},0},retiredSim{simTable,{},0};
        FakeVehicle human{table,1,kDriverHuman,&humanSim},anchor{table,2,kDriverTraffic,&anchorSim},retired{table,3,kDriverRacer,&retiredSim};
        auto** dataSlot=reinterpret_cast<const void* const**>(Address(kVehicleListData));
        auto* countSlot=reinterpret_cast<unsigned*>(Address(kVehicleListCount));
        auto* capacitySlot=reinterpret_cast<unsigned*>(Address(kVehicleListData+4));
        const auto oldCapacity=*capacitySlot;*capacitySlot=3;
        const auto oldData=*dataSlot;const auto oldCount=*countSlot;const bool oldFaulted=g_faulted;
        const void* list[3]={&human,&anchor,&retired};*dataSlot=list;*countSlot=3;
        ConstructionPointers protection{};unsigned count=0;const void* blocked=nullptr;
        for(unsigned cycle=0;cycle<100;++cycle){
            *countSlot=3;retiredSim.pending=1;g_faulted=false;
            expect(CaptureConstructionLiveSet(protection,count)&&count==3,"alpha69 killed but still listed reproduces alpha68 failure");
            expect(ValidateConstructionLifecycles(protection,count,&blocked)==ConstructionLifecycle::RetirementPending&&blocked==&retired,"alpha69 pending native garbage blocks construction");
            expect(ConstructionContains(protection,count,&retired)&&!g_faulted,"alpha69 pending death neither excluded from protection nor globally latched");
            *countSlot=2;
            expect(CaptureConstructionLiveSet(protection,count)&&ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::Ready,"alpha69 later native removal permits refill");
            expect(!ConstructionContains(protection,count,&retired),"alpha69 actually retired address can now be reused");
            *countSlot=3;retiredSim.pending=0;
            expect(CaptureConstructionLiveSet(protection,count)&&ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::Ready&&ConstructionContains(protection,count,&retired),"alpha69 native reuse between polls is live and protected");
        }
        retiredSim.pending=2;
        expect(ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::Invalid,"alpha69 invalid death byte fails closed");
        retiredSim.pending=0;retired.simable=nullptr;
        expect(ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::Invalid,"alpha69 missing simable fails closed");
        retired.simable=&retiredSim;retiredSim.table=nullptr;
        expect(ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::Invalid,"alpha69 wrong simable table fails closed");
        retiredSim.table=simTable;simTable[2]=nullptr;
        expect(ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::Invalid,"alpha69 changed kill slot fails closed");
        simTable[2]=reinterpret_cast<void*>(Address(kKillSimable));
        anchorSim.pending=1;
        expect(ValidateConstructionLifecycles(protection,count)==ConstructionLifecycle::RetirementPending,"alpha69 non-managed native retirement also defers construction");
        expect(ValidateConstructionLifecycles(protection,0)==ConstructionLifecycle::Invalid&&ValidateConstructionLifecycles(protection,unsigned(protection.size()+1))==ConstructionLifecycle::Invalid,"alpha69 bounded lifecycle scan rejects invalid counts");
        simTable[2]=oldKill;*dataSlot=oldData;*countSlot=oldCount;*capacitySlot=oldCapacity;g_faulted=oldFaulted;
    }
