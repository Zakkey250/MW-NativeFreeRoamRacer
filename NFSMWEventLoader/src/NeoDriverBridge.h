#pragma once
#include <windows.h>
#include "DifficultyPolicy.h"
bool MWELNeoDifficultySample(unsigned handle,mwel_difficulty::RaceSample& sample);
struct MWELNeoScopeV1 {void* vehicle;unsigned handle,driverClass,failure;float heading[3],speed;};
bool MWELInstallNeoDriver();
bool MWELNeoDriverInstalled();
void MWELResetNeoDriverRace();
bool MWELNeoHoldingAtGenuineGoal(unsigned handle);
