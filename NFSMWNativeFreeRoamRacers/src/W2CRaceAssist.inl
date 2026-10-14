#include "../include/RaceGuidanceTarget.h"
#include <NFSPluginSDK/Game.MW05/Types/WRoadNav.h>
static_assert(offsetof(NFSPluginSDK::MW05::WRoadNav,fPosition)==0x98);
static_assert(offsetof(NFSPluginSDK::MW05::WRoadNav,fForwardVector)==0xBC);
// Optional provider: NFRR remains independent of the W2C installation.
// NFRR CustomAI direct guidance; W2C only supplies an ordered yellow trail.
// W2C uses native memory XYZ (middle component is height). SDK UVector3
// is stored as y,z,x; VehicleSnapshot holds the SDK's named components.
Vec3 W2CWorldPoint(Vec3 sdk) noexcept {return {sdk.y,sdk.z,sdk.x};}
Vec3 W2CSdkPoint(const float* raw) noexcept {return {raw[2],raw[0],raw[1]};}
unsigned W2CRaceHandle(const VehicleSnapshot& racer) noexcept {
 // GetVehicleKey is an Attrib instance key; RaceStatus uses ISimable handles.
 auto* simable=static_cast<unsigned char*>(GetSimablePointer(racer.pointer));unsigned handle=0;
 return simable&&BattleVtable(simable,kISimableVtable)&&AudioRead(simable+8,&handle)?handle:0;
}
bool W2CRaceVehicle(void* ai,VehicleSnapshot& racer,VehicleSnapshot& player,bool localHuman=false,bool requireRaceAction=true,unsigned* failure=nullptr) noexcept {
 auto fail=[&](unsigned reason){if(failure)*failure=reason;return false;};if(failure)*failure=0;
 using Active=unsigned(__cdecl*)();
 const auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");
 const auto active=module?reinterpret_cast<Active>(GetProcAddress(module,"W2MW_IsWorldActiveV1")):nullptr;
 bool human=localHuman&&BattleVtable(ai,0x8929F0);
 auto events=GetModuleHandleW(L"NFSMWEventLoader.asi");auto eventActive=events?reinterpret_cast<Active>(GetProcAddress(events,"MWEL_RaceActiveV1")):nullptr;
 if((!active||!active())&&(!eventActive||!eventActive()))return fail(1);
 if(!human&&!BattleVtable(ai,kRacecarIVehicleAIVtable))return fail(15);
 void* vehicle=nullptr;void* back=nullptr;
 if(!AudioRead(static_cast<unsigned char*>(ai)-4,&vehicle))return fail(2);
 if(!ReadVehicle(vehicle,&racer))return fail(12);
 if(racer.driverClass!=(human?kDriverHuman:kDriverRacer))return fail(13);
 if(!AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&back)||back!=ai)return fail(14);
 auto* primary=static_cast<unsigned char*>(ai)-0x4C;void* goal=nullptr;void* action=nullptr;
 if(!BattleVtable(primary,human?0x892AD0:kRacecarPrimaryVtable))return fail(3);
 if(requireRaceAction&&(!AudioRead(primary+0xB8,&goal)||!BattleVtable(goal,kAIGoalRacerVtable)||
    !AudioRead(static_cast<unsigned char*>(goal)+4,&action)||!BattleVtable(action,kAIActionRaceVtable)))return fail(4);
 unsigned char* status=nullptr;unsigned count=0,play=0,context=0;unsigned char running=0,go=0;
 if(!AudioRead(reinterpret_cast<void*>(Address(0x91E000)),&status)||!status||!AudioRead(status+0x1958,&count)||count>16||
    !AudioRead(status+0x1960,&play)||play!=1||!AudioRead(status+0x1964,&context)||(context!=2&&!(localHuman&&context==0))||
    !AudioRead(status+0x198A,&running)||!running||!AudioRead(status+0x1978,&go)||!go)return fail(5);
 const unsigned handle=W2CRaceHandle(racer);if(!handle)return fail(6);bool member=false;
 for(unsigned i=0;i<count;++i){unsigned key=0,dead=0;unsigned char finished=0;
  if(!AudioRead(status+0x18+i*0x194,&key)||key!=handle)continue;
  if(!AudioRead(status+0x18+i*0x194+0x1C,&dead)||dead)return fail(7);
  if(!AudioRead(status+0x18+i*0x194+0x21,&finished)||finished)return fail(8);member=true;break;}
 if(!member)return fail(9);
 const void* const* list=nullptr;unsigned size=0;if(!ReadNativeVehicleList(list,size)||size>512)return fail(10);
 for(unsigned i=0;i<size;++i){VehicleSnapshot candidate{};if(ReadVehicle(const_cast<void*>(list[i]),&candidate)&&candidate.driverClass==kDriverHuman){player=candidate;return true;}}
 return fail(11);
}
#pragma comment(linker,"/EXPORT:NFRR_NeoScopeV1=_NFRR_NeoScopeV1")
extern "C" __declspec(dllexport) unsigned __cdecl NFRR_NeoScopeV1(void* ai,unsigned local,unsigned action,MWELNeoScopeV1* out,unsigned size){
 if(!out||size!=sizeof(*out))return 0;*out={};VehicleSnapshot racer{},player{};unsigned failure=0;
 bool ok=W2CRaceVehicle(ai,racer,player,local!=0,action!=0,&failure);out->failure=failure;
 if(ok){out->vehicle=racer.pointer;out->handle=W2CRaceHandle(racer);out->driverClass=racer.driverClass;out->heading[0]=racer.heading.x;out->heading[1]=racer.heading.y;out->heading[2]=racer.heading.z;out->speed=racer.speed;}return ok;
}
#pragma comment(linker,"/EXPORT:NFRR_MirrorRaceJunkmanV1=_NFRR_MirrorRaceJunkmanV1")
extern "C" __declspec(dllexport) unsigned __cdecl NFRR_MirrorRaceJunkmanV1(void* ai,unsigned local,unsigned* playerCount,unsigned* aiCount){
 if(playerCount)*playerCount=0;if(aiCount)*aiCount=0;
 VehicleSnapshot racer{},player{};
 unsigned failure=0;
 if(!W2CRaceVehicle(ai,racer,player,local!=0,false,&failure)||racer.driverClass!=kDriverRacer){
  return 0;
 }
 const bool applied=MirrorJunkmanToRaceAI(player.pointer,racer.pointer,playerCount,aiCount);
 if(applied)Log(LogLevel::Info,"RACE_JUNKMAN_MIRROR handle=%u player=%u ai=%u applied=1 owner=NFRR player_record_unchanged=1",
     W2CRaceHandle(racer),playerCount?*playerCount:0,aiCount?*aiCount:0);
 return applied?1u:0u;
}
bool W2COrderedRouteHint(const VehicleSnapshot& racer,Vec3& aim) noexcept {
 using Target=unsigned(__cdecl*)(unsigned,const float*,const float*,float,float*);
 const auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");
 const auto get=module?reinterpret_cast<Target>(GetProcAddress(module,"W2MW_RaceCustomTargetV1")):nullptr;
 const auto position=W2CWorldPoint(racer.position),heading=W2CWorldPoint(racer.heading);
 const float p[]={position.x,position.y,position.z},h[]={heading.x,heading.y,heading.z};float out[3]{};
 const unsigned handle=W2CRaceHandle(racer);if(!get||!handle||!get(handle,p,h,racer.speed,out))return false;
 aim=W2CSdkPoint(out);return true;
}
// One decision drives InitPath suppression and final aim. Occlusion is
// handled by native steering/braking, rather than switching route controllers.
extern bool g_customDriveInstalled,g_encounterPathInstalled;
struct W2CDirectCommand {
 void* ai=nullptr;void* vehicle=nullptr;void* race=nullptr;unsigned handle=0;
 ULONGLONG at=0,nextLog=0;bool active=false,valid=false;unsigned source=0,blocked=0;
 Vec3 aim{},position{},heading{},nativeAim{};ULONGLONG brakeLogAt=0;float lastSpeedRequest=-1;
 unsigned curveSource=0,aimFailures=0;float issuedCurvature=0;
};
W2CDirectCommand g_w2cCommands[16];
bool W2CReleaseNativeQuery(void* ai) noexcept {
 __try {
  void* nav=nullptr;
  if(!AudioRead(static_cast<unsigned char*>(ai)+0x24,&nav)||!nav)return false;
  // A normal race's AITarget is the physical CP objective. Clearing it makes
  // AIActionRace take 00428BE2 -> 00428EE9 and request zero speed. Preserve it.
  // Release only the competing road request, using NFRR's guarded method.
  reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00770060))(nav);
  return true;
 } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
