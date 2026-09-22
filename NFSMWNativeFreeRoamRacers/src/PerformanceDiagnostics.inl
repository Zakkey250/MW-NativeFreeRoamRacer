// Display-thread-only fixed storage. No per-frame allocation, sorting, file IO,
// game-state reads or native calls. Timings are observations, not fault guards.
enum class PerfStage : unsigned {
    FramePeriod, NativeChain, AudioDiagnostics, NfrrUpdate, ExternalTick, OutsideHook,
    Reporting, Enumerate, Battle, Refresh, Replenish, Candidate, Police, Telemetry,
    Construct, Activate, SpawnSelect, SpawnAppearance, SpawnFinalize, Count
};
constexpr const char* kPerfNames[] = {
    "framePeriod", "nativeChain", "audioDiagnostics", "nfrrUpdate", "externalTick", "outsideHook",
    "reporting", "enumerate", "battle", "refresh", "replenish", "candidate", "police", "telemetry",
    "construct", "activate", "spawnSelect", "spawnAppearance", "spawnFinalize"
};
// Microseconds. Quantiles are bucket upper bounds, NOT exact percentiles.
constexpr ULONGLONG kPerfBounds[] = {250,500,1000,2000,4000,8000,12000,16000,
    20000,25000,33000,50000,75000,100000,150000,250000,500000,1000000,2000000,5000000};
struct PerfHistogram {
    ULONGLONG count=0,total=0,maximum=0,over33ms=0,over100ms=0;
    std::array<ULONGLONG, std::size(kPerfBounds)+1> buckets{};
    void Add(ULONGLONG us) noexcept {
        ++count; total+=us; maximum=(std::max)(maximum,us);
        over33ms+=us>33000;over100ms+=us>100000;
        std::size_t slot=0;
        while(slot<std::size(kPerfBounds) && us>kPerfBounds[slot])++slot;
        ++buckets[slot];
    }
    ULONGLONG QuantileUpper(unsigned percent) const noexcept {
        if(!count || percent==0 || percent>100)return 0;
        const auto target=(count*percent+99)/100;
        ULONGLONG seen=0;
        for(std::size_t i=0;i<buckets.size();++i) {
            seen+=buckets[i];
            if(seen>=target)return i<std::size(kPerfBounds)?kPerfBounds[i]:maximum;
        }
        return maximum;
    }
};
std::array<PerfHistogram,static_cast<unsigned>(PerfStage::Count)> g_performance{};
static_assert(std::size(kPerfNames)==static_cast<unsigned>(PerfStage::Count));
ULONGLONG g_performanceWindowStart=0;

ULONGLONG ReadPerformanceClock() noexcept {
    if(g_frequency.QuadPart<=0)return 0;
    LARGE_INTEGER now{};
    if(!QueryPerformanceCounter(&now) || now.QuadPart<=0)return 0;
    const auto ticks=static_cast<ULONGLONG>(now.QuadPart);
    const auto frequency=static_cast<ULONGLONG>(g_frequency.QuadPart);
    // Avoid overflowing after a long system uptime.
    return (ticks/frequency)*1000000 + (ticks%frequency)*1000000/frequency;
}
ULONGLONG PerformanceNow() noexcept {
    return g_settings.frameTimingDiagnostics?ReadPerformanceClock():0;
}
ULONGLONG FrameClockNow() noexcept {
    return g_settings.frameTimingDiagnostics||g_settings.adaptiveSpawnPacing?ReadPerformanceClock():0;
}
void RecordPerformance(PerfStage stage,ULONGLONG start,ULONGLONG end) noexcept {
    if(!g_settings.frameTimingDiagnostics || !start || end<start)return;
    g_performance[static_cast<unsigned>(stage)].Add(end-start);
}
void PerformanceCheckpoint(PerfStage stage,ULONGLONG* start) noexcept {
    const auto end=PerformanceNow();RecordPerformance(stage,*start,end);*start=end;
}
void ResetPerformanceWindow(ULONGLONG start) noexcept {
    g_performance={};g_performanceWindowStart=start;
}
void ReportPerformanceWindow(ULONGLONG now) noexcept {
    if(!g_performanceWindowStart || now<g_performanceWindowStart) {
        ResetPerformanceWindow(now);return;
    }
    if(now-g_performanceWindowStart<30000000)return;
    const auto window=now-g_performanceWindowStart;
    // At most 19 rows per 30 seconds. Stage rows are nested: do not sum them.
    const LogBatch batch;
    for(unsigned i=0;i<static_cast<unsigned>(PerfStage::Count);++i) {
        const auto& h=g_performance[i];
        if(!h.count)continue;
        Log(LogLevel::Info,"PERF_WINDOW stage=%s windowUs=%llu samples=%llu totalUs=%llu maxUs=%llu p50UpperUs=%llu p95UpperUs=%llu p99UpperUs=%llu over33ms=%llu over100ms=%llu quantiles=bucket-upper nestedStages=1",
            kPerfNames[i],window,h.count,h.total,h.maximum,h.QuantileUpper(50),h.QuantileUpper(95),h.QuantileUpper(99),h.over33ms,h.over100ms);
    }
    ResetPerformanceWindow(now);
}
