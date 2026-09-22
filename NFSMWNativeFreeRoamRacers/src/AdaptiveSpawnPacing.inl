// Display/management thread only. Own scalars, no native object references.
AdaptiveSpawnPolicy g_adaptiveSpawn;
ULONGLONG g_spawnLoadPreviousStart=0,g_spawnLoadDiagnosticAt=0;
void ResetAdaptiveSpawnPacing() noexcept {
    g_adaptiveSpawn={};g_spawnLoadPreviousStart=0;g_spawnLoadDiagnosticAt=0;
}
void ObserveAdaptiveSpawnFrame(ULONGLONG start,ULONGLONG end,bool roaming,ULONGLONG now) noexcept {
    if(!g_settings.adaptiveSpawnPacing||!roaming||!start||end<start||
        (g_spawnLoadPreviousStart&&start<g_spawnLoadPreviousStart)){
        ResetAdaptiveSpawnPacing();return;
    }
    if(g_spawnLoadPreviousStart)g_adaptiveSpawn.Observe(std::max(start-g_spawnLoadPreviousStart,end-start),now);
    g_spawnLoadPreviousStart=start;
}
bool DeferAdaptiveSpawn(bool inProgress,ULONGLONG now) noexcept {
    if(!g_adaptiveSpawn.Defer(g_settings.adaptiveSpawnPacing,inProgress,now))return false;
    if(now>=g_spawnLoadDiagnosticAt){
        g_spawnLoadDiagnosticAt=now+5000;
        Log(LogLevel::Info,"SPAWN_LOAD_WAIT triggerFrameUs=%llu lastFrameUs=%llu deferredChecks=%llu forcedOpportunities=%llu maxWaitMs=2000 newLeaseRequested=0 existingAITickUnchanged=1",
            g_adaptiveSpawn.triggerFrameUs,g_adaptiveSpawn.lastFrameUs,g_adaptiveSpawn.deferrals,g_adaptiveSpawn.forcedOpportunities);
    }
    return true;
}
