    {
        encounter_custom::DirectionalTrail trail;
        expect(!trail.Select({},30,{0,0,1}).valid,"alpha53 empty directional trail has no command");
        for(unsigned i=0;i<=100;++i)trail.Record({0,0,float(i)*2},{0,0,1});
        auto aim=trail.Select({30,0,120},40,{0,0,1});
        expect(aim.valid&&trail.cursor()>50,"alpha53 30m lane error reacquires later forward segment without visiting old points");
        expect(aim.position.z>120&&aim.heading.z>=.5f,"alpha53 aim stays ahead instead of returning to old breadcrumb");
        const auto cursor=trail.cursor();
        aim=trail.Select({25,0,100},40,{0,0,1});
        expect(trail.cursor()>=cursor,"alpha53 incidental backward movement does not rewind history");
        for(float x:{-60.f,-20.f,0.f,20.f,60.f}) {
            trail.Reset();for(unsigned i=0;i<90;++i)trail.Record({0,0,float(i)*2},{0,0,1});
            aim=trail.Select({x,0,100},35,{0,0,1});
            expect(aim.valid&&aim.position.z>100&&aim.heading.z>=.499f,
                "alpha53 wide lateral error keeps bounded forward steering on either side");
        }
        trail.Reset();trail.Record({0,0,0},{0,0,1});trail.Record({0,0,10},{0,0,1});
        aim=trail.Select({0,0,50},50,{0,0,1});
        expect(aim.position.z>50&&aim.heading.z>.99f,"alpha53 passing entire recorded span continues its direction not U-turn");
        trail.Reset();for(unsigned i=0;i<30;++i)trail.Record({0,8,float(i)*2},{0,0,1});
        const auto bridge=trail.cursor();aim=trail.Select({0,0,40},30,{0,0,1});
        expect(trail.cursor()==bridge&&aim.position.y==0,"alpha53 elevated trail cannot pull car onto another height");
        trail.Reset();for(unsigned i=0;i<30;++i)trail.Record({0,0,float(i)*2},{0,0,1});
        aim=trail.Select({0,0,40},30,{0,0,-1});
        expect(trail.cursor()==1&&aim.heading.z<-.99f,"alpha53 opposing segment is not mistaken for same-direction route");
        trail.Reset();
        for(unsigned i=0;i<=10;++i)trail.Record({0,0,float(i)*4},{0,0,1});
        for(unsigned i=1;i<=10;++i)trail.Record({float(i)*4,0,40},{1,0,0});
        aim=trail.Select({0,0,28},40,{0,0,1});
        expect(aim.heading.x>.1f&&aim.heading.z>=.499f,"alpha53 gradual bend aim crosses former strict corner gate without reverse command");
        aim=trail.Select({20,0,42},40,{1,0,0});
        expect(trail.cursor()>10&&aim.heading.x>=.499f,"alpha53 car on turn exit reacquires new direction without returning to missed corner");
        expect(!trail.Record({NAN,0,0},{0,0,1})&&!trail.Select({},NAN,{0,0,1}).valid,
            "alpha53 nonfinite trajectory inputs rejected");
        expect(!trail.Select({},30,{}).valid,"alpha53 zero rival orientation has no synthetic turn");
        expect(!trail.Record({1000,0,1000},{0,0,1})&&trail.size()==1,
            "alpha53 teleport starts separate trajectory without invented connection");
        trail.Reset();bool bounded=true;std::uint64_t prior=0;
        for(unsigned i=0;i<10000;++i) {
            const float z=float(i)*2;
            bounded=trail.Record({0,0,z},{0,0,1})&&bounded;
            aim=trail.Select({30,0,z-40},40,{0,0,1});
            bounded=bounded&&aim.valid&&trail.cursor()>=prior&&aim.position.z>z-40;
            prior=trail.cursor();
        }
        expect(bounded&&trail.size()<64,"alpha53 long offset following stays forward bounded and monotonic");
        trail.Reset();bounded=true;
        for(unsigned i=0;i<10000;++i)bounded=trail.Record({0,0,float(i)*2},{0,0,1})&&bounded;
        expect(bounded&&trail.size()==trail.Capacity&&!trail.blocked(),"alpha53 full history rolls without permanently disabling guidance");
        trail.Reset();expect(trail.size()==0&&trail.target()==0,"alpha53 role reset releases directional state");
    }
