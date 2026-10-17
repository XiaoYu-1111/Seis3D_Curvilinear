#include "PointKernels3D.hpp"
#include "Seis3D/AbsorbingBoundary3D.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Seis3D::Curvilinear;
namespace {
constexpr double pi=3.14159265358979323846;
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
using Line=std::array<std::vector<double>,9>;
// Periodic 1D P/S plane wave embedded in the real 3D nine-field point kernel.
// Analytic continuum solution is independent of the implementation and of CUDA.
double plane(int nodes,bool shear,int mode,bool checker=false,double cfl=.1*std::sqrt(3)) {
    const int nx=nodes+2*ghostWidth3D,ny=15,nz=15;const auto count=static_cast<std::size_t>(nx)*ny*nz;
    Point::Mesh g;g.nx=nx;g.ny=ny;g.nz=nz;g.count=count;g.h=1.0/nodes;g.lambda=2;g.mu=g.density=1;
    std::vector<double> metrics(9*count),zero(count),jacobian(count,1);
    for(std::size_t q=0;q<count;++q)metrics[9*q]=metrics[9*q+4]=metrics[9*q+8]=1;
    g.metrics=metrics.data();g.damping=g.source=zero.data();g.jacobian=jacobian.data();
    Line base,state,k[4],stage;std::array<std::vector<double>,9> volume,rate;Point::Fields in,out;
    for(int f=0;f<9;++f){for(Line* line:{&base,&state,&k[0],&k[1],&k[2],&k[3],&stage})(*line)[f].resize(nodes);volume[f].resize(count);rate[f].resize(count);in.f[f]=volume[f].data();out.f[f]=rate[f].data();}
    const double speed=shear?1:2,time=checker?.25:.173; // Non-integer transit: a frozen/wrong-speed field must fail.
    // The Nyquist mode used to be an exact undamped null mode of centered FD4.
    for(int x=0;x<nodes;++x){const double value=checker?(x%2?-1.0:1.0):std::sin(2*pi*mode*x/nodes);state[shear?1:0][x]=value;state[shear?6:3][x]=-speed*value;if(!shear)state[4][x]=state[5][x]=-g.lambda/speed*value;}
    const int steps=static_cast<int>(std::ceil(time/(cfl*g.h/(std::sqrt(3)*2))));const double dt=time/steps;
    for(int step=0;step<steps;++step) {
        base=state;
        for(int s=0;s<4;++s) {
            const Line& input=s==0?base:stage;
            for(int f=0;f<9;++f)for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)volume[f][Point::index(g,x,y,z)]=input[f][(x-ghostWidth3D+nodes)%nodes];
            for(int x=0;x<nodes;++x){const auto q=Point::index(g,x+ghostWidth3D,7,7);Point::rhs(in,out,g,q,0,step,s);for(int f=0;f<9;++f)k[s][f][x]=rate[f][q];}
            if(s<3)for(int f=0;f<9;++f)for(int x=0;x<nodes;++x)stage[f][x]=base[f][x]+dt*(s==2?1:.5)*k[s][f][x];
        }
        for(int f=0;f<9;++f)for(int x=0;x<nodes;++x)state[f][x]=base[f][x]+dt*(k[0][f][x]+2*k[1][f][x]+2*k[2][f][x]+k[3][f][x])/6;
    }
    double error=0,exactNorm=0,amplitude=0;
    for(int x=0;x<nodes;++x){const double exact=std::sin(2*pi*mode*(double(x)/nodes-speed*time));const double v=state[shear?1:0][x];error+=(v-exact)*(v-exact);exactNorm+=exact*exact;amplitude+=v*v;}
    return checker?std::sqrt(amplitude/nodes):std::sqrt(error/exactNorm);
}
}
int main() {
    try {
        for(bool shear:{false,true}) {
            const double coarse=plane(32,shear,2),fine=plane(64,shear,2);
            std::cout<<(shear?"S":"P")<<" continuum relative L2 error: coarse="<<coarse<<" fine="<<fine<<" ratio="<<coarse/fine<<'\n';
            check(fine<.005&&fine<coarse*.6,"Plane-wave phase/amplitude or spatial convergence failed");
            const double eightNodes=plane(32,shear,4);
            std::cout<<(shear?"S":"P")<<" 8 nodes/wavelength continuum relative L2="<<eightNodes<<'\n';
            check(eightNodes<.03,"Eight-node wavelength plane wave exceeds 3% error");
        }
        const double shortWave=plane(64,false,1,true);
        std::cout<<"Nyquist velocity RMS / initial="<<shortWave<<" (old centered operator: 1)\n";
        check(shortWave<.02,"Unphysical collocated checkerboard mode retained");
        for(double cfl:{.22,.5,.8})for(bool shear:{false,true}) {
            const double error=plane(32,shear,4,false,cfl);
            std::cout<<"CFL="<<cfl<<" "<<(shear?"S":"P")<<" 8 PPW relative L2="<<error<<std::endl;
            check(error<.03,"Larger CFL exceeds 3% plane-wave error at 8 PPW");
        }
        SimulationParameters p;check(p.duration==1&&p.maxSteps==5000,"Initial stop must be 1 second or 5000 steps");
        p.nx=100;p.ny=20;p.nz=100;p.absorbingBoundary.cells={12,4,10};
        check(absorbingWidths3D(p)==std::array<int,3>{12,4,10},"Sponge widths coupled to narrow Y");
        check(absorbingRate3D(p,50,10,0,3000)==0,"Top or interior unexpectedly damped");
        check(absorbingRate3D(p,0,10,50,3000)>0&&absorbingRate3D(p,50,10,99,3000)>0,"Sides or bottom undamped");
        const double rate=absorbingRate3D(p,0,10,50,3000);p.absorbingBoundary.strength*=2;
        check(absorbingRate3D(p,0,10,50,3000)==2*rate,"Sponge strength ignored");
        p.absorbingBoundary.enabled=false;check(absorbingRate3D(p,0,0,99,3000)==0,"Disabled sponge still active");
        CpuSimulation3D run;std::string error;p.nx=p.ny=p.nz=12;p.topographyAmplitude=0;p.maxSteps=2;
        check(run.initialize(p,error,{1})&&run.diagnostics().maxDampingDt==0&&run.advance(2,error),"Disabled sponge backend failed");
        const auto revision=run.revision();p.absorbingBoundary.cells[0]=0;check(!run.initialize(p,error)&&run.revision()==revision,"Invalid sponge discarded active session");
        for(double f0:{30.0,50.0,60.0}) {
            SimulationParameters high;high.nx=48;high.ny=20;high.nz=64;high.sourceZ=20;
            high.dominantFrequency=f0;high.spacing=f0==30?2.5:f0==50?1.25:1.0;high.topographyAmplitude=2;high.topographyWavelength=100;high.duration=.12;
            CpuSimulation3D session(queryBackend3D(BackendKind3D::Cuda).available?BackendKind3D::Cuda:BackendKind3D::Cpu);
            check(session.initialize(high,error,{2})&&session.advance(session.totalSteps(),error),"30/50/60 Hz resolved source run failed");
            const auto& d=session.diagnostics();
            std::cout<<"Resolved Ricker: f0="<<f0<<" h="<<high.spacing<<" steps="<<session.currentStep()<<" PPW="<<d.pointsPerShortestWavelength<<" dt_ms="<<session.dt()*1000<<'\n';
            check(d.allFinite&&d.pointsPerShortestWavelength>=8&&d.maxTractionRateRelative<1e-9&&session.currentStep()*session.dt()>=.12-1e-12,"30/50/60 Hz sampling/stop/surface check failed");
        }
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
