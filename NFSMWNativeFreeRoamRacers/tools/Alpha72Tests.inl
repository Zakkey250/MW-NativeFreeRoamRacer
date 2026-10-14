    {
        const auto oldSettings=g_settings;
        const auto oldFrequency=g_frequency;
        const auto oldTiming=g_frameTiming;
        const auto oldPerf=g_performance;
        const auto oldWindow=g_performanceWindowStart;
        const Settings defaults;
        expect(!defaults.deepAudioDiagnostics && !defaults.frameTimingDiagnostics,
            "diagnostic defaults OFF independent of LB; frame timing opt-in");
        expect(defaults.freeRoamAudioSlots==4 && !defaults.lbEngineAudioExpansion,
            "alpha72 preserves four audio slots and optional expansion OFF");
        PerfHistogram h;
        expect(h.QuantileUpper(95)==0,"alpha72 empty histogram has no percentile");
        for(unsigned i=0;i<95;++i)h.Add(16000);
        for(unsigned i=0;i<4;++i)h.Add(100000);
        h.Add(6000000);
        expect(h.count==100 && h.QuantileUpper(50)==16000 && h.QuantileUpper(95)==16000 &&
            h.QuantileUpper(99)==100000 && h.QuantileUpper(100)==6000000,
            "alpha72 histogram tails and overflow upper bounds");
        expect(h.over33ms==5 && h.over100ms==1 && h.maximum==6000000 && h.total==7920000,
            "alpha72 stall counts total and maximum retained");
        expect(ValidFrameTiming(100,200,300,400,500) && !ValidFrameTiming(0,0,0,0,0) &&
            !ValidFrameTiming(100,99,300,400,500) && !ValidFrameTiming(100,200,199,400,500) &&
            !ValidFrameTiming(100,200,300,299,500) && !ValidFrameTiming(100,200,300,400,399),
            "alpha72 rejects invalid frame boundaries");
        FrameTimingState threshold{};
        expect(ShouldReportFrameDelay(true,0,0,0,0,1000,&threshold,100),
            "alpha72 isolated external callback stall also reported");
        g_settings.frameTimingDiagnostics=true;
        QueryPerformanceFrequency(&g_frequency);
        expect(PerformanceNow()>0,"alpha72 QPC microsecond clock available");
        for(ULONGLONG externalUs:{0ULL,123000ULL}) {
            ResetPerformanceWindow(1000);g_frameTiming={};g_frameTiming.roaming=true;
            g_frameTiming.previousStart=1000;g_frameTiming.previousEnd=1200;
            ObserveFrameTiming(2000,2100,2150,2200,2200+externalUs,true);
            expect(g_performance[static_cast<unsigned>(PerfStage::NfrrUpdate)].total==50 &&
                g_performance[static_cast<unsigned>(PerfStage::ExternalTick)].total==externalUs &&
                g_performance[static_cast<unsigned>(PerfStage::NativeChain)].total==100 &&
                g_performance[static_cast<unsigned>(PerfStage::OutsideHook)].total==800 &&
                g_performance[static_cast<unsigned>(PerfStage::FramePeriod)].total==1000,
                "alpha72 callback absent/present has separate nonoverlapping timing");
        }
        ObserveFrameTiming(3000,3100,3150,3200,3300,false);
        expect(!g_frameTiming.roaming && g_frameTiming.previousEnd==0 &&
            !g_performance[static_cast<unsigned>(PerfStage::FramePeriod)].count,
            "alpha72 frontend transition clears driving window");
        ObserveFrameTiming(100000,100100,100150,100200,100300,true);
        expect(!g_performance[static_cast<unsigned>(PerfStage::FramePeriod)].count,
            "alpha72 first returning frame excludes loading interval");
        g_settings.frameTimingDiagnostics=false;
        ResetPerformanceWindow(1000);
        RecordPerformance(PerfStage::Construct,1,100000);
        ObserveFrameTiming(2000,2100,2200,2300,2400,true);
        expect(PerformanceNow()==0 && !g_performance[static_cast<unsigned>(PerfStage::Construct)].count &&
            !g_performance[static_cast<unsigned>(PerfStage::FramePeriod)].count,
            "alpha72 timing OFF performs no sampling");
        g_settings=oldSettings;g_frequency=oldFrequency;g_frameTiming=oldTiming;
        g_performance=oldPerf;g_performanceWindowStart=oldWindow;
    }
