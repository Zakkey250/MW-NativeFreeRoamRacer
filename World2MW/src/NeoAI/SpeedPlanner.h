#pragma once
#include "Types.h"
namespace mwneo {
// Binding constraint diagnostics: 0 maximum, 1 curvature, 2 width,
// 3 hazard, 4 confidence, 5 vehicle spacing, 6 static obstacle, 7 blocked, 8 plan end.
struct SpeedPlan {float target=0,maxCurvature=0;bool valid=false;unsigned constraint=0;float constraintDistance=0,constraintCurve=0;};
inline SpeedPlan PlanSpeed(const Guidance& g,const Vehicle& v,const Profile& p){
 SpeedPlan result{};if(!g.status||g.count<2||g.count>MaxSamples||!std::isfinite(p.maxSpeed)||p.maxSpeed<=0||!std::isfinite(p.brakeDecel)||p.brakeDecel<=0||!std::isfinite(p.lateralAccel)||p.lateralAccel<=0||!std::isfinite(g.confidence)||!std::isfinite(g.obstacleDistance)||!std::isfinite(g.leadSpeed))return result;
 float speed=p.maxSpeed;const float horizon=SpeedHorizon(v.speed);
 const float execution=std::clamp(SteeringHorizon(v.speed,p)*.7f,12.f,45.f);
 for(uint32_t i=0;i<g.count;++i)if(!Finite(g.samples[i].position))return result;
 auto shapePoint=[&](uint32_t i){auto b=g.samples[i].position;if(!i||i+1>=g.count)return b;
  auto a=g.samples[i-1].position,c=g.samples[i+1].position;return Point{b.x*.5f+(a.x+c.x)*.25f,b.y,b.z*.5f+(a.z+c.z)*.25f};
 };
 for(uint32_t i=0;i<g.count;++i){const auto& s=g.samples[i];if(!Finite(s.position)||!std::isfinite(s.distance)||s.distance<0)return result;
  if(!std::isfinite(s.widthLeft)||!std::isfinite(s.widthRight)||s.widthLeft<0||s.widthRight<0)return result;
  if(i&&s.distance<g.samples[i-1].distance)return result;if(s.distance>horizon)break;
  // Evaluate the shape over a car-length-independent span. A 3 m lane
  // correction between lattice points is not an abrupt road corner.
  uint32_t before=i>2?i-3:0,after=std::min(g.count-1,i+3);
  // Near the car, use the actual steering arc below rather than a lattice
  // vertex correction. Farther corners retain the braking preview.
  if(!std::isfinite(s.roadCurvature))return result;
  bool checkedLocal=g.status==1&&(s.flags&CheckedExecution)&&s.distance<=30;
  float curve=checkedLocal?0.f:(s.roadCurvature>=0?s.roadCurvature:
   ((!(s.flags&LocalCorrection)||s.distance>execution)&&before<i&&after>i?Curve(shapePoint(before),shapePoint(i),shapePoint(after)):0));
  // A clearance-checked path proves space, not sufficient lateral grip.
  // Fixed supports cannot be pushed like another race vehicle, so include
  // the actual detour shape even on an otherwise straight authored road.
  if((g.predictiveAvoidance&AuthoredObstacleAvoidance)&&before<i&&after>i)
   curve=std::max(curve,Curve(shapePoint(before),shapePoint(i),shapePoint(after)));
  result.maxCurvature=std::max(result.maxCurvature,curve);
  float allowed=curve>.0001f?std::sqrt(p.lateralAccel/curve):p.maxSpeed;unsigned reason=1;
  float width=s.widthLeft+s.widthRight;
  if(width<2*v.halfWidth+.5f){allowed=0;reason=2;}
  else if(width<5){float narrow=22.f+(width-2*v.halfWidth-.5f)*10;if(narrow<allowed){allowed=narrow;reason=2;}}
  if((s.flags&Hazard)&&allowed>25){allowed=25;reason=3;}
  if(s.flags&PlanEnd){allowed=0;reason=8;}
  // Backward braking propagation: a distant corner is not today's apex speed.
  float usable=std::max(0.f,s.distance-(4.f+std::abs(v.speed)*.2f));
  float limit=std::sqrt(allowed*allowed+2*p.brakeDecel*usable);
  if(limit<speed){speed=limit;result.constraint=reason;result.constraintDistance=s.distance;result.constraintCurve=curve;}
 }
 // Predictive avoidance bits: 1 traffic, 2 parallel wall scrape/reserve.
 // Their steering corrections are not road corners. Road curvature above
 // still limits actual bends; neither bit overrides blocked status below.
 // The actual steering arc still limits immediate corner speed; widening
 // the preview filter must not hide a genuine hairpin at the car's nose.
 if(LimitSteeringArc(g.predictiveAvoidance)&&Finite(g.aim)&&Finite(v.heading)){
  auto delta=Sub(g.aim,v.position);float square=DotXZ(delta,delta);
  if(square>16){float curve=2*std::abs(CrossXZ(UnitXZ(v.heading),delta))/square;
   float limit=curve>.0001f?std::sqrt(p.lateralAccel/curve):p.maxSpeed;
   if(limit<speed){speed=limit;result.constraint=1;result.constraintDistance=0;result.constraintCurve=curve;}
  }
 }
 float confidence=std::clamp(g.confidence,0.f,1.f);
 if(confidence<.6f&&12.f+confidence*45<speed){speed=12.f+confidence*45;result.constraint=4;result.constraintDistance=0;result.constraintCurve=0;}
 if(g.obstacleDistance<1000&&g.leadVehicle!=RaceTrafficPush){
  float lead=g.leadVehicle?std::max(0.f,g.leadSpeed):0;
  // Race following distance: preserve body clearance, but permit an actively
  // checked passing lane rather than imposing a long cruising headway.
  float gap=std::max(0.f,g.obstacleDistance-(g.leadVehicle?5.f+std::abs(v.speed)*.25f:4.f));
  float limit=std::sqrt(lead*lead+2*p.brakeDecel*gap);
  if(limit<speed){speed=limit;result.constraint=g.leadVehicle?5:6;result.constraintDistance=g.obstacleDistance;result.constraintCurve=0;}
 }
 if(g.status==2){speed=0;result.constraint=7;result.constraintDistance=0;result.constraintCurve=0;}
 result.target=std::clamp(speed,0.f,p.maxSpeed);result.valid=std::isfinite(result.target);return result;
}
}
