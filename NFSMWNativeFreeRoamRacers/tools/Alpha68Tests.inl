    {
        auto** dataSlot=reinterpret_cast<const void* const**>(Address(kVehicleListData));
        auto* countSlot=reinterpret_cast<unsigned*>(Address(kVehicleListCount));
        auto* capacitySlot=reinterpret_cast<unsigned*>(Address(kVehicleListData+4));
        const auto oldCapacity=*capacitySlot;*capacitySlot=3;
        const auto oldData=*dataSlot;const auto oldCount=*countSlot;
        const auto oldScratch=g_vehicleScratch;
        FakeVehicle human{table,1,kDriverHuman,&simable},anchor{table,2,kDriverTraffic,&simable},retired{table,3,kDriverRacer,&simable};
        const void* live[3]={&human,&anchor,nullptr};
        *dataSlot=live;*countSlot=2;
        g_vehicleScratch.clear();
        for(auto* object:{&human,&anchor,&retired}){VehicleSnapshot s{};s.pointer=object;g_vehicleScratch.push_back(s);}
        ConstructionPointers protection{};unsigned count=999;
        for(unsigned i=0;i<100;++i){
            expect(CaptureConstructionLiveSet(protection,count)&&count==2,"alpha68 native membership excludes retired frame entry");
            expect(!ConstructionContains(protection,count,&retired),"alpha68 reuse of retired address is not live replacement");
            expect(ConstructionContains(protection,count,&human)&&ConstructionContains(protection,count,&anchor),"alpha68 true live replacement still trips same guard");
        }
        live[2]=&retired;*countSlot=3;
        expect(CaptureConstructionLiveSet(protection,count)&&ConstructionContains(protection,count,&retired),"alpha68 same address when genuinely live remains protected");
        g_vehicleScratch.pop_back();
        expect(CaptureConstructionLiveSet(protection,count)&&count==3,"alpha68 newly live object missing from frame protected too");
        live[2]=&anchor;
        expect(!CaptureConstructionLiveSet(protection,count)&&count==0,"alpha68 duplicate native membership fails closed");
        live[2]=nullptr;expect(!CaptureConstructionLiveSet(protection,count)&&count==0,"alpha68 null native member fails closed");
        FakeVehicle invalid{nullptr,4,1,&simable};live[2]=&invalid;
        expect(!CaptureConstructionLiveSet(protection,count)&&count==0,"alpha68 invalid live vtable fails closed");
        *countSlot=4;
        expect(!CaptureConstructionLiveSet(protection,count)&&count==0,"alpha68 count overflow rejected before reading table");
        *countSlot=0;expect(!CaptureConstructionLiveSet(protection,count),"alpha68 no player/world list cannot construct");
        *countSlot=2;*dataSlot=nullptr;expect(!CaptureConstructionLiveSet(protection,count),"alpha68 null native vector cannot construct");
        *dataSlot=oldData;*countSlot=oldCount;*capacitySlot=oldCapacity;g_vehicleScratch=oldScratch;
    }
