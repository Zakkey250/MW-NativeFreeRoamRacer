    {
        using namespace encounter_wave;
        expect(Speaker(L"start_speaker00_cue.wav")==0 && Speaker(L"player_victory_speaker09_x.wav")==9,
            "voice actor parsed from stage filenames");
        expect(Speaker(L"speaker0_")==-1 && Speaker(L"speaker100_x")==-1 && Speaker(L"nospeaker")==-1,
            "voice malformed actor rejected");
        std::vector<unsigned char> wav(48,0);
        auto write32=[&](unsigned p,unsigned v) {std::memcpy(wav.data()+p,&v,4);};
        auto write16=[&](unsigned p,unsigned short v) {std::memcpy(wav.data()+p,&v,2);};
        std::memcpy(wav.data(),"RIFF",4);write32(4,40);std::memcpy(wav.data()+8,"WAVEfmt ",8);
        write32(16,16);write16(20,1);write16(22,1);write32(24,22050);write32(28,44100);
        write16(32,2);write16(34,16);std::memcpy(wav.data()+36,"data",4);write32(40,4);
        Pcm pcm{};
        expect(Parse(wav,pcm)&&pcm.rate==22050&&pcm.offset==44&&pcm.bytes==4,"voice bounded PCM parser accepts installed format");
        const auto good=wav;
        write32(40,0xffffffff);expect(!Parse(wav,pcm),"voice chunk overflow rejected");wav=good;
        write32(4,100);expect(!Parse(wav,pcm),"voice truncated RIFF rejected");wav=good;
        write16(20,3);expect(!Parse(wav,pcm),"voice unsupported codec rejected");wav=good;
        write16(22,0);expect(!Parse(wav,pcm),"voice zero channels rejected");wav=good;
        write32(28,1);expect(!Parse(wav,pcm),"voice inconsistent format rejected");wav=good;
        for(std::size_t n=0;n<good.size();++n)
            expect(!Parse(std::span<const unsigned char>(good.data(),n),pcm),"voice every truncated prefix rejected");
        wchar_t root[MAX_PATH]{};
        const auto rootLength=GetEnvironmentVariableW(L"NFR_TEST_VOICE_DIRECTORY",root,MAX_PATH);
        if(rootLength>0&&rootLength<MAX_PATH) {
        unsigned counts[100][3]{};unsigned total=0,bad=0;
        const wchar_t* folders[]={L"start",L"player_victory",L"player_defeat"};
        for(unsigned stage=0;stage<3;++stage) {
            const auto directory=std::wstring(root)+L"\\"+folders[stage]+L"\\";
            WIN32_FIND_DATAW file{};HANDLE find=FindFirstFileW((directory+L"*.wav").c_str(),&file);
            if(find==INVALID_HANDLE_VALUE) continue;
            do {
                if(file.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
                std::vector<unsigned char> bytes;
                const int actor=Speaker(file.cFileName);
                if(actor<0||!ReadEncounterWave(directory+file.cFileName,bytes,pcm)) {++bad;continue;}
                ++counts[actor][stage];++total;
            } while(FindNextFileW(find,&file));
            FindClose(find);
        }
        unsigned complete=0;
        for(unsigned i=0;i<100;++i) if(counts[i][0]&&counts[i][1]&&counts[i][2]) ++complete;
        expect(total==599&&bad==0&&complete==5,"all 599 installed WAVs pass production decoder and five complete actors (read-only no playback)");
        } else std::puts("SKIP optional owned-voice test: set NFR_TEST_VOICE_DIRECTORY (no installed path assumed)");
    }
    {
        using namespace NFSPluginSDK::MW05;
        std::printf("LAYOUT databaseProfile=%X profileCareer=%X careerCash=%X totalCashOffset=%X\n",
            unsigned(offsetof(cFrontEndDatabase,CurrentUserProfiles)),unsigned(offsetof(UserProfile,mTheCareerSettings)),
            unsigned(offsetof(CareerSettings,CurrentCash)),unsigned(offsetof(UserProfile,mTheCareerSettings)+offsetof(CareerSettings,CurrentCash)));
        // Raw fixtures: SDK aggregate constructors may call the real game.
        std::vector<unsigned char> databaseBytes(sizeof(cFrontEndDatabase)),profileBytes(sizeof(UserProfile));
        auto* database=reinterpret_cast<cFrontEndDatabase*>(databaseBytes.data());
        auto* profile=reinterpret_cast<UserProfile*>(profileBytes.data());
        void* oldDatabase=nullptr;void* slot=reinterpret_cast<void*>(Address(0x0091CF90));
        std::memcpy(&oldDatabase,slot,sizeof(oldDatabase));
        std::memcpy(slot,&database,sizeof(database));
        database->bProfileLoaded=true;database->CurrentUserProfiles[0]=profile;
        profile->mTheCareerSettings.CurrentCash=2000;
        g_battle={};g_battle.profile=profile;
        expect(!AwardEncounterCash()&&profile->mTheCareerSettings.CurrentCash==2000,"no reward outside won phase");
        battle::Sample s{};s.player.identity={1,11,111};s.rival.identity={2,22,222};
        s.player.forward=s.rival.forward={0,0,1};s.player.speed=s.rival.speed=30;s.rival.position.z=10;
        auto win=[&] {
            g_battle.model.Reset();g_battle.rewardAttempted=false;
            s.player.position={};s.rival.position.z=10;g_battle.model.Start(s);
            s.player.position.z=13;s.rival.position.z=11;g_battle.model.Step(s,.05f);
            s.player.position.z=15;s.rival.position.z=12;g_battle.model.Step(s,.05f);
            for(unsigned i=0;i<30;++i) {s.player.position.z+=10;g_battle.model.Step(s,.1f);}
        };
        win();expect(AwardEncounterCash()&&profile->mTheCareerSettings.CurrentCash==3000,"won adapter pays exactly 1000 to captured active profile");
        expect(!AwardEncounterCash()&&profile->mTheCareerSettings.CurrentCash==3000,"duplicate finish cannot pay twice");
        for(unsigned amount:{300u,3000u}) {
            profile->mTheCareerSettings.CurrentCash=2000;g_battle.reward=amount;win();
            expect(AwardEncounterCash()&&profile->mTheCareerSettings.CurrentCash==2000+int(amount),
                "alpha57 captured Custom reward is exactly credited to active profile");
            expect(!AwardEncounterCash(),"alpha57 variable cash still pays at most once");
        }
        g_battle.reward=1000;
        win();profile->mTheCareerSettings.CurrentCash=INT32_MAX-500;
        expect(!AwardEncounterCash()&&profile->mTheCareerSettings.CurrentCash==INT32_MAX-500,"reward overflow fails without mutation");
        win();g_battle.profile=nullptr;
        expect(!AwardEncounterCash(),"changed or missing profile cannot receive reward");
        g_battle.pending=true;g_battle.actor=9;g_battle.aiChanged=false;g_gaugeVisible=true;
        EndEncounterBattle(false,true);
        expect(!EncounterBattleBusy()&&!g_gaugeVisible&&g_battle.actor==-1&&!g_battle.profile,
            "finish/world reset clears race actor HUD profile and pending state");
        std::memcpy(slot,&oldDatabase,sizeof(oldDatabase));g_encounterCooldownUntil=0;
        const auto& ja=encounter_text::japanese;
        expect(std::wcscmp(ja[0],L"\u30d0\u30c8\u30eb\u958b\u59cb\uff01\n\u307e\u305a\u306f\u8ffd\u3044\u629c\u304b\u306a\u3044\u3068\uff01")==0,
            "requested Japanese start and newline preserved as Unicode");
        expect(std::wcsstr(ja[3],L"+1000") && encounter_text::Get(encounter_text::Id::Victory,true)==encounter_text::english[3],
            "message IDs support independent Japanese and English tables");
        auto geometry=std::make_unique<GaugeGeometry>();
        geometry->Rect(0,0,0,1,0);expect(geometry->count==0,"zero distance gauge does not draw fill");
        geometry->Rect(0,0,300,20,0xff00ff00);
        expect(geometry->count==6&&geometry->vertices[1].x==299.5f,"300m gauge uses bounded full-width geometry");
        for(unsigned i=0;i<2000;++i) geometry->Rect(0,0,1,1,0);
        expect(geometry->count<=geometry->vertices.size()&&geometry->count%6==0,"gauge geometry cannot overflow buffer");
    }
