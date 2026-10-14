#pragma once
#include "SpeedPlanner.h"
namespace mwneo {
// Use the game's finite tank and recharge. No additional power multiplier,
// refill, physics or performance-package writes are made here.
struct Nitrous {
 bool pressed=false;
 bool Update(const Vehicle& v,const Guidance& g,const Profile& p,const Controls& c,bool equipped,float tank){
  const auto plan=PlanSpeed(g,v,p);
  // Accumulate a usable burst after depletion instead of tapping NOS every
  // physics tick as soon as a tiny amount has recharged.
  bool eligible=equipped&&std::isfinite(tank)&&tank>(pressed?.01f:.12f)&&c.valid&&plan.valid&&
   g.status==1&&g.confidence>=.8f&&c.recovery==Recovery::Normal&&!c.reverse&&
   c.throttle>.95f&&c.brake<.01f&&v.speed>12&&
   c.targetSpeed-v.speed>(pressed?2.f:5.f)&&fabs(c.headingError)<.18f&&
   fabs(c.steering)<.18f&&DotXZ(UnitXZ(v.heading),UnitXZ(v.velocity))>.985f;
  // Do not spend a tank accelerating toward an imminent braking point.
  // On a long straight, a distant corner outside the reaction window is OK.
  if(plan.constraint==1&&plan.constraintDistance>0&&
     plan.constraintDistance<25.f+v.speed*1.2f)eligible=false;
  if(g.leadVehicle==RaceTrafficPush&&g.obstacleDistance<12)eligible=false;
  pressed=eligible;return pressed;
 }
};
}
