#include "Seis3D/CurvilinearSolver.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace Seis3D::Curvilinear;
// Reproducible raw-field capture; no gain, clipping, normalization or renderer.
int main(int argc,char** argv) {
    const std::string tag=argc>1?argv[1]:"current";
    const auto root=std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"audit";
    std::filesystem::create_directories(root);
    for(double relief:{0.0,20.0}) {
        SimulationParameters p;p.nx=p.nz=200;p.ny=20;p.spacing=10;p.topographyAmplitude=relief;
        p.topographyWavelength=300;p.sourceZ=50;p.duration=0.5;p.maxSteps=0;
        p.absorbingBoundary.cells={6,6,6}; // Hold the old boundary fixed to isolate the stencil correction.
        if(argc>2)p.dominantFrequency=std::stod(argv[2]);
        std::string error;auto s=createSimulationBackend3D(queryBackend3D(BackendKind3D::Cuda).available?BackendKind3D::Cuda:BackendKind3D::Cpu,error);
        if(!s||!s->initialize(p,error,{4})){std::cerr<<error;return 1;}
        while(!s->completed())if(!s->advance(32,error)){std::cerr<<error;return 1;}
        const auto state=s->snapshot();const auto& g=state.grid;
        const auto name=tag+(relief==0?"_flat":"_curved");
        std::ofstream csv(root/(name+".csv"));csv<<std::setprecision(17)<<"ix,iz,x,z,vx,vy,vz,sxx,syy,szz,sxz\n";
        for(int k=0;k<p.nz;++k)for(int i=0;i<p.nx;++i){const auto q=g.index(i+g.ghost,p.ny/2+g.ghost,k+g.ghost);const auto& x=g.coordinates[q];csv<<i<<','<<k<<','<<x[0]<<','<<x[2]<<','<<state.vx[q]<<','<<state.vy[q]<<','<<state.vz[q]<<','<<state.sxx[q]<<','<<state.syy[q]<<','<<state.szz[q]<<','<<state.sxz[q]<<'\n';}
        std::ofstream trace(root/(name+"_receivers.csv"));trace<<std::setprecision(17)<<"t";for(std::size_t r=0;r<s->receivers().size();++r)trace<<",R"<<r;trace<<'\n';
        for(int t=0;t<=s->currentStep();++t){trace<<t*s->dt();for(const auto& r:s->receivers())trace<<','<<r.samples[t];trace<<'\n';}
        std::ofstream meta(root/(name+".txt"));meta<<std::setprecision(17)<<"nx="<<p.nx<<" ny="<<p.ny<<" nz="<<p.nz<<" h="<<p.spacing<<" f0="<<p.dominantFrequency<<" sourceZ="<<p.sourceZ<<" A="<<relief<<" L="<<p.topographyWavelength<<" T="<<p.duration<<" Vp="<<p.vp<<" Vs="<<p.vs<<" rho="<<p.density<<" dt="<<s->dt()<<" steps="<<s->currentStep()<<" energy="<<s->diagnostics().elasticEnergy<<" peakV="<<s->diagnostics().maxAbsVelocity<<'\n';
        std::cout<<name<<" dt="<<s->dt()<<" steps="<<s->currentStep()<<" elapsed_ms="<<s->diagnostics().timing.totalStepMs<<std::endl;
    }
}
