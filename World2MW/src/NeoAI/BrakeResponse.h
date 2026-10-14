#pragma once
#include "Types.h"
namespace mwneo {
// Pedal authority is car-specific. Learn only from clear, aligned forward
// braking; do not infer authority from collisions, recovery or traffic contact.
// Preview uses at most half the verified pedal authority, capped at three
// times the base model. Unmeasured cars keep the original braking envelope.
struct BrakeResponse {
 float gain=1,time=0,pedalTime=0,speedLoss=0,gradeTime=0,lastSpeed=0;
 unsigned windows=0;bool ready=false;
 void ClearWindow(){time=pedalTime=speedLoss=gradeTime=0;}
 void Observe(const Vehicle& v,const Guidance& g,const Profile& p,float previousBrake,bool normal){
  auto h=UnitXZ(v.heading),travel=UnitXZ(v.velocity);
  bool eligible=normal&&g.status==1&&g.confidence>=.8f&&!g.predictiveAvoidance&&
   g.obstacleDistance>60&&v.speed>12&&v.speed<90&&previousBrake>=.12f&&previousBrake<=.8f&&
   fabs(v.steering)<.15f&&v.speed*v.speed*fabs(std::tan(v.steering*p.steeringLock)/p.wheelbase)<12&&
   fabs(v.heading.y)<.12f&&DotXZ(h,travel)>.99f&&
   std::isfinite(v.dt)&&v.dt>=.001f&&v.dt<=.05f&&std::isfinite(v.speed)&&
   std::isfinite(p.brakeDecel)&&p.brakeDecel>0;
  if(!eligible){ClearWindow();ready=false;return;}
  if(!ready){lastSpeed=v.speed;ready=true;return;}
  time+=v.dt;pedalTime+=previousBrake*v.dt;
  speedLoss+=lastSpeed-v.speed;gradeTime+=v.heading.y*v.dt;lastSpeed=v.speed;
  if(time<.15f)return;
  // Subtract a modest drag estimate rather than attributing all coasting
  // deceleration to the pedal. Underestimating brake authority is preferable.
  float authority=(speedLoss-9.81f*gradeTime-(.5f+.001f*v.speed*v.speed)*time)/pedalTime;
  if(std::isfinite(authority)&&authority>=p.brakeDecel*.7f&&authority<120){
   gain+=.3f*(std::clamp(authority/p.brakeDecel,1.f,6.f)-gain);++windows;
  }
  ClearWindow();
 }
 float PlanningDecel(const Profile& p)const{
  if(windows<3||!std::isfinite(gain))return p.brakeDecel;
  return std::clamp(p.brakeDecel*gain*.5f,p.brakeDecel,p.brakeDecel*3.f);
 }
 float PedalDecel(const Profile& p)const{return p.brakeDecel*(windows>=3?gain:1.f);}
};
}
