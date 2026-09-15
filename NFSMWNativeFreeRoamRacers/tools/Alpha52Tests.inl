    {
        // alpha.51 runtime, battle 2: the initial breadcrumb was held until
        // the 300m result despite the rival passing on a parallel driving line.
        encounter_custom::Trail trace;
        const battle::Point start{-4318.99f,213.37f,777.41f};
        const battle::Point heading{-.675f,0,.738f};
        trace.Record(start,heading);
        trace.Select({-4321.99f,212.81f,769.59f},24.4f,{-.649f,0,.760f});
        trace.Record({-4346.34f,213.59f,799.69f},{-.834f,0,.550f});
        const auto initial=trace.cursor();
        auto destination=trace.Select({-4338.62f,213.56f,788.63f},27.2f,{-.665f,0,.747f});
        expect(trace.cursor()>initial&&destination.position.x!=start.x,
            "alpha52 runtime battle 2 parallel pass releases first breadcrumb");

        // Battle 1: reconstruct a short local span from logged request 61/67.
        // The intermediate player samples were not logged; this is not a full replay.
        trace.Reset();
        trace.Record({-2865.28f,171.69f,1526.85f},{-.746f,0,-.666f});
        trace.Record({-2874.56f,170.52f,1516.09f},{-.591f,0,-.807f});
        trace.Record({-2888.05f,169.25f,1484.69f},{-.421f,0,-.905f});
        trace.Select({-2869.24f,169.91f,1514.19f},37.8f,{-.421f,0,-.905f});
        // Runtime telemetry is 1Hz, management is 20Hz. Interpolate this sparse
        // fixture rather than falsely treating the 1-second log as one AI tick.
        for(unsigned step=1;step<=20;++step) {
            const float t=float(step)/20;
            destination=trace.Select({-2869.24f+(-2885.14f+2869.24f)*t,
                169.91f+(169.06f-169.91f)*t,1514.19f+(1481.06f-1514.19f)*t},
                35.4f,{-.363f,0,-.932f});
        }
        expect(trace.cursor()==3&&destination.position.z==1484.69f,
            "alpha52 reconstructed battle 1 lane offset does not leave target behind");

        trace.Reset();trace.Record({0,0,0},{0,0,1});trace.Record({0,0,20},{0,0,1});
        destination=trace.Select({0,0,-5},40,{0,0,1});
        const auto oldCursor=trace.cursor();
        trace.Record({0,0,40},{0,0,1});trace.Record({0,0,60},{0,0,1});
        destination=trace.Select({0,0,-5},40,{0,0,1});
        expect(trace.cursor()==oldCursor&&destination.position.z==60,
            "alpha52 straight recorded endpoint extends before rival arrival to reduce braking");
        destination=trace.Select({0,0,-5},0,{0,0,1});
        expect(destination.position.z==60,"alpha52 rolling endpoint never retracts under braking");
        trace.Record({0,0,80},{0,0,1});trace.Record({20,0,80},{1,0,0});
        destination=trace.Select({0,0,-5},100,{0,0,1});
        expect(destination.position.x==0&&destination.position.z==80,
            "alpha52 longer horizon still stops at recorded corner not later branch");
        trace.Reset();
        for(unsigned i=0;i<=60;++i) trace.Record({0,0,float(i)*2},{0,0,1});
        destination=trace.Select({0,0,-5},100,{0,0,1});
        expect(destination.position.z==100,"alpha52 long straight horizon capped at 100m");
        for(float offset:{-12.f,-7.f,7.f,12.f}) {
            trace.Reset();trace.Record({0,0,0},{0,0,1});trace.Record({0,0,20},{0,0,1});
            trace.Select({offset,0,8},40,{0,0,1});
            expect(trace.cursor()==2,"alpha52 passed point accepts bounded MW lane offset on either side");
        }
        for(auto rival:{battle::Point{12.1f,0,8},battle::Point{20,0,8},
                battle::Point{7,8,8},battle::Point{7,0,-8},battle::Point{7,0,31}}) {
            trace.Reset();trace.Record({0,0,0},{0,0,1});trace.Record({0,0,20},{0,0,1});
            trace.Select(rival,40,{0,0,1});
            expect(trace.cursor()==1,"alpha52 wrong branch height behind and distant gates remain bounded");
        }
        trace.Reset();trace.Record({0,0,0},{0,0,1});trace.Record({0,0,20},{0,0,1});
        trace.Select({7,0,8},40,{0,0,-1});
        expect(trace.cursor()==1,"alpha52 reverse heading cannot pass a parallel breadcrumb");
        trace.Select({7,0,-1},40,{0,0,1});
        expect(trace.cursor()==1,"alpha52 wider passed corridor does not widen near-point arrival radius");
        trace.Reset();
        bool moving=true;std::uint64_t previous=0;
        for(unsigned i=0;i<10000;++i) {
            const float z=float(i)*2;
            moving=trace.Record({0,0,z},{0,0,1})&&moving;
            destination=trace.Select({7,0,z-20},40,{0,0,1});
            moving=moving&&destination.valid&&trace.cursor()>=previous;
            previous=trace.cursor();
        }
        expect(moving&&trace.size()<32&&!trace.blocked(),
            "alpha52 lane offset long drive retains monotonic cursor and bounded ring");
    }
