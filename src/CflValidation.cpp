#include "Seis3D/CurvilinearSolver.hpp"
#include "Seis3D/MaterialModels3D.hpp"
#include "Seis3D/ModelPresets3D.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
namespace {
void check(bool okay,const std::string& why){if(!okay)throw std::runtime_error(why);}
struct Run { SimulationResult state;std::vector<ReceiverTrace3D> traces; };
Run run(SimulationParameters p,BackendKind3D backend,const char* name,std::ostream& report) {
    CpuSimulation3D s(backend);std::string error;check(s.initialize(p,error,{2}),error);
    double late=0,drivenPeak=0;int tailSamples=0;
    while(!s.completed()) {
        check(s.advance(std::min(32,s.totalSteps()-s.currentStep()),error),std::string(name)+": "+error);
        const auto& d=s.diagnostics();
        check(d.allFinite&&d.maxTractionRateRelative<1e-9,"Finite fields or traction-rate closure failed");
        const double time=s.currentStep()*s.dt();
        if(time<=3/p.dominantFrequency)drivenPeak=std::max(drivenPeak,d.elasticEnergy);
        if(time>std::max(.6*p.duration,3.5/p.dominantFrequency)){late=std::max(late,d.elasticEnergy);++tailSamples;}
    }
    const auto& d=s.diagnostics();
    check(s.currentStep()*s.dt()>=p.duration-1e-12,"Step cap hid late-time behaviour");
    check(s.dt()<=d.waveDtLimit&&s.dt()<=d.dampingDtLimit&&s.dt()<=d.sourceDtLimit,"Time ceiling exceeded");
    check(drivenPeak>0&&tailSamples>0&&std::isfinite(d.peakEnergy)&&late<=1.5*drivenPeak,"Late energy exceeded source-window peak or tail was not sampled");
    report<<name<<','<<(backend==BackendKind3D::Cuda?"CUDA":"CPU")<<','<<p.cflSafety<<','<<s.totalSteps()<<','<<s.dt()*1000<<','<<late/drivenPeak<<','<<d.timing.totalStepMs<<','<<d.timing.averageStepMs()<<'\n';report.flush();
    std::cout<<name<<" CFL="<<p.cflSafety<<" steps="<<s.totalSteps()<<" dt_ms="<<s.dt()*1000<<" late/source_peak="<<late/drivenPeak<<" total_ms="<<d.timing.totalStepMs<<std::endl;
    return {s.snapshot(),s.receivers()};
}
double fieldDifference(const Run& a,const Run& b) {
    double err=0,norm=0;
    const std::vector<double>* x[]{&a.state.vx,&a.state.vy,&a.state.vz};
    const std::vector<double>* y[]{&b.state.vx,&b.state.vy,&b.state.vz};
    for(int f=0;f<3;++f)for(std::size_t q=0;q<x[f]->size();++q){const double d=(*x[f])[q]-(*y[f])[q];err+=d*d;norm+=(*x[f])[q]*(*x[f])[q];}
    return std::sqrt(err/std::max(norm,1e-300));
}
}
int main(int argc,char** argv) {try {
    const auto root=std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"validation";std::filesystem::create_directories(root);
    const bool benchmark=argc>1&&std::string(argv[1])=="--benchmark";
    std::ofstream report(root/(benchmark?"cfl_benchmark.csv":"cfl_range.csv"));report<<std::setprecision(12)<<"case,backend,cfl,steps,dt_ms,late_over_peak,total_ms,average_ms\n";
    const auto backend=defaultBackend3D();
    if(benchmark) {
        for(double relief:{0.0,20.0}) {
            auto p=applyModelPreset3D(0,{});p.topographyAmplitude=relief;p.maxSteps=0;
            p.cflSafety=.22;const auto old=run(p,backend,relief==0?"flat":"curved",report);
            p.cflSafety=.8;const auto fast=run(p,backend,relief==0?"flat":"curved",report);
            const double relative=fieldDifference(old,fast);
            std::cout<<"Benchmark velocity relative L2 vs CFL .22="<<relative<<std::endl;
            check(relative<.1,"Benchmark field differs by more than 10% from .22 reference");
        }
        return 0;
    }
    for(int id=0;id<10;++id) {
        SimulationParameters p;p.nx=20;p.ny=18;p.nz=24;p.maxSteps=0;p.duration=1;
        p.topographyAmplitude=0;
        if(id==1||id==2){p.topographyAmplitude=35;p.topographyWavelength=id==1?240:80;}
        if(id==3||id==4){p.topographyAmplitude=20;p.topographyWavelength=120;p.vs=id==3?150:2550;}
        if(id==5){p=applyMaterialPreset3D(1,p);p.spacing=1;p.topographyAmplitude=0;p.nz=64;p.duration=.4;}
        if(id==6){p.absorbingBoundary.enabled=false;p.sourceZ=12;}
        if(id==7||id==8){p.nx=48;p.ny=20;p.nz=64;p.sourceZ=20;p.dominantFrequency=id==7?30:50;p.spacing=id==7?2.5:1.25;p.topographyAmplitude=2;p.topographyWavelength=100;p.duration=.16;}
        if(id==9){p.nx=p.ny=p.nz=8;p.topographyAmplitude=5;p.topographyWavelength=40;p.sourceZ=2;}
        const auto name="case_"+std::to_string(id);
        for(double cfl:{.22,.8}){p.cflSafety=cfl;(void)run(p,backend,name.c_str(),report);}
    }
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