W2CDirectCommand* W2CCommand(void* ai,ULONGLONG now) noexcept {
 using Mode=unsigned(__cdecl*)();auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");auto mode=module?reinterpret_cast<Mode>(GetProcAddress(module,"W2MW_RaceControllerV1")):nullptr;
 if(mode&&mode()!=1)return nullptr;
 VehicleSnapshot racer{},player{};
 if(!g_battleSurface||!g_customDriveInstalled||!g_encounterPathInstalled||
    !W2CRaceVehicle(ai,racer,player)){
  for(auto& row:g_w2cCommands)if(row.ai==ai){row.active=false;row.valid=false;row.at=0;}
  return nullptr;
 }
 unsigned handle=W2CRaceHandle(racer);void* race=nullptr;
 if(!AudioRead(reinterpret_cast<void*>(Address(0x91E000)),&race))return nullptr;
 W2CDirectCommand* selected=nullptr;
 for(auto& candidate:g_w2cCommands)if(candidate.ai==ai){selected=&candidate;break;}
 if(!selected)for(auto& candidate:g_w2cCommands)if(!candidate.active){selected=&candidate;break;}
 if(!selected)return nullptr;
 auto& row=*selected;
 if(row.ai!=ai||row.vehicle!=racer.pointer||row.race!=race||row.handle!=handle){
  row={};row.ai=ai;row.vehicle=racer.pointer;row.race=race;row.handle=handle;
 }
 const auto position=W2CWorldPoint(racer.position);
 const float dx=position.x-row.position.x,dy=position.y-row.position.y,dz=position.z-row.position.z;
 if(row.at&&now>=row.at&&now-row.at<25&&dx*dx+dy*dy+dz*dz<6400)return row.valid?&row:nullptr;
 row.at=now;row.position=position;row.heading=W2CWorldPoint(racer.heading);row.valid=false;
 Vec3 aim{};row.source=W2COrderedRouteHint(racer,aim)?1:0;if(!row.source)++row.aimFailures;
 if(row.source&&std::isfinite(aim.x)&&std::isfinite(aim.y)&&std::isfinite(aim.z)){
  row.aim=aim;row.valid=row.active||W2CReleaseNativeQuery(ai);
 }
 if(row.active!=row.valid){
  Log(LogLevel::Info,"W2C_DIRECT handle=%u active=%u native_query_release=%u checkpoint_target_preserved=1 native_steering=1 native_braking=1",handle,unsigned(row.valid),unsigned(row.valid));
 }
 row.active=row.valid;
 if(!row.valid)return nullptr;
 if(now>=row.nextLog){row.nextLog=now+5000;
  Log(LogLevel::Info,"W2C_CUSTOM_YELLOW handle=%u road_blocked=%u original_custom_trail=1 next_CP_order=1 custom_pace=0 player_trail=0 forced_recovery=0",handle,row.blocked);}
 return &row;
}
bool W2CDirectDriveHint(void* ai,ULONGLONG now,Vec3& aim) noexcept {
 auto* row=W2CCommand(ai,now);if(!row)return false;aim=row->aim;return true;
}
bool W2CBlockRoadQuery(void* nav) noexcept {
 void* primary=nullptr;void* back=nullptr;
 if(!nav||!AudioRead(static_cast<unsigned char*>(nav)+0x84,&primary)||!BattleVtable(primary,kRacecarPrimaryVtable)||
    !AudioRead(static_cast<unsigned char*>(primary)+0x70,&back)||back!=nav)return false;
 auto* row=W2CCommand(static_cast<unsigned char*>(primary)+0x4C,GetTickCount64());
 if(!row)return false;++row->blocked;return true;
}

