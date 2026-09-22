    {
        struct Ops {
            EncounterNativeAI ai{reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),reinterpret_cast<void*>(3),
                reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),reinterpret_cast<void*>(6),1,true};
            unsigned starts=0,repairs=0,chooses=0;
            bool invalidate=true,recover=true,recoverValid=true,changeOwner=false,changeAfterRepair=false,nullAction=false,readOK=true;
            bool Read(const battle::Identity&,EncounterNativeAI& out,const char*& field){out=ai;field="fixture";return readOK;}
            bool Start(const EncounterNativeAI&){++starts;if(invalidate)ai.navValid=0;if(changeOwner)ai.nav=nullptr;if(nullAction){ai.action=nullptr;ai.raceAction=false;}return true;}
            bool Recover(const EncounterNativeAI&){++repairs;if(!recover)return false;if(recoverValid)ai.navValid=1;if(changeAfterRepair)ai.goal=nullptr;return true;}
            bool Choose(const EncounterNativeAI&){++chooses;ai.action=reinterpret_cast<void*>(4);ai.raceAction=true;return true;}
        };
        using R=NativeStartResult;
        expect(!NativeReseedFresh(false,1000,1000)&&!NativeReseedFresh(true,1000,999),"alpha66 invalid or backwards seed rejected");
        expect(NativeReseedFresh(true,1000,1100)&&!NativeReseedFresh(true,1000,1101),"alpha66 own road snapshot maximum age 100ms");
        for(unsigned live:{5u,20u,23u,25u,28u}){
            Ops ops;EncounterNativeStart state;
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Ready&&ops.starts==1&&ops.repairs==1,
                "alpha66 immediate owned road reseed fixes invalidated native start independent of population");
            for(unsigned i=0;i<8;++i)AdvanceNativeStart(state,{},1050+i*50,live,ops);
            expect(ops.starts==1&&ops.repairs==1&&ops.chooses==0,"alpha66 terminal readiness cannot repeat native operations");
            ops={};ops.invalidate=false;state={};
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Ready&&ops.repairs==0,"alpha66 normal valid native start unchanged");
            ops={};ops.recover=false;state={};
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Pending,"alpha66 unavailable seed preserves bounded wait");
            for(unsigned i=1;i<20;++i)AdvanceNativeStart(state,{},1000+i*50,live,ops);
            expect(ops.repairs==1&&AdvanceNativeStart(state,{},2000,live,ops)==R::Failed,"alpha66 no reseed retry or timeout extension");
            ops={};ops.recoverValid=false;state={};
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Pending,"alpha66 native call alone is not readiness evidence");
            ops.ai.navValid=1;
            expect(AdvanceNativeStart(state,{},1050,live,ops)==R::Ready&&ops.repairs==1,"alpha66 later readiness accepted without second reseed");
            ops={};ops.changeOwner=true;state={};
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Failed&&ops.repairs==0,"alpha66 changed owner cannot be reseeded");
            ops={};ops.changeAfterRepair=true;state={};
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Failed&&ops.repairs==1,"alpha66 owner revalidated after native rebuild");
            ops={};ops.nullAction=true;state={};
            expect(AdvanceNativeStart(state,{},1000,live,ops)==R::Pending&&ops.chooses==1,"alpha66 action selection remains bounded after nav repair");
            expect(AdvanceNativeStart(state,{},1050,live,ops)==R::Ready&&ops.repairs==1&&ops.chooses==1,"alpha66 repaired nav and action complete on later tick");
        }
    }
