#pragma once
// Metadata only; model strings are owned by the runtime catalog, not game memory.
struct InstalledVehicle {
    const char* model;
    unsigned int key;
    int cost;
    bool customizable = true;
};
