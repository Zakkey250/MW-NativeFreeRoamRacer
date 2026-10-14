#pragma once
#include "SpeedPlanner.h"
#include "SteeringResponse.h"
#include "BrakeResponse.h"
namespace mwneo {
struct Driver {
 Recovery recovery=Recovery::Normal;float stateTime=0,stuckTime=0,lastProgress=0,lastSpeed=0,acceleration=0,integral=0,effort=0,steering=0;
 uint32_t recoveries=0,hardBrakes=0;bool initialized=false,hardBraking=false;
 Point lastHeading{},reverseOrigin{},lastPosition{};float yawRate=0,queueTime=0;
 float movementTime=0,movementDistance=0,translationSpeed=0;bool movementReady=false;
 SteeringResponse response{};
 BrakeResponse brakes{};
 bool previewBraking=false;
 // Learned lateral-grip ceiling (m/s^2, 0 = unlearned). Survives Reset() so a respawn
 // does not forget where this car's tyres actually let go.
 // Opt-in: only the event loader's owned racers enable the grip guard; other users of this header (NFRR) are unchanged.
 float gripCeiling=0,understeerTime=0,steerSaturation=0,understeerPeak=0;bool gripGuard=false;
 void Reset(){float grip=gripCeiling;bool guard=gripGuard;*this={};gripCeiling=grip;gripGuard=guard;}
 Controls Update(const Vehicle& v,const Guidance& g,const Profile& profile){
  previewBraking=false;
  Controls out{};float dt=std::clamp(v.dt,.001f,.1f);
  if(!Finite(v.position)||!Finite(v.heading)||!std::isfinite(v.speed)||!Finite(g.aim)||!g.status)return out;
  Point h=UnitXZ(v.heading),aim=Sub(g.aim,v.position);float square=DotXZ(aim,aim);
  if(LengthXZ(h)<.9f)return out;
  if(!initialized){initialized=true;lastSpeed=v.speed;lastProgress=g.progress;lastHeading=h;lastPosition=v.position;}
  // Native speed can remain nonzero with wheels spinning against a wall.
  // Measure translation over a window so pose updates at a lower frequency
  // than OnDriving cannot alternate between falsely moving and stationary.
  float moved=LengthXZ(Sub(v.position,lastPosition));lastPosition=v.position;
  if(moved>20){movementTime=movementDistance=0;movementReady=false;}
  else {movementTime+=dt;movementDistance+=moved;}
  if(movementTime>=.5f){translationSpeed=movementDistance/movementTime;movementReady=true;movementTime=movementDistance=0;}
  float deltaYaw=std::atan2(CrossXZ(lastHeading,h),DotXZ(lastHeading,h));
  response.Observe(v,g,profile,deltaYaw,recovery==Recovery::Normal);
  brakes.Observe(v,g,profile,std::max(0.f,-effort),recovery==Recovery::Normal&&moved<10);
  float turn=deltaYaw/dt;
  yawRate+=(std::clamp(turn,-3.f,3.f)-yawRate)*std::min(1.f,dt*6);lastHeading=h;
  acceleration+=(std::clamp((v.speed-lastSpeed)/dt,-40.f,40.f)-acceleration)*std::min(1.f,dt*5);
  Profile planning=profile;planning.brakeDecel=brakes.PlanningDecel(profile);
  // Understeer learning. The commanded wheel angle saturates at the steering
  // limit, yet the chassis yaw stays well below the bicycle-model prediction:
  // the tyres are already past their grip. Snap the working lateral limit to
  // the acceleration actually realised, and let it recover slowly.
  const float configuredGrip=profile.lateralAccel,gripFloor=configuredGrip*.65f;
  if(!(gripCeiling>0)||gripCeiling>configuredGrip)gripCeiling=configuredGrip;
  if(gripGuard){
   // Learn a safe speed without shrinking steering authority. Otherwise a
   // lower learned ceiling makes the same wheel angle look more saturated,
   // reinforces the learning trigger and disagrees with provider arc checks.
   float lockLimit=SteeringLimit(v.speed,profile,response.gain);
   float model=std::tan(steering*profile.steeringLock)*response.gain/profile.wheelbase,expectedYaw=v.speed*model;
   steerSaturation=lockLimit>.001f?std::abs(steering)/lockLimit:0;
   bool saturated=steerSaturation>=.85f;
   bool deficit=recovery==Recovery::Normal&&v.speed>25&&saturated&&std::abs(expectedYaw)>.12f&&acceleration>-12.f&&
    (expectedYaw*yawRate<=0||std::abs(yawRate)<.82f*std::abs(expectedYaw));
   if(deficit){understeerTime+=dt;understeerPeak=std::max(understeerPeak,v.speed*std::abs(yawRate));}
   else {understeerTime=std::max(0.f,understeerTime-2*dt);if(understeerTime==0)understeerPeak=0;}
   if(understeerTime>.3f){
    // A single low-yaw sample during turn-in must not erase grip already
    // demonstrated in this deficit window. Persistent low response still
    // lowers the ceiling, while transient steering response keeps its peak.
    float realised=understeerPeak;
    gripCeiling=std::clamp(std::min(gripCeiling,realised*1.1f),gripFloor,configuredGrip);
    understeerTime=.15f;
    understeerPeak=v.speed*std::abs(yawRate);
   }else if(!deficit&&gripCeiling<configuredGrip)gripCeiling=std::min(configuredGrip,gripCeiling+.5f*dt);
  }
  if(gripGuard)planning.lateralAccel=std::min(planning.lateralAccel,gripCeiling);
  auto speed=PlanSpeed(g,v,planning);if(!speed.valid)return out;
  float forward=DotXZ(h,aim),side=CrossXZ(h,aim),headingError=std::atan2(side,forward);
  // Guidance is sampled more slowly than physics. Its quantized centimetre
  // progress jumps must not cancel stuck detection while rubbing a wall.
  bool stationary=movementReady?translationSpeed<1.f:std::abs(v.speed)<2.f;
  bool closeQueue=g.leadVehicle&&g.obstacleDistance<12;
  bool raceTraffic=g.leadVehicle==RaceTrafficPush;
  bool queueStationary=stationary||(raceTraffic&&std::abs(v.speed)<1.f);
  if(queueStationary&&closeQueue&&recovery!=Recovery::Reverse&&(raceTraffic||g.leadSpeed<1))queueTime+=dt;else queueTime=0;
  // A dead-end plan can remain geometrically clear at its last point. If
  // replanning cannot release its zero-speed limit, it still needs recovery.
  bool blockedStatic=g.status==2||(speed.constraint==8&&speed.target<.5f);
  bool wrongWay=DotXZ(h,UnitXZ(g.forward))<-.3f;
  bool trying=blockedStatic||wrongWay||(effort>.45f&&!closeQueue)||(queueTime>(raceTraffic?1.f:4.f)&&g.rearClear);
  if(recovery==Recovery::Normal||recovery==Recovery::DetectingStuck){
   if(stationary&&trying){stuckTime+=dt;recovery=Recovery::DetectingStuck;}else{stuckTime=0;recovery=Recovery::Normal;}
   if(stuckTime>(blockedStatic?1.f:2.5f)||(raceTraffic&&queueTime>=1.f)){recovery=g.rearClear?Recovery::Reverse:Recovery::Realign;reverseOrigin=v.position;stateTime=0;++recoveries;integral=0;queueTime=0;}
  }else{
   stateTime+=dt;
   if(recovery==Recovery::Reverse&&(stateTime>4.f||Distance(v.position,reverseOrigin)>4||!g.rearClear)){recovery=Recovery::Realign;stateTime=0;integral=0;}
   else if(recovery==Recovery::Realign&&stateTime>1.8f){recovery=Recovery::Rejoin;stateTime=0;}
   else if(recovery==Recovery::Rejoin){
    if(std::abs(v.speed)>3&&!wrongWay&&stateTime>2){recovery=Recovery::Normal;stuckTime=0;stateTime=0;}
    else if(stateTime>5){recovery=Recovery::DetectingStuck;stuckTime=1.5f;stateTime=0;}
   }
  }
  // A failed forward rejoin is another contact jam, not a reason to wait
  // through the entire normal recovery timeout before backing out again.
  if(raceTraffic&&queueTime>=1.f&&recovery!=Recovery::Reverse){recovery=g.rearClear?Recovery::Reverse:Recovery::Realign;reverseOrigin=v.position;stateTime=0;++recoveries;integral=0;queueTime=0;}
  // Once the car is moving into a freshly checked, aligned forward corridor,
  // do not crawl through the remaining fixed recovery timeout. A blocked
  // corridor, wrong heading or unresolved contact still retains recovery.
  if((recovery==Recovery::Realign||recovery==Recovery::Rejoin)&&stateTime>.15f&&g.status==1&&g.confidence>=.8f&&
     v.speed>2&&DotXZ(h,UnitXZ(g.forward))>.7f&&forward>2&&fabs(headingError)<.35f&&!closeQueue){
   recovery=Recovery::Normal;stuckTime=stateTime=0;integral=0;
  }
  float target=speed.target;bool reversing=recovery==Recovery::Reverse;
  if(reversing)target=g.rearClear?std::min(fabs(g.reverseSteering)>.05f?2.5f:4.f,std::sqrt(2*profile.brakeDecel*std::max(0.f,g.rearDistance-2.8f))):0;
  else if(recovery==Recovery::Realign)target=std::min(target,7.f);
  else if(recovery==Recovery::Rejoin)target=std::min(target,16.f);
  bool manoeuvring=!reversing&&(g.status==2||raceTraffic)&&g.maneuverSpeed>0&&(recovery==Recovery::Realign||recovery==Recovery::Rejoin);
  if(manoeuvring)target=g.maneuverSpeed;
  float curve=square>4?2*side/square:0;
  float desired=std::atan(profile.wheelbase*curve/response.gain)/profile.steeringLock;
  // Damping uses measured heading response, rather than steering input alone.
  // This suppresses overshoot when successive avoidance targets move sideways.
  if(std::abs(v.speed)>8)desired+=std::clamp((std::abs(v.speed)*curve-yawRate)*.16f,-.2f,.2f);
  if(forward<1&&target>0)desired=std::copysign(.8f,headingError);
  // Back out along the rear corridor that was actually checked. Applying full
  // opposite lock here rotates a wedged car into the same obstacle again.
  if(reversing)desired=g.reverseSteering;
  else if(manoeuvring)desired=g.maneuverSteering;
  float limit=SteeringLimit(v.speed,profile,response.gain);
  desired=std::clamp(desired,-limit,limit);
  // Unwind promptly when the checked target changes sides. Ramping stale
  // opposite lock for another half-second travels tens of metres into a wall.
  float slew=std::abs(v.speed)>45?1.4f:2.6f;
  if(steering*desired<0||std::abs(desired)<std::abs(steering))slew=6.f;
  steering=std::clamp(Approach(steering,desired,dt*slew),-limit,limit);
  float measured=std::abs(v.speed),error=target-measured;
  if(std::abs(error)<8)integral=std::clamp(integral+error*dt,-8.f,8.f);else integral*=std::max(0.f,1-dt*2);
  float demand=.9f*error+.15f*integral-.18f*acceleration;
  if(std::abs(error)<.7f)demand=0;
  // A future corner's braking envelope is moving downward as we advance.
  // After pedal authority is measured, track that envelope rather than
  // waiting for a large speed error. For sqrt(c²+2*b*(s-4-.2*v)), its
  // deceleration at matched speed is b*v/(target+.2*b). This does not lower
  // the planned speed, and the below-target brake release remains active.
  previewBraking=brakes.windows>=3&&!reversing&&!manoeuvring&&recovery==Recovery::Normal&&g.status==1&&
     speed.constraint==1&&speed.constraintDistance>5.f+measured*.2f&&target>12&&error<.75f;
  if(previewBraking)
   demand-=std::min(planning.brakeDecel*1.25f,planning.brakeDecel*measured/(target+planning.brakeDecel*.2f));
  float requested=demand>=0?demand/profile.driveAccel:demand/brakes.PedalDecel(profile);
  // Use full engine output below the geometry's speed envelope. A small
  // coast band avoids pedal chatter around the limit; static stops still win.
  if(!reversing&&!manoeuvring&&recovery==Recovery::Normal&&g.status==1&&error>2)requested=1;
  // Positive acceleration damping may ease off gas below the speed limit;
  // it must not request a brake merely because engine boost is accelerating.
  if(error>.75f&&requested<0)requested=0;
  if(reversing&&g.rearClear&&measured<std::min(2.f,target-.5f))requested=1;
  // A checked low-speed escape needs enough torque to break static contact;
  // a small target speed must not leave a wedged car idling at partial gas.
  if(manoeuvring&&measured<std::min(2.f,target-.5f))requested=1;
  if(target<.1f&&measured<.5f)requested=-.25f;
  // The combined pedal ramp must not spend its first third of a second still
  // accelerating into a genuine tight corner. Cut stale positive effort as
  // soon as a binding corner is already overspeed; ramp the brake from zero.
  // Small speed errors, gentle bends and temporary traffic steering keep the
  // ordinary smooth pedal policy.
  // A truncated checked path or a fixed obstacle also has a real braking
  // envelope. Do not keep accelerating for the pedal ramp's first 0.3 s
  // after exceeding it. Vehicle spacing keeps the existing passing policy.
  bool fixedOverspeed=gripGuard&&(speed.constraint==6||speed.constraint==8);
  bool cornerOverspeed=!reversing&&!manoeuvring&&recovery==Recovery::Normal&&g.status==1&&error<-5&&
   (fixedOverspeed||(LimitSteeringArc(g.predictiveAvoidance)&&speed.constraint==1&&speed.constraintCurve>.006f));
  if(cornerOverspeed&&effort>0)effort=0;
  effort=Approach(effort,std::clamp(requested,-1.f,1.f),dt*(requested<effort?(cornerOverspeed?6.f:3.f):1.8f));
  if(error>.75f&&effort<0)effort=0;
  // Race exit acceleration is not a comfort throttle ramp. Once the checked
  // corridor permits a higher speed, use the requested full engine output
  // immediately; retain the coast band near the envelope and real braking.
  if(!reversing&&!manoeuvring&&recovery==Recovery::Normal&&g.status==1&&error>2)effort=1;
  if(((reversing&&g.rearClear)||manoeuvring)&&measured<std::min(2.f,target-.5f))effort=1;
  // No throttle ramp-down delay once the actual execution corridor is unsafe.
  // Normal corner/spacing control retains continuous pedal transitions.
  if(g.status==2&&measured>2&&!reversing&&!manoeuvring)effort=-std::clamp(measured/(profile.brakeDecel*.5f),.3f,1.f);
  if(raceTraffic&&closeQueue&&queueStationary&&g.status==1&&recovery<=Recovery::DetectingStuck)effort=1;
  // A stopped recovery candidate assumes the wheels are set before moving.
  if((reversing||manoeuvring)&&measured<.5f&&fabs(steering-desired)>.08f)effort=-.25f;
  float throttleOut=std::max(0.f,effort),brakeOut=std::max(0.f,-effort);
  if(gripGuard&&!reversing&&!manoeuvring&&recovery==Recovery::Normal&&g.status==1&&measured>20){
   // Friction circle: cornering load takes grip away from drive torque.
   float load=measured*measured*std::abs(curve)/std::max(1.f,planning.lateralAccel);
   throttleOut*=std::clamp(1-(load-.8f)*2.5f,.35f,1.f);
   // Steering saturated and the aim still pulling away: the car cannot make
   // this arc at this speed. Lift off and shed speed before the wall does.
   if(steerSaturation>=.95f&&std::abs(headingError)>.2f){
    throttleOut=0;brakeOut=std::max(brakeOut,std::abs(headingError)>.4f?.35f:.18f);
   }
  }
  out={steering,throttleOut,brakeOut,target,curve,headingError,recovery,reversing,true};
  if(out.brake>.7f&&!hardBraking)++hardBrakes;hardBraking=out.brake>.7f;
  lastSpeed=v.speed;lastProgress=g.progress;return out;
 }
};
}