void W2CTraceSpeed(void* ai,std::uintptr_t caller,float native,float applied,ULONGLONG now) noexcept {
 if(caller!=Address(0x00428FFA))return;
 for(auto& row:g_w2cCommands){
  if(row.ai!=ai||!row.valid||!row.active||now<row.at||now-row.at>250)continue;
  bool drop=std::isfinite(native)&&row.lastSpeedRequest>=0&&native<row.lastSpeedRequest-10;
  row.lastSpeedRequest=native;
  if(now<row.brakeLogAt+(drop?200:1000))return;row.brakeLogAt=now;
  auto* primary=static_cast<unsigned char*>(ai)-0x4C;void* nav=nullptr;void* goal=nullptr;void* action=nullptr;
  Vec3 rawNav{},navForward{};float curvature=0,limit=0,actual=0;unsigned road=0,avoidable=0;unsigned char behind=0,reverse=0;
  if(!AudioRead(static_cast<unsigned char*>(ai)+0x24,&nav)||!nav||!AudioRead(primary+0xB8,&goal)||!goal||
     !AudioRead(static_cast<unsigned char*>(goal)+4,&action)||!action)return;
  AudioRead(static_cast<unsigned char*>(nav)+0x98,&rawNav);AudioRead(static_cast<unsigned char*>(nav)+0x94,&curvature);
  AudioRead(static_cast<unsigned char*>(nav)+0x58,&road);AudioRead(static_cast<unsigned char*>(nav)+0x5C,&avoidable);
  AudioRead(static_cast<unsigned char*>(action)+0x5C,&limit);AudioRead(static_cast<unsigned char*>(row.vehicle)+0x78,&actual);
  AudioRead(primary+0xA8,&behind);AudioRead(primary+0xA7,&reverse);
  AudioRead(static_cast<unsigned char*>(nav)+0xBC,&navForward);
  const float headingNorm=std::sqrt(row.heading.x*row.heading.x+row.heading.y*row.heading.y+row.heading.z*row.heading.z);
  const float alignment=navForward.x*row.heading.x+navForward.y*row.heading.y+navForward.z*row.heading.z;
  const Vec3 aim=W2CWorldPoint(row.aim),old=W2CWorldPoint(row.nativeAim);
  Log(LogLevel::Info,"W2C_BRAKE_TRACE handle=%u native=%.2f applied=%.2f actual=%.2f native_limit=%.2f drop=%u road_occlusion=%u avoidable=%u reverse=%u behind=%u curvature=%.5f aim=%.2f,%.2f,%.2f native_aim=%.2f,%.2f,%.2f native_nav=%.2f,%.2f,%.2f nav_forward=%.3f,%.3f,%.3f heading_alignment=%.3f speed_override=0",row.handle,native,applied,actual,limit,unsigned(drop),road,avoidable,unsigned(reverse),unsigned(behind),curvature,aim.x,aim.y,aim.z,old.x,old.y,old.z,rawNav.x,rawNav.y,rawNav.z,navForward.x,navForward.y,navForward.z,alignment);
  Log(LogLevel::Info,"W2C_CONTROL_AUDIT handle=%u heading_length=%.4f curvature_source=%u issued_curvature=%.6f aim_failures=%u position=%.3f,%.3f,%.3f heading=%.5f,%.5f,%.5f route=custom speed=native actuator=native",row.handle,headingNorm,row.curveSource,row.issuedCurvature,row.aimFailures,row.position.x,row.position.y,row.position.z,row.heading.x,row.heading.y,row.heading.z);
  return;
 }
}

