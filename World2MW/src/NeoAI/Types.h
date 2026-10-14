#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mwneo {
constexpr uint32_t AbiVersion=5,MaxSamples=64;
struct Point {float x=0,y=0,z=0;};
inline Point Sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float DotXZ(Point a,Point b){return a.x*b.x+a.z*b.z;}
inline float CrossXZ(Point a,Point b){return a.x*b.z-a.z*b.x;}
inline float LengthXZ(Point a){return std::hypot(a.x,a.z);}
inline float Distance(Point a,Point b){auto d=Sub(a,b);return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
inline bool Finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline Point UnitXZ(Point p){float n=LengthXZ(p);return n>.001f?Point{p.x/n,0,p.z/n}:Point{};}
inline float Approach(float value,float target,float step){return value+std::clamp(target-value,-step,step);}
enum RouteFlags:uint32_t {Main=1,SubRoute=2,Supplement=4,Shortcut=8,Branch=16,Merge=32,ReverseForbidden=64,Narrow=128,Hazard=256,LocalCorrection=512,CheckedExecution=1024,PlanEnd=2048};
enum class Recovery:uint32_t {Normal,DetectingStuck,Reverse,Realign,Rejoin};
constexpr uint32_t RaceTrafficPush=2; // leadVehicle policy: contact is permitted in a race.
constexpr uint32_t PendingGateReacquire=4; // predictiveAvoidance bit; native CP still pending after physically passing its plane.
constexpr uint32_t AuthoredObstacleAvoidance=8; // Fixed editor volumes require a physically feasible detour speed.
inline bool LimitSteeringArc(uint32_t avoidance){return !avoidance||(avoidance&AuthoredObstacleAvoidance);}
struct Vehicle {uint32_t size=sizeof(Vehicle),version=AbiVersion,handle=0;Point position{},heading{},velocity{};float speed=0,halfWidth=1,dt=.016f,halfLength=2.5f,steering=0,steeringGain=1;Recovery recovery=Recovery::Normal;};
struct Sample {Point position{};float distance=0,widthLeft=3,widthRight=3;uint32_t flags=Main|ReverseForbidden;float roadCurvature=-1;};
// POD ABI shared by the loader's geometry provider and the independent driver.
// status 0 = not eligible, 1 = guidance, 2 = owned but temporarily blocked.
struct Guidance {
 uint32_t size=sizeof(Guidance),version=AbiVersion,status=0,event=0,lap=0,edge=0,branch=0,count=0;
 float progress=0,lateralError=0,laneOffset=0,confidence=0,obstacleDistance=10000,leadSpeed=0;
 uint32_t leadVehicle=0,rearClear=0;float rearDistance=0,reverseSteering=0,maneuverSteering=0,maneuverSpeed=0;Point aim{},forward{};uint32_t predictiveAvoidance=0;std::array<Sample,MaxSamples> samples{};
};
struct Profile {
 float maxSpeed=330.f/3.6f,lateralAccel=28.f,brakeDecel=12,driveAccel=8;
 float wheelbase=2.65f,steeringLock=.55f,lookaheadBase=10,lookaheadTime=.95f;
};
struct Controls {float steering=0,throttle=0,brake=0,targetSpeed=0,curvature=0,headingError=0;Recovery recovery=Recovery::Normal;bool reverse=false,valid=false;};
inline float SteeringHorizon(float speed,const Profile& p){return std::clamp(p.lookaheadBase+std::abs(speed)*p.lookaheadTime,10.f,120.f);}
inline float SpeedHorizon(float speed){return std::clamp(70.f+std::abs(speed)*2.2f,70.f,320.f);}
inline float ObstacleHorizon(float speed){return std::clamp(20.f+std::abs(speed)*1.5f,20.f,180.f);}
constexpr float SteeringTransientReserve=1.25f;
// Mechanical lock alone permits hundreds of m/s² at race speed. Bound the
// requested wheel angle by the same lateral envelope used for road speed.
// The transient reserve permits passing corrections without a traffic brake.
inline float SteeringLimit(float speed,const Profile& p,float gain){
 float mechanical=std::clamp(1.15f-std::abs(speed)*.008f,.35f,1.f);
 if(std::abs(speed)<8)return mechanical;
 float curve=p.lateralAccel*SteeringTransientReserve/(speed*speed);
 return std::min(mechanical,std::atan(p.wheelbase*curve/std::clamp(gain,.35f,1.8f))/p.steeringLock);
}
inline float Curve(Point a,Point b,Point c){Point ab=Sub(b,a),bc=Sub(c,b),ac=Sub(c,a);float den=LengthXZ(ab)*LengthXZ(bc)*LengthXZ(ac);return den>.01f?2*std::abs(CrossXZ(ab,bc))/den:0;}
}
