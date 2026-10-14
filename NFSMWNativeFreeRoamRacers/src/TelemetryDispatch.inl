// Telemetry traversal is diagnostic work; bypass it entirely when logging is OFF.
void LogTelemetry(const VehicleSnapshot& player,
                  const std::vector<VehicleSnapshot>& vehicles,
                  const float dt) {
    if(!LoggingEnabled())return;
    g_telemetryTimer += dt;
    if (g_telemetryTimer < g_settings.telemetryIntervalSeconds) return;
    g_telemetryTimer = 0.0f;
    const LogBatch batch;
    LogTelemetryRows(player, vehicles);
}
