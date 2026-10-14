#pragma once
#include "Types.h"
namespace mwneo {
// Learn the car/input channel's steering response only in light, steady turns.
// Chassis yaw is integrated over a window to tolerate quantized pose updates.
// Skidding, contact, recovery and large lateral demand never train the model.
struct SteeringResponse {
 float gain=1,time=0,expected=0,measured=0,lastSteering=0;unsigned windows=0;
 void ClearWindow(){time=expected=measured=0;}
 void Observe(const Vehicle& v,const Guidance& g,const Profile& p,float deltaYaw,bool normal){
  auto h=UnitXZ(v.heading),travel=UnitXZ(v.velocity);
  float model=std::tan(v.steering*p.steeringLock)/p.wheelbase;
  bool eligible=normal&&g.status==1&&g.confidence>=.8f&&!g.predictiveAvoidance&&g.obstacleDistance>60&&
   v.speed>=12&&v.speed<=85&&fabs(v.steering)>=.01f&&fabs(v.steering)<=.22f&&
   v.speed*v.speed*fabs(model)<16&&DotXZ(h,travel)>.99f&&fabs(CrossXZ(h,travel))<.10f&&
   fabs(deltaYaw)<.18f&&fabs(v.steering-lastSteering)<.04f;
  lastSteering=v.steering;
  if(!eligible||(expected*model<0)){ClearWindow();return;}
  time+=v.dt;expected+=v.speed*model*v.dt;measured+=deltaYaw;
  if(time<.5f)return;
  if(fabs(expected)>.08f&&measured*expected>0){
   float ratio=measured/expected;
   if(std::isfinite(ratio)&&ratio>.35f&&ratio<1.8f){gain+=.2f*(std::clamp(ratio,.55f,1.4f)-gain);++windows;}
  }
  ClearWindow();
 }
};
}