using W2CCurvatureFn=float(__thiscall*)(void*,const NFSPluginSDK::MW05::UMath::Vector3*,const NFSPluginSDK::MW05::UMath::Vector3*);
W2CCurvatureFn g_originalW2CCurvature=nullptr;
float __fastcall W2CCurvatureHook(void* nav,void*,const NFSPluginSDK::MW05::UMath::Vector3* position,const NFSPluginSDK::MW05::UMath::Vector3* heading){
 const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
 if(caller!=Address(0x00428C6C)||!nav)return g_originalW2CCurvature(nav,position,heading);
 void* primary=nullptr;
 if(!AudioRead(static_cast<unsigned char*>(nav)+0x84,&primary)||!BattleVtable(primary,kRacecarPrimaryVtable))return g_originalW2CCurvature(nav,position,heading);
 auto* row=W2CCommand(static_cast<unsigned char*>(primary)+0x4C,GetTickCount64());
 if(!row)return g_originalW2CCurvature(nav,position,heading);
 // AIActionRace calculates speed and its behind-target test before the final
 // SetDriveTarget hook. Both must see that same cached command, not the stale
 // native road destination left after InitPath suppression. Physical CPs,
 // native occlusion/avoidable positions and braking state remain untouched.
 const auto rawAim=W2CWorldPoint(row->aim);
 const float orderedAim[]={rawAim.x,rawAim.y,rawAim.z};
 const float orderedPosition[]={row->position.x,row->position.y,row->position.z};
 if(!SynchronizeRaceGuidanceTarget(nav,orderedAim,orderedPosition))return g_originalW2CCurvature(nav,position,heading);
 float native=g_originalW2CCurvature(nav,position,heading);
 // The provider has already selected a collision-tested, supported corridor.
 // Plan lateral speed for that issued command, including its avoidance lane,
 // instead of a raw knot angle or the abandoned native road spline. Native
 // object speed constraints, reverse/behind flags and final brakes stay live.
 // Native CalculateCurvature's second argument is linear velocity, not the
 // car's forward vector. Use the command's verified VehicleSnapshot heading,
 // so stopped racers and sliding cars retain a valid steering reference.
 float curve=0;
 const float positionXYZ[]={row->position.x,row->position.y,row->position.z},headingXYZ[]={row->heading.x,row->heading.y,row->heading.z};
 if(!RaceGuidanceCurvature(positionXYZ,headingXYZ,orderedAim,curve)){row->curveSource=2;row->issuedCurvature=0;return native;}
 row->curveSource=1;row->issuedCurvature=curve;
 static ULONGLONG next[16]{};auto now=GetTickCount64();unsigned slot=unsigned(row-g_w2cCommands);
 if(now>=next[slot]){next[slot]=now+1000;Log(LogLevel::Info,"W2C_COMMAND_CURVATURE handle=%u native=%.6f issued_aim=%.6f native_speed_planner=1 native_object_constraints=1 final_speed_override=0",row->handle,native,curve);}
 return curve;
}
void InstallW2CCurvatureHook() noexcept {
 const unsigned char entry[]={0x83,0xEC,0x2C,0x56,0x8B,0xF1,0x8B,0x46,0x08};
 const unsigned char caller[]={0xE8,0xA4,0xFC,0x34,0x00};
 const unsigned char forwardRead[]={0x8D,0x85,0xBC,0x00,0x00,0x00};
 auto address=reinterpret_cast<void*>(Address(0x00778910));
 bool ok=g_customDriveInstalled&&!memcmp(address,entry,sizeof(entry))&&!memcmp(reinterpret_cast<void*>(Address(0x00428C67)),caller,sizeof(caller))&&!memcmp(reinterpret_cast<void*>(Address(0x00414245)),forwardRead,sizeof(forwardRead));
 if(ok&&MH_CreateHook(address,W2CCurvatureHook,reinterpret_cast<void**>(&g_originalW2CCurvature))==MH_OK){
  ok=MH_EnableHook(address)==MH_OK;if(!ok)MH_RemoveHook(address);
 }else ok=false;
 Log(ok?LogLevel::Info:LogLevel::Warning,"W2C_ROUTE_CURVATURE hook=%u callerScoped=00428C6C native_vehicle_speed_model=1 avoidable_preserved=1 ordered_target_before_speed=1",unsigned(ok));
}
