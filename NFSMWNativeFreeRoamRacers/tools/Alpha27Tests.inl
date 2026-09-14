    {
        VehicleSnapshot p{},r{};p.pointer=reinterpret_cast<void*>(1);r.pointer=reinterpret_cast<void*>(2);
        p.driverClass=kDriverHuman;r.driverClass=kDriverRacer;p.heading=r.heading={0,0,1};
        r.position={0,5.2f,10.41f};
        expect(IsEncounterFollowing(p,r),"alpha27 observed near uphill geometry stays eligible even stopped");
        p.heading={0.95f,0,0.30f};
        expect(IsEncounterFollowing(p,r),"alpha27 curved-road heading of 0.30 now qualifies without grace");
        r.position={0,6,1};
        expect(!IsEncounterFollowing(p,r),"alpha27 close stacked roads retain vertical rejection");
        r.position={0,12.01f,30};
        expect(!IsEncounterFollowing(p,r),"alpha27 slope tolerance cannot exceed 12 meters");
        r.position={0,5.2f,10.41f};p.heading={0.98f,0,0.15f};
        battle::Sample s{};s.player.identity={1,11,111};s.rival.identity={2,22,222};
        s.player.forward={p.heading.x,p.heading.y,p.heading.z};s.rival.forward={0,0,1};
        s.rival.position={r.position.x,r.position.y,r.position.z};
        battle::Model m;
        expect(m.Start(s).event==battle::Event::Started,"alpha27 battle start accepts the prompt slope and positive-heading grace envelope");
        s.player.forward={0,0,-1};m.Reset();
        expect(m.Start(s).event==battle::Event::None,"alpha27 battle start still rejects oncoming geometry");
    }
    {
        std::array<unsigned char,0x2C8> road{};
        std::array<unsigned char,0x16*2> segments{};
        std::array<unsigned char,0x20*2> nodes{};
        std::array<unsigned char,0x40> profiles{};
        auto set=[&](auto& buffer,std::size_t at,auto value) {std::memcpy(buffer.data()+at,&value,sizeof(value));};
        set(road,0x50,std::uint8_t{1});set(road,0x8E,std::int16_t{1});
        set(road,0x90,0.4f);set(road,0x2C1,std::int8_t{1});set(road,0x2C4,0.5f);
        set(segments,0x16,std::uint16_t{0});set(segments,0x18,std::uint16_t{1});
        const std::uintptr_t addresses[]={0x009B38C0,0x009B38BC,0x009B38B8};
        std::uintptr_t savedGlobals[3]{};
        for(unsigned i=0;i<3;++i) std::memcpy(&savedGlobals[i],reinterpret_cast<void*>(Address(addresses[i])),4);
        auto setPointer=[&](unsigned i,void* pointer) {std::memcpy(reinterpret_cast<void*>(Address(addresses[i])),&pointer,4);};
        setPointer(0,segments.data());setPointer(1,nodes.data());setPointer(2,profiles.data());
        AnchorRoadSeed seed;
        expect(CaptureAnchorRoadSeed(road.data(),seed),"anchor scalar snapshot validates both native node links");
        road.fill(0xEE);
        expect(ValidateAnchorRoadNetwork(seed)&&seed.At<std::int16_t>(0x8E)==1&&seed.At<float>(0x90)==0.4f,
            "anchor snapshot survives original traffic-nav poison or recycling");
        AnchorRoadSeed invalid;
        expect(!CaptureAnchorRoadSeed(road.data(),invalid),"poisoned EEEEEEEE road never reaches native reset");
        auto bad=seed;set(bad.bytes,0x8E,std::int16_t{-1});
        expect(!ValidateAnchorRoadNetwork(bad),"negative road segment rejected before table indexing");
        bad=seed;set(bad.bytes,0x8C,std::uint8_t{2});
        expect(!ValidateAnchorRoadNetwork(bad),"invalid road endpoint index rejected");
        bad=seed;set(bad.bytes,0x90,std::numeric_limits<float>::quiet_NaN());
        expect(!ValidateAnchorRoadNetwork(bad),"nonfinite road parameter rejected");
        setPointer(1,reinterpret_cast<void*>(0xEEEEEEEE));
        expect(!ValidateAnchorRoadNetwork(seed),"unreadable native node table rejected without native call");
        for(unsigned i=0;i<3;++i) std::memcpy(reinterpret_cast<void*>(Address(addresses[i])),&savedGlobals[i],4);
    }
