    {
        using namespace nfsmw_population;
        Status status;
        expect(QueryPopulation(1,&status)==Unavailable,"alpha64 LB absent is optional");
        expect(PopulationDecision(Unavailable,status,1)==PopulationAccess::Legacy,"alpha64 unavailable provider keeps legacy ceiling");
        expect(PopulationDecision(Unsafe,status,1)==PopulationAccess::Unsafe,"alpha64 unsafe provider fails closed");
        expect(PopulationDecision(Busy,status,1)==PopulationAccess::Unsafe,"alpha64 foreign lease fails closed");
        for(unsigned limit:{20u,24u,28u,32u})for(unsigned live=0;live<=limit;++live){
            status={sizeof(Status),requiredCapabilities,limit,live,limit-live,3};
            expect(PopulationDecision(Available,status,3)==(live==limit?PopulationAccess::Full:PopulationAccess::Ready),"alpha64 capacity boundary");
            status.freeSlots=limit-live+1;
            expect(PopulationDecision(Available,status,3)==PopulationAccess::Unsafe,"alpha64 impossible vacancy rejected");
        }
        status={sizeof(Status),requiredCapabilities,28,28,0,3};
        status.capabilities=3;expect(PopulationDecision(Available,status,3)==PopulationAccess::Unsafe,"alpha64 missing traffic gate rejected");
        status.capabilities=7;status.reserved=2;expect(PopulationDecision(Available,status,3)==PopulationAccess::Unsafe,"alpha64 missing reservation rejected");
        status.reserved=3;status.limit=33;expect(PopulationDecision(Available,status,3)==PopulationAccess::Full,"alpha70 no consumer-specific capacity ceiling");
        VehicleSnapshot player{},anchor{},traffic{};
        player.pointer=reinterpret_cast<void*>(1);anchor.pointer=reinterpret_cast<void*>(2);
        traffic.pointer=reinterpret_cast<void*>(3);traffic.driverClass=kDriverTraffic;
        traffic.position.x=1000;traffic.offScreenTime=3;
        expect(ReclaimableTraffic(traffic,player,anchor),"alpha64 distant offscreen traffic eligible");
        for(unsigned driver:{0u,2u,3u,4u}){traffic.driverClass=driver;expect(!ReclaimableTraffic(traffic,player,anchor),"alpha64 nontraffic cannot be reclaimed");}
        traffic.driverClass=kDriverTraffic;traffic.onScreenTime=1;
        expect(!ReclaimableTraffic(traffic,player,anchor),"alpha64 visible traffic protected");
        traffic.onScreenTime=0;traffic.offScreenTime=2.99f;
        expect(!ReclaimableTraffic(traffic,player,anchor),"alpha64 recent screen presence protected");
        traffic.offScreenTime=3;traffic.position.x=100;
        expect(!ReclaimableTraffic(traffic,player,anchor),"alpha64 nearby traffic protected");
        traffic.position.x=1000;traffic.pointer=anchor.pointer;
        expect(!ReclaimableTraffic(traffic,player,anchor),"alpha64 spawn anchor protected");
        traffic.pointer=player.pointer;
        expect(!ReclaimableTraffic(traffic,player,anchor),"alpha64 player protected");
    }
