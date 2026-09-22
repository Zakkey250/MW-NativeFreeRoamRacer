    {
        RefillPacing pacing;
        expect(RefillPacing::Interval(5,0,6)==1,"alpha67 empty population fast refill");
        expect(RefillPacing::Interval(5,1,6)==2&&RefillPacing::Interval(5,3,6)==2,"alpha67 large deficit two second refill");
        expect(RefillPacing::Interval(5,4,6)==5&&RefillPacing::Interval(5,5,6)==5,"alpha67 small deficit preserves configured cadence");
        for(std::size_t maximum=1;maximum<=15;++maximum){
            for(std::size_t count=0;count<=maximum;++count){
                expect(RefillPacing::Interval(.5f,count,maximum)<=.5f,"alpha67 preserves faster user interval");
                expect(pacing.Ready(5,count,maximum)==(count<maximum),"alpha67 no overpopulation at any configured cap");
            }
        }
        pacing.Spawned();expect(!pacing.Ready(5,1,6),"alpha67 actual generation starts cooldown");
        pacing.Tick(1,1,6);expect(!pacing.Ready(5,1,6),"alpha67 search tick cannot bypass generation cooldown");
        pacing.Tick(1,1,6);expect(pacing.Ready(5,1,6),"alpha67 generation allowed after adaptive interval");
        pacing.Spawned();pacing.Tick(10,2,6);expect(pacing.shortage==10,"alpha67 failed searches accumulate shortage age");
        pacing.Tick(1,6,6);expect(pacing.shortage==0,"alpha67 full population clears fallback age");
        pacing.Tick(std::numeric_limits<float>::quiet_NaN(),1,6);expect(std::isfinite(pacing.sinceSpawn),"alpha67 nonfinite elapsed rejected");
        const auto rank=[](float distance,float ahead,float heading,float age=0,bool offscreen=true){return RefillAnchorRank(distance,ahead,heading,350,600,.25f,age,offscreen);};
        expect(rank(350,1,1)==0&&rank(475,1,1)==0,"alpha67 front inner band prioritized");
        expect(rank(476,1,1)==1&&rank(600,1,1)==1,"alpha67 front outer band still eligible");
        expect(rank(349,1,1)<0&&rank(601,1,1)<0,"alpha67 distances never expanded");
        expect(rank(400,0,1)==2&&rank(400,-1,1)==3,"alpha67 front priority over side and rear");
        expect(rank(400,1,0,9.99f)<0&&rank(400,1,0,10)==2,"alpha67 lateral heading fallback delayed ten seconds");
        expect(rank(400,1,-.01f,100)<0,"alpha67 fallback never accepts opposing heading");
        expect(rank(400,1,0,100,false)<0,"alpha67 fallback never bypasses offscreen guard");
        expect(rank(400,-1,0,100)<0,"alpha67 fallback never chooses behind player");
        expect(rank(std::numeric_limits<float>::quiet_NaN(),1,1)<0,"alpha67 invalid distance rejected");
        // Bounded simulation: under continuous valid anchors and available slots,
        // cap six is filled at most one car per callback, with proper pacing.
        unsigned count=0,spawns=0;pacing={};float search=0;
        for(unsigned i=0;i<1200;++i){
            pacing.Tick(.05f,count,6);search+=.05f;
            if(search>=1&&pacing.Ready(5,count,6)){search=0;++count;++spawns;pacing.Spawned();}
            expect(count<=6,"alpha67 simulated long refill never exceeds maximum");
        }
        expect(count==6&&spawns==6,"alpha67 continuous eligible anchors refill to six without further spawning");
        count=0;
        for(unsigned i=0;i<400;++i)pacing.Tick(.05f,count,6);
        expect(pacing.Ready(5,count,6)&&pacing.shortage>=10,"alpha67 long anchor absence neither latches nor reserves traffic");
    }
