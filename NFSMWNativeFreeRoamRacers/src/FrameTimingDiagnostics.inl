// Read-only, bounded evidence for reported transient stalls. Not an error detector.
struct FrameTimingState {
    ULONGLONG previousEnd=0,lastReport=0;
    bool reported=false;
};
FrameTimingState g_frameTiming;
bool ShouldReportFrameDelay(bool roaming,ULONGLONG gap,ULONGLONG nativeMs,
                            ULONGLONG audioMs,ULONGLONG updateMs,ULONGLONG now,
                            FrameTimingState* state) noexcept {
    if(!roaming || (gap<250 && nativeMs<100 && audioMs<100 && updateMs<100)) return false;
    if(state->reported && (now<state->lastReport || now-state->lastReport<5000)) return false;
    state->lastReport=now;state->reported=true;
    return true;
}
void ObserveFrameTiming(ULONGLONG start,ULONGLONG nativeEnd,ULONGLONG audioEnd,
                         ULONGLONG end,bool roaming) noexcept {
    const auto gap=g_frameTiming.previousEnd && start>=g_frameTiming.previousEnd ? start-g_frameTiming.previousEnd : 0;
    g_frameTiming.previousEnd=end;
    if(nativeEnd<start || audioEnd<nativeEnd || end<audioEnd) return;
    if(ShouldReportFrameDelay(roaming,gap,nativeEnd-start,audioEnd-nativeEnd,end-audioEnd,end,&g_frameTiming))
        Log(LogLevel::Info,"FRAME_DELAY gapMs=%llu nativeCallMs=%llu audioDiagnosticMs=%llu populationUpdateMs=%llu observationOnly=1 notAnException=1",
            gap,nativeEnd-start,audioEnd-nativeEnd,end-audioEnd);
}
