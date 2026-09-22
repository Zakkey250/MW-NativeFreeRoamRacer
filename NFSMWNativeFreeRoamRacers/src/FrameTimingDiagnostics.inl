// Read-only, bounded evidence for reported transient stalls. Not an error detector.
struct FrameTimingState {
    ULONGLONG previousEnd=0,previousStart=0,lastReport=0;
    bool reported=false,roaming=false;
};
FrameTimingState g_frameTiming;
bool ShouldReportFrameDelay(bool roaming,ULONGLONG gap,ULONGLONG nativeMs,
                            ULONGLONG audioMs,ULONGLONG updateMs,ULONGLONG now,
                            FrameTimingState* state,ULONGLONG externalMs=0) noexcept {
    if(!roaming || (gap<250 && nativeMs<100 && audioMs<100 && updateMs<100 && externalMs<100)) return false;
    if(state->reported && (now<state->lastReport || now-state->lastReport<5000)) return false;
    state->lastReport=now;state->reported=true;
    return true;
}
bool ValidFrameTiming(ULONGLONG start,ULONGLONG nativeEnd,ULONGLONG audioEnd,
                      ULONGLONG updateEnd,ULONGLONG end) noexcept {
    return start && nativeEnd>=start && audioEnd>=nativeEnd && updateEnd>=audioEnd && end>=updateEnd;
}
void ObserveFrameTiming(ULONGLONG start,ULONGLONG nativeEnd,ULONGLONG audioEnd,
                         ULONGLONG updateEnd,ULONGLONG end,bool roaming) noexcept {
    ObserveAdaptiveSpawnFrame(start,end,roaming&&ValidFrameTiming(start,nativeEnd,audioEnd,updateEnd,end),g_settings.adaptiveSpawnPacing?GetTickCount64():0);
    if(!g_settings.frameTimingDiagnostics)return;
    if(!roaming || !g_frameTiming.roaming || !ValidFrameTiming(start,nativeEnd,audioEnd,updateEnd,end)) {
        // Exclude menu/loading/world-transition intervals from driving distributions.
        g_frameTiming={};g_frameTiming.roaming=roaming;
        ResetPerformanceWindow(end);
    } else {
        const auto gap=g_frameTiming.previousEnd && start>=g_frameTiming.previousEnd ? start-g_frameTiming.previousEnd : 0;
        RecordPerformance(PerfStage::FramePeriod,g_frameTiming.previousStart,start);
        RecordPerformance(PerfStage::OutsideHook,g_frameTiming.previousEnd,start);
        RecordPerformance(PerfStage::NativeChain,start,nativeEnd);
        RecordPerformance(PerfStage::AudioDiagnostics,nativeEnd,audioEnd);
        RecordPerformance(PerfStage::NfrrUpdate,audioEnd,updateEnd);
        RecordPerformance(PerfStage::ExternalTick,updateEnd,end);
        if(ShouldReportFrameDelay(true,gap/1000,(nativeEnd-start)/1000,(audioEnd-nativeEnd)/1000,
            (updateEnd-audioEnd)/1000,end/1000,&g_frameTiming,(end-updateEnd)/1000))
            Log(LogLevel::Info,"FRAME_DELAY gapMs=%llu nativeCallMs=%llu audioDiagnosticMs=%llu populationUpdateMs=%llu externalTickMs=%llu observationOnly=1 notAnException=1 nativeIncludesHookChain=1 outsideHookUnattributed=1",
                gap/1000,(nativeEnd-start)/1000,(audioEnd-nativeEnd)/1000,(updateEnd-audioEnd)/1000,(end-updateEnd)/1000);
        ReportPerformanceWindow(end);
    }
    // Keep our synchronous report IO out of the next outsideHook bucket.
    const auto reportEnd=PerformanceNow();
    if(roaming)RecordPerformance(PerfStage::Reporting,end,reportEnd);
    g_frameTiming.previousStart=roaming?start:0;
    g_frameTiming.previousEnd=roaming?reportEnd:0;
}
