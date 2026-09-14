    {
        auto snapshot=[](Vec3 raw,void* identity,unsigned driver) {
            VehicleSnapshot s{};s.pointer=identity;s.driverClass=driver;s.vehicleKey=unsigned(reinterpret_cast<std::uintptr_t>(identity));
            NFSPluginSDK::MW05::UMath::Vector3 sdk{};
            std::memcpy(&sdk,&raw,12);s.position=FromUMath(sdk);
            // Bogus historical heading: forward LOCAL velocity, identical even
            // when cars face different world directions. Must never be consumed.
            raw={0,0,20};std::memcpy(&sdk,&raw,12);s.heading=FromUMath(sdk);s.speed=30;
            return s;
        };
        auto legacyPlayer=snapshot({206.62537f,107.22319f,1794.3097f},reinterpret_cast<void*>(1),kDriverHuman);
        auto legacyRival=snapshot({195.28821f,107.80299f,1815.0497f},reinterpret_cast<void*>(2),kDriverRacer);
        const auto p=EncounterPhysicalPosition(legacyPlayer.position),r=EncounterPhysicalPosition(legacyRival.position);
        expect(std::abs(p.y-r.y)<.6f&&std::abs(legacyPlayer.position.y-legacyRival.position.y)>11,
            "alpha30 real dump has 0.58m height difference not the legacy 11.34m horizontal delta");
        expect(std::abs(Distance(p,r)-Distance(legacyPlayer.position,legacyRival.position))<.001f,
            "alpha30 axis correction preserves physical distance and does not rescale meters");
        for(unsigned angle=0;angle<24;++angle) {
            const float theta=float(angle)*3.14159265359f/12;
            const Vec3 heading{std::sin(theta),0,std::cos(theta)};
            auto reader=[&](void*,Vec3& out){out=heading;return true;};
            Vec3 origin{200,107,1800};
            auto world=[&](float distance){return Vec3{origin.x+heading.x*distance,origin.y,origin.z+heading.z*distance};};
            auto a=snapshot(world(0),reinterpret_cast<void*>(1),kDriverHuman);
            auto b=snapshot(world(20),reinterpret_cast<void*>(2),kDriverRacer);
            VehicleSnapshot ca{},cb{};
            const auto savedA=a;
            MakeEncounterGeometry(a,ca,reader);MakeEncounterGeometry(b,cb,reader);
            expect(IsEncounterFollowing(ca,cb)&&std::abs(ca.position.y-cb.position.y)<.01f,
                "alpha30 rear-follow eligibility invariant under world yaw with real SDK byte layout");
            battle::Sample sample{};sample.player.identity={1,11,1};sample.rival.identity={2,22,2};
            sample.player.position={ca.position.x,ca.position.y,ca.position.z};sample.rival.position={cb.position.x,cb.position.y,cb.position.z};
            sample.player.forward=sample.rival.forward={heading.x,heading.y,heading.z};sample.player.speed=sample.rival.speed=30;
            battle::Model m;m.Start(sample);
            auto pp=world(23);sample.player.position={pp.x,pp.y,pp.z};m.Step(sample,.05f);m.Step(sample,.05f);
            expect(m.leader()==battle::Leader::Player&&m.roleTrusted(),"alpha30 all compass directions allow a real world pass");
            for(unsigned i=1;i<=31;++i){pp=world(23+float(i)*10);sample.player.position={pp.x,pp.y,pp.z};m.Step(sample,.1f);}
            expect(m.phase()==battle::Phase::Won,"alpha30 passing then separating wins in every world direction");
            expect(a.position.x==savedA.position.x&&a.position.y==savedA.position.y&&a.position.z==savedA.position.z&&a.heading.x==savedA.heading.x,
                "alpha30 encounter conversion never changes population or marker snapshot");
        }
        auto noHeading=[](void*,Vec3&){return false;};VehicleSnapshot output{};
        expect(!MakeEncounterGeometry(legacyPlayer,output,noHeading),"alpha30 start rejects missing native heading instead of reading local velocity");
        expect(MakeEncounterGeometry(legacyPlayer,output,noHeading,false),"alpha30 active collision recovery retains valid position without heading");
        battle::Sample s{};s.player.identity={1,11,1};s.rival.identity={2,22,2};
        s.player.forward=s.rival.forward={0,0,1};s.rival.position.z=10;
        battle::Model m;m.Start(s);s.player.position.x=15;m.Step(s,.1f);
        for(unsigned i=0;i<32;++i){s.rival.position.z+=10;m.Step(s,.1f);}
        expect(m.phase()==battle::Phase::Active&&!m.roleTrusted(),"alpha30 never-valid progress cannot turn the initial rival role into a loss");
    }
