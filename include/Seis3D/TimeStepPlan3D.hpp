#pragma once
#include "CurvilinearSolver.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace Seis3D::Curvilinear {
struct TimeStepPlan3D {double dt{},waveLimit{},dampingLimit{},sourceLimit{},durationSteps{};int steps{};};
inline bool timeStepPlan3D(const SimulationParameters& p,double maxVp,double metricNorm,double minLength,double maxDamping,TimeStepPlan3D& plan,std::string& error) {
    error.clear();
    for(double value:{p.spacing,p.cflSafety,p.duration,p.dominantFrequency,maxVp,metricNorm,minLength,maxDamping})if(!std::isfinite(value)){error="Time-step inputs must be finite.";return false;}
    if(p.spacing<=0||p.duration<=0||p.duration>10||p.dominantFrequency<=0||p.dominantFrequency>1e6||maxVp<=0||metricNorm<=0||minLength<=0||maxDamping<0||p.cflSafety<minCflCoefficient3D||p.cflSafety>maxCflCoefficient3D||p.maxSteps<0||p.maxSteps>maxTimeSteps3D){error="Invalid time-step parameters.";return false;}
    plan.waveLimit=p.cflSafety*std::min(p.spacing/metricNorm,minLength)/maxVp;
    plan.dampingLimit=maxDamping>0?.5/maxDamping:std::numeric_limits<double>::max();
    plan.sourceLimit=1/(50*p.dominantFrequency);
    const double safe=std::min({plan.waveLimit,plan.dampingLimit,plan.sourceLimit});
    plan.durationSteps=std::max(1.0,std::ceil(p.duration/safe));
    if(!std::isfinite(plan.durationSteps)||(p.maxSteps==0&&plan.durationSteps>maxTimeSteps3D)){error="Time-step limit: at most 20,000 steps. Set a maximum step cap.";return false;}
    plan.dt=p.duration/plan.durationSteps;plan.steps=static_cast<int>(p.maxSteps>0?std::min(plan.durationSteps,double(p.maxSteps)):plan.durationSteps);
    return plan.dt>0&&std::isfinite(plan.dt);
}
}
