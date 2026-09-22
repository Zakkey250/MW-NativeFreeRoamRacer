// A short synchronous report, not a queue: every row is still written immediately.
// Keep the existing cadence and call order. No file is opened before the report is due.
void LogTelemetry(const VehicleSnapshot& player,
                  const std::vector<VehicleSnapshot>& vehicles,
                  const float dt) {
    g_telemetryTimer += dt;
    if (g_telemetryTimer < g_settings.telemetryIntervalSeconds) return;
    g_telemetryTimer = 0.0f;
    const LogBatch batch;
    LogTelemetryRows(player, vehicles);
}
