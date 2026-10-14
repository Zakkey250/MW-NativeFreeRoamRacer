#include "../../World2MW/src/NeoAI/Driver.h"
#include "../../World2MW/src/NeoAI/JamWatch.h"
#include "../../World2MW/src/NeoAI/Nitrous.h"
#include "../../World2MW/src/NativeLoader/NeoBody.h"
using NeoDrivingFn=void(__thiscall*)(void*,float);
using NeoInputFn=void(__thiscall*)(void*,float);
static NeoDrivingFn neoOriginalDriving=nullptr;
static NeoInputFn neoOriginalInput[4]{};
using NeoNOSFn=void(__thiscall*)(void*,bool);
static NeoNOSFn neoOriginalNOS=nullptr;
static bool neoNitrousInstalled=false;
static bool neoInstalled=false;static DWORD neoWriter=0;
static thread_local std::uintptr_t neoDrivingCaller=0;
struct NeoRuntimeState {
 void* ai=nullptr;void* input=nullptr;void* race=nullptr;unsigned handle=0;bool owned=false;
 mwneo::Driver driver;mwneo::Guidance guidance;mwneo::Controls controls;ULONGLONG sampled=0,logged=0,activeAt=0;
 unsigned ticks=0,rejectedNative=0,guidanceFailures=0,fallbacks=0;
 unsigned powerCalls=0;float lastPowerNative=0,lastPowerBoosted=0;ULONGLONG powerLogged=0;
 mwneo::JamWatch jam{};unsigned respawns=0,respawnRefused=0;
 mwneo::Nitrous nitrous{};bool nosEquipped=false,nosEngaged=false,nosRead=false;float nosTank=0,nosBoost=0,nosRequestedSeconds=0,nosEngagedSeconds=0;
 float requested[3]{},postwrite[3]{};ULONGLONG diagnosticAt=0;unsigned diagnosticPhase=0;
 std::array<mwneo::Point,65> testPoints{};unsigned testPointCount=0;bool testPointsReady=false;float testIntegral=0,testSteering=0;
 bool brakeProbeStarted=false,brakeProbeFinished=false,brakeProbeValid=true;float brakeProbeInitial=0,brakeProbeTime=0,brakeProbeGrade=0;
};
static NeoRuntimeState neoRuntime[16];
static bool NeoLocalPlayer();
#include "MWNeoOwnership.inl"
static unsigned NeoSelectedController(){
 auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");using Get=unsigned(__cdecl*)();
 auto fn=module?reinterpret_cast<Get>(GetProcAddress(module,"W2MW_RaceControllerV1")):nullptr;return fn?fn():1;
}
static bool NeoLocalPlayer(){
 auto events=GetModuleHandleW(L"NFSMWEventLoader.asi");using LocalTest=unsigned(__cdecl*)();
 auto local=events?reinterpret_cast<LocalTest>(GetProcAddress(events,"MWEL_LocalRaceTestV1")):nullptr;
 if(local&&local())return true;
 auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");using Get=unsigned(__cdecl*)();
 auto fn=module?reinterpret_cast<Get>(GetProcAddress(module,"W2MW_LocalRaceTestV1")):nullptr;return fn&&fn();
}
static NeoRuntimeState* NeoOwned(void* ai){for(auto& s:neoRuntime)if(s.ai==ai&&s.owned&&GetTickCount64()-s.activeAt<250)return &s;return nullptr;}
static void NeoLogNativeUpgradeLevels(void* vehicle,unsigned handle){
 const unsigned char guard[]={0x6A,0xFF,0x68,0x38,0xA9,0x87,0x00};
 if(memcmp(reinterpret_cast<void*>(Address(0x672D30)),guard,sizeof(guard)))return;
 __try {
  const unsigned char nameGuard[]={0x8B,0x41,0x2C,0x8B,0x40,0x24,0xC3};
  if(!memcmp(reinterpret_cast<void*>(Address(0x688090)),nameGuard,sizeof(nameGuard))){
   const char* model=reinterpret_cast<const char*(__thiscall*)(void*)>(Address(0x688090))(vehicle);
   if(model&&strnlen_s(model,64)<64)Log(LogLevel::Info,"MW_NEOAI_NATIVE_MODEL handle=%u physical_model=%s",handle,model);
  }
  // IVehicle::GetVehicleAttributes (688070) returns this exact +24 surface.
  // Read effective attribute upgrade levels even when no customization
  // record exists (the no-save player uses native performance matching).
  auto& attributes=*reinterpret_cast<const NFSPluginSDK::MW05::Attrib::Gen::pvehicle*>(static_cast<unsigned char*>(vehicle)+0x24);
  unsigned levels[7]{};
  for(unsigned i=0;i<7;++i)levels[i]=unsigned(NFSPluginSDK::MW05::Physics::Upgrades::Package::GetLevel(attributes,NFSPluginSDK::MW05::Physics::Upgrades::Type(i)));
  Log(LogLevel::Info,"MW_NEOAI_EFFECTIVE_UPGRADES handle=%u native_attribute_levels=1 tires=%u brakes=%u chassis=%u transmission=%u engine=%u induction=%u nos=%u",handle,levels[0],levels[1],levels[2],levels[3],levels[4],levels[5],levels[6]);
 }__except(EXCEPTION_EXECUTE_HANDLER){Log(LogLevel::Warning,"MW_NEOAI_EFFECTIVE_UPGRADES handle=%u read_failed=1",handle);}
}
static void (__cdecl* neoOriginalEngineOutput)()=nullptr;
static bool neoEngineOutputInstalled=false;
static void __cdecl NeoBoostEngineOutput(void* engine,float* output){
 // EngineRacer's primary object owns the vehicle at +48; its verified
 // IEngine interface is +54. Match that vehicle's exact acquired AI owner.
 void* vehicle=nullptr;void* ai=nullptr;
 if(!engine||!output||(!BattleVtable(static_cast<unsigned char*>(engine)+0x54,0x8AB6E0)&&!BattleVtable(static_cast<unsigned char*>(engine)+0x54,0x8ABF88))||
    !AudioRead(static_cast<unsigned char*>(engine)+0x48,&vehicle)||!vehicle||!AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&ai))return;
 auto* row=NeoOwned(ai);if(!row)return;
 float native=*output,result=race_power::NeoOutput(native,true);if(result==native)return;
 *output=result;++row->powerCalls;row->lastPowerNative=native;row->lastPowerBoosted=result;
}
// 006B002A follows the stock positive-output catchup calculation. Preserve
// all registers/flags and replay the displaced mov ecx,[esi+84] via MinHook.
static void __declspec(naked) NeoEngineOutputHook(){
 __asm {
  pushfd
  pushad
  mov ebx,esp
  and esp,0FFFFFFF0h
  sub esp,512
  fxsave [esp]
  lea eax,[ebx+64]
  push eax
  push esi
  call NeoBoostEngineOutput
  add esp,8
  fxrstor [esp]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [neoOriginalEngineOutput]
 }
}
static bool InstallNeoEngineOutput(){
 const unsigned char guard[]={0x8B,0x8E,0x84,0x00,0x00,0x00,0x51,0x8B,0xCE,0xE8};
 auto target=reinterpret_cast<void*>(Address(0x6B002A));
 if(memcmp(target,guard,sizeof(guard))){Log(LogLevel::Warning,"MW_NEOAI_POWER_REFUSED engine_output_guard=006B002A");return false;}
 bool ok=MH_CreateHook(target,&NeoEngineOutputHook,reinterpret_cast<void**>(&neoOriginalEngineOutput))==MH_OK;
 if(ok&&MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);ok=false;}
 Log(LogLevel::Info,"MW_NEOAI_POWER installed=%u constant_scale=2.000 positive_engine_output=1 owned_racers_and_local_player=1",unsigned(ok));return ok;
}
static bool NeoRoadBlocked(void* nav){
 void* primary=nullptr;return nav&&AudioRead(static_cast<unsigned char*>(nav)+0x84,&primary)&&BattleVtable(primary,kRacecarPrimaryVtable)&&NeoOwned(static_cast<unsigned char*>(primary)+0x4C);
}
static void NeoInputDispatch(unsigned channel,void* input,float value,std::uintptr_t caller){
 NeoDiagnosticWriter(input,channel,value,caller);
 if(neoWriter!=GetCurrentThreadId())for(auto& row:neoRuntime)if(row.owned&&row.input==input&&GetTickCount64()-row.activeAt<250){
  ++row.rejectedNative;return;
 }
 neoOriginalInput[channel](input,value);
}
static void __fastcall NeoSteering(void* p,void*,float v){NeoInputDispatch(0,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoGas(void* p,void*,float v){NeoInputDispatch(1,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoBrake(void* p,void*,float v){NeoInputDispatch(2,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoHandBrake(void* p,void*,float v){NeoInputDispatch(3,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoNOS(void* input,void*,bool pressed){
 if(neoWriter!=GetCurrentThreadId())for(auto& row:neoRuntime)if(row.owned&&row.input==input&&GetTickCount64()-row.activeAt<250)return;
 neoOriginalNOS(input,pressed);
}
static bool InstallNeoNitrous(){
 const unsigned char setter[]={0x8A,0x44,0x24,0x04,0x88,0x41,0x29,0xC2,0x04,0x00};
 auto address=reinterpret_cast<void*>(Address(0x69CC40));
 if(memcmp(address,setter,sizeof(setter)))return false;
 bool ok=MH_CreateHook(address,&NeoNOS,reinterpret_cast<void**>(&neoOriginalNOS))==MH_OK;
 if(ok&&MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);ok=false;neoOriginalNOS=nullptr;}
 Log(LogLevel::Info,"MW_NEOAI_NITROUS installed=%u native_finite_tank=1 automatic_refill=0 owned_race_only=1",unsigned(ok));return ok;
}
static bool NeoReadNitrous(void* primary,bool& equipped,float& tank,bool& engaged,float& boost){
 static_assert(offsetof(NFSPluginSDK::MW05::AIVehicle,mIEngine)==0xDC);
 void* engine=nullptr;
 if(!AudioRead(static_cast<BYTE*>(primary)+0xDC,&engine)||
    (!BattleVtable(engine,0x8AB6E0)&&!BattleVtable(engine,0x8ABF88)))return false;
 auto table=*reinterpret_cast<void***>(engine);
 if(table[7]!=reinterpret_cast<void*>(Address(0x6A03F0))||table[8]!=reinterpret_cast<void*>(Address(0x6A0410))||table[10]!=reinterpret_cast<void*>(Address(0x6A0400))||table[11]!=reinterpret_cast<void*>(Address(0x6A0430)))return false;
 equipped=reinterpret_cast<bool(__thiscall*)(void*)>(table[11])(engine);
 tank=reinterpret_cast<float(__thiscall*)(void*)>(table[7])(engine);
 engaged=reinterpret_cast<bool(__thiscall*)(void*)>(table[8])(engine);
 boost=reinterpret_cast<float(__thiscall*)(void*)>(table[10])(engine);
 return std::isfinite(tank)&&tank>=0&&tank<=1.01f&&std::isfinite(boost);
}
static bool NeoDrive(void* primary,float dt){
 if(!neoInstalled||NeoSelectedController()!=2)return false;
 auto* ai=static_cast<unsigned char*>(primary)+0x4C;VehicleSnapshot racer{},player{};
 // Native stuck/avoidance actions can temporarily replace AIActionRace.
 // Once acquired, retain ownership while exact vehicle/race membership is
 // valid; changing a native sub-action is not permission to resume native AI.
 unsigned scopeFailure=0;
 if(!W2CRaceVehicle(ai,racer,player,NeoLocalPlayer(),NeoOwned(ai)==nullptr,&scopeFailure)){for(auto& s:neoRuntime)if(s.ai==ai&&s.owned){if(neoNitrousInstalled&&s.input)neoOriginalNOS(s.input,false);Log(LogLevel::Info,"MW_NEOAI_RELEASE handle=%u reason=out_of_race_scope ticks=%u scope_failure=%u driver_class=%u",s.handle,s.ticks,scopeFailure,racer.driverClass);s.owned=false;}return false;}
 void* input=nullptr;void* transmission=nullptr;void* race=nullptr;
 if(!AudioRead(static_cast<unsigned char*>(primary)+0xE0,&input)||!BattleVtable(input,racer.driverClass==kDriverHuman?0x8AC670:0x8AB5C8)||
    !AudioRead(static_cast<unsigned char*>(primary)+0xD4,&transmission)||!transmission||
    !AudioRead(reinterpret_cast<void*>(Address(0x91E000)),&race))return false;
 unsigned handle=W2CRaceHandle(racer);NeoBodyMotion body;if(!NeoReadBody(static_cast<BYTE*>(racer.pointer),body))return false;
 mwneo::Point pos{body.position[0],body.position[1],body.position[2]},heading{body.heading[0],body.heading[1],body.heading[2]};
 mwneo::Vehicle v;v.handle=handle;v.position=pos;v.heading=heading;v.velocity={body.velocity[0],body.velocity[1],body.velocity[2]};v.speed=mwneo::LengthXZ(v.velocity);v.dt=dt;v.halfWidth=body.halfWidth;v.halfLength=body.halfLength;
 NeoRuntimeState* selected=nullptr;
 for(auto& s:neoRuntime)if(s.ai==ai){selected=&s;break;}
 if(!selected)for(auto& s:neoRuntime)if(!s.owned){selected=&s;break;}if(!selected)return false;
 auto& row=*selected;
 if(row.ai!=ai||row.input!=input||row.handle!=handle||row.race!=race){row={};row.ai=ai;row.input=input;row.handle=handle;row.race=race;}
 auto now=GetTickCount64();bool wasOwned=row.owned;
 v.steering=row.controls.steering;
 v.steeringGain=row.driver.response.gain;
 v.recovery=row.driver.recovery;
 bool diagnostic=NeoDiagnosticTarget(row);
 if(diagnostic&&!row.diagnosticAt)row.diagnosticAt=now;
 if(diagnostic)NeoDiagnosticOrder(row,"NEO_UPDATE_BEGIN",neoDrivingCaller);
 if((!diagnostic||neoDiagnosticMode==3||neoDiagnosticMode==6)&&(!row.sampled||now-row.sampled>=50)){
  using Query=unsigned(__cdecl*)(const mwneo::Vehicle*,mwneo::Guidance*);
  auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");auto query=reinterpret_cast<Query>(GetProcAddress(module,"W2MW_NeoGuidanceV1"));
  mwneo::Guidance next;unsigned status=query?query(&v,&next):0;
  if(!status){if(!wasOwned)return false;++row.guidanceFailures;row.guidance.status=2;}
  else row.guidance=next;row.sampled=now;
 }
 row.owned=true;row.activeAt=now;
 mwneo::Profile profile;using GetProfile=unsigned(__cdecl*)(mwneo::Profile*,unsigned);auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");
 auto getProfile=reinterpret_cast<GetProfile>(GetProcAddress(module,"W2MW_NeoProfileV1"));if(getProfile)getProfile(&profile,sizeof(profile));
 if(diagnostic&&neoDiagnosticMode!=3)NeoDiagnosticControls(row,v,now);else row.controls=row.driver.Update(v,row.guidance,profile);
 if(!row.controls.valid){row.controls={};row.controls.brake=.4f;++row.guidanceFailures;}
 if(!diagnostic&&row.jam.Update(v,row.guidance,row.driver.recovery)){
  using Recover=unsigned(__cdecl*)(const mwneo::Vehicle*,void*);
  auto recover=reinterpret_cast<Recover>(GetProcAddress(module,"W2MW_NeoRecoverV1"));
  unsigned result=recover?recover(&v,ai):0;float age=row.jam.age;row.jam.Attempted();
  // The bridge returns 2 when native placement was attempted but the
  // measured pose/CP verification failed. Do not clear the unresolved jam
  // or count such an attempt as a successful recovery.
  if(result==1){
   ++row.respawns;auto response=row.driver.response;response.ClearWindow();response.lastSteering=0;
   auto brakes=row.driver.brakes;brakes.ClearWindow();brakes.ready=false;
   row.driver.Reset();row.driver.response=response;row.driver.brakes=brakes;row.driver.recovery=mwneo::Recovery::Rejoin;row.jam.Reset();row.sampled=0;
   row.controls={};row.controls.brake=.4f;row.controls.valid=true;
  }else {++row.respawnRefused;if(result==2)row.sampled=0;}
  Log(LogLevel::Info,"MW_NEOAI_RECOVERY_RESULT handle=%u stalled_physics_seconds=%.3f native_respawn_result=%u respawns=%u refused=%u native_driving_handoff=0",handle,age,result,row.respawns,row.respawnRefused);
  if(result){
   // The old pose/guidance belong to the jam location. Stop this tick and
   // query the measured post-respawn pose before applying any throttle.
   neoOriginalInput[0](input,0);neoOriginalInput[1](input,0);neoOriginalInput[2](input,.4f);neoOriginalInput[3](input,0);
   if(neoNitrousInstalled)neoOriginalNOS(input,false);row.nitrous.pressed=false;
   ++row.ticks;return true;
  }
 }
 // Native reverse override timer has no authority over an owned driver's gear.
 *reinterpret_cast<float*>(static_cast<unsigned char*>(primary)+0x94)=0;
 auto table=*reinterpret_cast<void***>(transmission);
 bool reverse=reinterpret_cast<bool(__thiscall*)(void*)>(table[5])(transmission);
 if(reverse!=row.controls.reverse){
  // First brake to a stop. Applying the forward controller's throttle while
  // still in R makes the car accelerate backwards and prevents the shift.
  if(std::abs(v.speed)<1){
   reinterpret_cast<bool(__thiscall*)(void*,int)>(table[3])(transmission,row.controls.reverse?0:2);
   reverse=reinterpret_cast<bool(__thiscall*)(void*)>(table[5])(transmission);
   row.driver.integral=0;row.driver.effort=0;
  }
  row.controls.throttle=0;row.controls.brake=reverse!=row.controls.reverse?.65f:0;
 }
 neoWriter=GetCurrentThreadId();
 // MW's IInput steering sign is opposite to the raw X/Z bicycle model.
 // Verified from live heading delta for both Human and Racecar inputs.
 neoOriginalInput[0](input,-row.controls.steering);
 neoOriginalInput[1](input,row.controls.throttle);
 neoOriginalInput[2](input,row.controls.brake);
 neoOriginalInput[3](input,0);
 if(neoNitrousInstalled){
  row.nosRead=NeoReadNitrous(primary,row.nosEquipped,row.nosTank,row.nosEngaged,row.nosBoost);
  bool pressed=!diagnostic&&row.nosRead&&row.nitrous.Update(v,row.guidance,profile,row.controls,row.nosEquipped,row.nosTank);
  row.nitrous.pressed=pressed;neoOriginalNOS(input,pressed);
  if(pressed)row.nosRequestedSeconds+=std::clamp(dt,0.f,.1f);
  if(row.nosRead&&row.nosEngaged)row.nosEngagedSeconds+=std::clamp(dt,0.f,.1f);
 }
 neoWriter=0;++row.ticks;
 if(diagnostic){row.requested[0]=-row.controls.steering;row.requested[1]=row.controls.throttle;row.requested[2]=row.controls.brake;NeoDiagnosticRead(input,row.postwrite);NeoDiagnosticOrder(row,"NEO_POSTWRITE",neoDrivingCaller);}
 if(diagnostic&&now-row.diagnosticAt<NeoDiagnosticWindow())Log(LogLevel::Info,"NEO_RESPONSE seq=%llu tick=%u vehicle=%u phase=%u dt=%.6f speed=%.6f heading=%.6f,%.6f,%.6f position=%.6f,%.6f,%.6f velocity=%.6f,%.6f,%.6f gear=%u transmission_reverse=%u requested_reverse=%u steer=%.6f gas=%.6f brake=%.6f target=%.6f status=%u",++neoDiagnosticSequence,row.ticks,row.handle,row.diagnosticPhase,dt,v.speed,heading.x,heading.y,heading.z,pos.x,pos.y,pos.z,v.velocity.x,v.velocity.y,v.velocity.z,reinterpret_cast<unsigned(__thiscall*)(void*)>(table[1])(transmission),unsigned(reverse),unsigned(row.controls.reverse),row.requested[0],row.requested[1],row.requested[2],row.controls.targetSpeed,row.guidance.status);
 if(!wasOwned){
  W2CReleaseNativeQuery(ai);Log(LogLevel::Info,"MW_NEOAI_ACQUIRE handle=%u event=%08X route=NeoAI speed=NeoAI actuator=NeoAI recovery=NeoAI native_physics=1",handle,row.guidance.event);
  // Read the same native customization package returned by IVehicle slot 50
  // (688260: mov eax,[ecx+38]). Do not infer upgrades from the model name.
  void* customization=nullptr;std::array<unsigned,8> package{};
  const bool packageRead=AudioRead(static_cast<unsigned char*>(racer.pointer)+0x38,&customization)&&customization&&
   AudioRead(static_cast<unsigned char*>(customization)+offsetof(NFSPluginSDK::MW05::FECustomizationRecord,mInstalledPhysics),&package);
  Log(LogLevel::Info,"MW_NEOAI_VEHICLE_SETUP handle=%u customization_read=%u tires=%u brakes=%u chassis=%u transmission=%u engine=%u induction=%u nos=%u junkman=%u",handle,unsigned(packageRead),package[0],package[1],package[2],package[3],package[4],package[5],package[6],package[7]);
  NeoLogNativeUpgradeLevels(racer.pointer,handle);
 }
 if(now>=row.logged){row.logged=now+250;auto planningProfile=profile;planningProfile.brakeDecel=row.driver.brakes.PlanningDecel(profile);auto speedDiagnostic=mwneo::PlanSpeed(row.guidance,v,planningProfile);
  auto travel=mwneo::UnitXZ(v.velocity),facing=mwneo::UnitXZ(v.heading);
  float slip=v.speed>2?std::atan2(mwneo::CrossXZ(facing,travel),mwneo::DotXZ(facing,travel)):0;
  Log(LogLevel::Info,"MW_NEOAI_MOTION handle=%u CP=%u progress=%.2f velocity=%.4f,%.4f,%.4f slip=%.5f yaw_rate=%.5f",handle,row.guidance.edge,row.guidance.progress,v.velocity.x,v.velocity.y,v.velocity.z,slip,row.driver.yawRate);
  if(now>=row.powerLogged){row.powerLogged=now+1000;Log(LogLevel::Info,"MW_NEOAI_POWER_SAMPLE handle=%u scale=2.000 applied_calls=%u native=%.3f boosted=%.3f",handle,row.powerCalls,row.lastPowerNative,row.lastPowerBoosted);}
  Log(LogLevel::Info,"MW_NEOAI_AVOIDANCE handle=%u predictive=%u road_curve=%.5f steering_gain=%.3f steering_windows=%u brake_gain=%.3f brake_windows=%u preview_brake=%u planning_decel=%.3f",handle,row.guidance.predictiveAvoidance,row.guidance.samples[0].roadCurvature,row.driver.response.gain,row.driver.response.windows,row.driver.brakes.gain,row.driver.brakes.windows,unsigned(row.driver.previewBraking),row.driver.brakes.PlanningDecel(profile));
  if(neoNitrousInstalled)Log(LogLevel::Info,"MW_NEOAI_NITROUS_SAMPLE handle=%u read=%u equipped=%u requested=%u engaged=%u tank=%.4f boost=%.4f requested_seconds=%.3f engaged_seconds=%.3f",handle,unsigned(row.nosRead),unsigned(row.nosEquipped),unsigned(row.nitrous.pressed),unsigned(row.nosEngaged),row.nosTank,row.nosBoost,row.nosRequestedSeconds,row.nosEngagedSeconds);
  Log(LogLevel::Info,"MW_NEOAI_CONTROL handle=%u event=%08X CP=%u progress=%.2f speed=%.2f target=%.2f steering=%.3f gas=%.3f brake=%.3f state=%u status=%u curve=%.5f heading_error=%.3f position=%.2f,%.2f,%.2f heading=%.4f,%.4f,%.4f obstacle=%.2f lead=%.2f ticks=%u native_rejected=%u guidance_failures=%u fallbacks=%u gear=%u reverse_request=%u rear_clear=%u dt=%.5f recovery_time=%.3f travel_heading=%.4f,%.4f,%.4f native_speed=%.2f dimension=%.3f,%.3f,%.3f aim_distance=%.2f plan_curve=%.5f speed_constraint=%u constraint_distance=%.2f constraint_curve=%.5f traffic_push=%u",handle,row.guidance.event,row.guidance.edge,row.guidance.progress,v.speed,row.controls.targetSpeed,row.controls.steering,row.controls.throttle,row.controls.brake,unsigned(row.controls.recovery),row.guidance.status,row.controls.curvature,row.controls.headingError,pos.x,pos.y,pos.z,heading.x,heading.y,heading.z,row.guidance.obstacleDistance,row.guidance.leadSpeed,row.ticks,row.rejectedNative,row.guidanceFailures,row.fallbacks,reinterpret_cast<unsigned(__thiscall*)(void*)>(table[1])(transmission),unsigned(row.controls.reverse),row.guidance.rearClear,dt,row.driver.stateTime,racer.heading.y,racer.heading.z,racer.heading.x,racer.speed,body.dimension[0],body.dimension[1],body.dimension[2],mwneo::LengthXZ(mwneo::Sub(row.guidance.aim,v.position)),speedDiagnostic.maxCurvature,speedDiagnostic.constraint,speedDiagnostic.constraintDistance,speedDiagnostic.constraintCurve,unsigned(row.guidance.leadVehicle==mwneo::RaceTrafficPush));
 }
 return true;
}
static bool NeoOwnedFailure(void* primary){
 auto* ai=static_cast<unsigned char*>(primary)+0x4C;
 for(auto& s:neoRuntime)if(s.ai==ai&&s.owned){
  ++s.guidanceFailures;s.activeAt=GetTickCount64();neoWriter=0;
  // An owned controller fault cannot silently hand steering back to native AI.
  if(s.guidanceFailures==1||s.guidanceFailures%120==0)Log(LogLevel::Error,"MW_NEOAI_SAFE_STOP handle=%u reason=controller_fault failures=%u",s.handle,s.guidanceFailures);
  __try{neoOriginalInput[1](s.input,0);neoOriginalInput[2](s.input,.5f);neoOriginalInput[3](s.input,0);if(neoNitrousInstalled)neoOriginalNOS(s.input,false);}__except(EXCEPTION_EXECUTE_HANDLER){}
  return true;
 }return false;
}
static bool NeoDriveGuarded(void* primary,float dt){__try{if(NeoDrive(primary,dt))return true;return NeoOwnedFailure(primary);}__except(EXCEPTION_EXECUTE_HANDLER){return NeoOwnedFailure(primary);}}
static void __fastcall NeoDriving(void* primary,void*,float dt){neoDrivingCaller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());if(!NeoDriveGuarded(primary,dt))neoOriginalDriving(primary,dt);}
static void InstallNeoAI(){
 auto eventLoader=GetModuleHandleW(L"NFSMWEventLoader.asi");using Owner=unsigned(__cdecl*)();auto owner=eventLoader?reinterpret_cast<Owner>(GetProcAddress(eventLoader,"MWEL_NeoDriverOwnerV1")):nullptr;
 // The event module may still be validating packages. Its driver export
 // declares ownership; do not race its installation and steal input hooks
 // merely because its installed flag is not set yet.
 if(owner){Log(LogLevel::Info,"MW_NEOAI_DELEGATED owner=NFSMWEventLoader installed=%u native_vehicle_scope_provider=NFRR legacy_hooks_skipped=1",owner());return;}
 const unsigned addresses[]={0x69CB80,0x69CB90,0x69CBA0,0x69CBC0};
 const unsigned char offsets[]={0x0C,0x1C,0x20,0x24};
 for(unsigned i=0;i<4;++i){unsigned char expected[]={0x8B,0x44,0x24,0x04,0x89,0x41,offsets[i],0xC2,0x04,0x00};if(memcmp(reinterpret_cast<void*>(Address(addresses[i])),expected,10)){Log(LogLevel::Warning,"MW_NEOAI_REFUSED input_abi=%08X",addresses[i]);return;}}
 const unsigned char driving[]={0x56,0x57,0x8B,0x7C,0x24,0x0C,0x8B,0xF1,0x8B,0x06,0x57};
 if(memcmp(reinterpret_cast<void*>(Address(0x408690)),driving,sizeof(driving))||*reinterpret_cast<unsigned*>(Address(0x892720+17*4))!=Address(0x408690)){Log(LogLevel::Warning,"MW_NEOAI_REFUSED driver_abi=1");return;}
 void* detours[]={&NeoSteering,&NeoGas,&NeoBrake,&NeoHandBrake};unsigned created=0;bool ok=true;
 for(unsigned i=0;i<4;++i){if(MH_CreateHook(reinterpret_cast<void*>(Address(addresses[i])),detours[i],reinterpret_cast<void**>(&neoOriginalInput[i]))!=MH_OK){ok=false;break;}++created;}
 if(ok)ok=MH_CreateHook(reinterpret_cast<void*>(Address(0x408690)),&NeoDriving,reinterpret_cast<void**>(&neoOriginalDriving))==MH_OK;
 if(ok){for(unsigned i=0;i<4;++i)ok=MH_EnableHook(reinterpret_cast<void*>(Address(addresses[i])))==MH_OK&&ok;ok=MH_EnableHook(reinterpret_cast<void*>(Address(0x408690)))==MH_OK&&ok;}
 if(!ok){for(unsigned i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(Address(addresses[i])));MH_RemoveHook(reinterpret_cast<void*>(Address(addresses[i])));}if(neoOriginalDriving){MH_DisableHook(reinterpret_cast<void*>(Address(0x408690)));MH_RemoveHook(reinterpret_cast<void*>(Address(0x408690)));}}
 neoInstalled=ok;Log(LogLevel::Info,"MW_NEOAI_INSTALLED=%u setter_guards=1 final_consumer_verified=0 custom_event_opponents_only=1",unsigned(ok));
 if(ok){InstallNeoOwnershipDiagnostic();if(NeoSelectedController()==2){neoEngineOutputInstalled=InstallNeoEngineOutput();neoNitrousInstalled=InstallNeoNitrous();}}
}
