#include "Seis3D/MaterialModels3D.hpp"
#include "Seis3D/VtsWriter.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
namespace {
void check(bool value,const std::string& error){if(!value)throw std::runtime_error(error);}
double compare(const std::vector<double>& a,const std::vector<double>& b){check(a.size()==b.size(),"Buffer size mismatch");double peak=0,error=0;for(std::size_t i=0;i<a.size();++i){peak=std::max(peak,std::abs(a[i]));error=std::max(error,std::abs(a[i]-b[i]));}return error/std::max(peak,1e-30);}
double compareFields(const SimulationResult& a,const SimulationResult& b){const std::vector<double>* x[]{&a.vx,&a.vy,&a.vz,&a.sxx,&a.syy,&a.szz,&a.sxy,&a.sxz,&a.syz};const std::vector<double>* y[]{&b.vx,&b.vy,&b.vz,&b.sxx,&b.syy,&b.szz,&b.sxy,&b.sxz,&b.syz};double error=0;for(int i=0;i<9;++i)error=std::max(error,compare(*x[i],*y[i]));return error;}
}
int main(){try{
    std::string error;const auto root=std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"validation";std::filesystem::create_directories(root);
    SimulationParameters uniform;uniform.nx=14;uniform.ny=10;uniform.nz=24;uniform.maxSteps=80;uniform.topographyAmplitude=0;
    auto identical=uniform;identical.layeredMaterial=true;identical.layers={{"A",50,uniform.vp,uniform.vs,uniform.density},{"B",0,uniform.vp,uniform.vs,uniform.density}};
    CpuSimulation3D a,b;check(a.initialize(uniform,error,{1})&&b.initialize(identical,error,{1}),error);check(a.advance(80,error)&&b.advance(80,error),error);check(compareFields(a.snapshot(),b.snapshot())<1e-12,"Constant layered material changed uniform physics");
    const auto revision=b.revision();auto invalid=identical;invalid.layers.back().thickness=3;check(!b.initialize(invalid,error)&&b.revision()==revision,"Invalid layer discarded session");invalid=identical;invalid.layers.front().vs=0;check(!b.initialize(invalid,error),"Invalid layer Vs accepted");invalid=identical;invalid.layers.clear();check(!b.initialize(invalid,error),"Empty layered material accepted");
    const bool cuda=queryBackend3D(BackendKind3D::Cuda).available;std::ofstream report(root/"material_validation.csv");report<<"preset,steps,dt,min_ppw,min_layer_cells,unresolved_layers,cuda_tested,max_field_relative_error,max_receiver_relative_error\n";
    for(int preset=1;preset<=4;++preset){
        SimulationParameters seed;seed.nx=12;seed.ny=8;seed.nz=64;seed.spacing=1;seed.topographyAmplitude=0;seed.duration=.40;seed.maxSteps=preset==1?0:300;
        auto p=applyMaterialPreset3D(preset,seed);CpuSimulation3D cpu;check(cpu.initialize(p,error,{1}),error);
        const auto initial=cpu.snapshot();double maxVp=0,minVs=1e30;for(const auto& l:p.layers){maxVp=std::max(maxVp,l.vp);minVs=std::min(minVs,l.vs);}
        check(cpu.diagnostics().materialMaxVp==maxVp&&cpu.diagnostics().materialMinVs==minVs,"CFL/PPW did not use material extrema");check(cpu.diagnostics().unresolvedLayers==0,"Fine grid lost layer coverage");
        if(preset==1){for(int k:{0,4,5,9,10,29,30,49,50,63}){const auto q=initial.grid.index(2+initial.grid.ghost,2+initial.grid.ghost,k+initial.grid.ghost);const int id=materialLayerAtDepth3D(p,k);check(initial.layerIds[q]==id&&std::abs(initial.materialVp[q]-p.layers[id].vp)<1e-9&&std::abs(initial.materialVs[q]-p.layers[id].vs)<1e-9&&initial.materialDensity[q]==p.layers[id].density,"Supplied table was not assigned at exact interfaces");}check(cpu.diagnostics().minLayerCells>4.99,"Five metre layer did not span five cells");}
        std::cout<<"Testing material "<<preset<<" steps="<<cpu.totalSteps()<<std::endl;check(cpu.advance(cpu.totalSteps(),error),error);const auto result=cpu.snapshot();check(result.allFinite&&result.diagnostics.elasticEnergy>0&&result.diagnostics.maxTractionRateRelative<1e-8,"Layered propagation failed numerical guards");
        if(preset==1)check(cpu.currentStep()*cpu.dt()>1.5/p.dominantFrequency&&result.maxAbsVz>1e-10,"Loess case stopped before source pulse");
        double fields=0,receivers=0;
        if(cuda){auto gpu=createSimulationBackend3D(BackendKind3D::Cuda,error);check(gpu&&gpu->initialize(p,error)&&gpu->advance(gpu->totalSteps(),error),error);fields=compareFields(result,gpu->snapshot());check(fields<1e-8,"Layered CPU/CUDA nine-field mismatch");for(std::size_t r=0;r<cpu.receivers().size();++r){const auto& x=cpu.receivers()[r];const auto& y=gpu->receivers()[r];receivers=std::max({receivers,compare(x.samples,y.samples),compare(x.samplesVx,y.samplesVx),compare(x.samplesVy,y.samplesVy)});}check(receivers<1e-8,"Layered CPU/CUDA receivers differ");check(std::abs(result.diagnostics.elasticEnergy-gpu->diagnostics().elasticEnergy)/result.diagnostics.elasticEnergy<1e-8,"Material energy uses wrong coefficients");
            for(int component=5;component<=7;++component){DisplayFrame3D x,y;DisplayRequest3D request{10,6,3,component};check(cpu.readDisplayFrame(request,x,error)&&gpu->readDisplayFrame(request,y,error),error);for(int plane=0;plane<3;++plane)check(x.planes[plane].values==y.planes[plane].values,"Material slices differ between backends");}
        }
        if(preset==1)check(writeVts(result,root/"loess_layers.vts",error),error);
        report<<preset<<','<<cpu.currentStep()<<','<<cpu.dt()<<','<<cpu.diagnostics().pointsPerShortestWavelength<<','<<cpu.diagnostics().minLayerCells<<','<<cpu.diagnostics().unresolvedLayers<<','<<cuda<<','<<fields<<','<<receivers<<'\n';report.flush();
    }
    auto curved=applyMaterialPreset3D(1,SimulationParameters{});curved.nx=20;curved.ny=12;curved.nz=80;curved.spacing=1;curved.topographyAmplitude=2;curved.topographyWavelength=50;curved.maxSteps=3;CpuSimulation3D terrain;check(terrain.initialize(curved,error,{1})&&terrain.advance(3,error),error);const auto follow=terrain.snapshot();
    for(int j=follow.grid.ghost;j<follow.grid.ny-follow.grid.ghost;++j)for(int i=follow.grid.ghost;i<follow.grid.nx-follow.grid.ghost;++i)check(follow.layerIds[follow.grid.index(i,j,follow.grid.ghost)]==0,"Surface-following top is not first layer");
    for(int k=follow.grid.ghost;k<follow.grid.nz-follow.grid.ghost;++k){const auto first=follow.grid.index(follow.grid.ghost,follow.grid.ghost,k);for(int j=follow.grid.ghost;j<follow.grid.ny-follow.grid.ghost;++j)for(int i=follow.grid.ghost;i<follow.grid.nx-follow.grid.ghost;++i){const auto q=follow.grid.index(i,j,k);check(follow.layerIds[q]==follow.layerIds[first]&&follow.materialVp[q]==follow.materialVp[first]&&follow.materialVs[q]==follow.materialVs[first]&&follow.materialDensity[q]==follow.materialDensity[first],"Same grid depth contains horizontal material jumps");}}
    for(auto mode:{LayerDepthMode3D::GridIndex,LayerDepthMode3D::BelowSurface,LayerDepthMode3D::Datum}){curved.layerDepthMode=mode;check(terrain.initialize(curved,error,{1})&&terrain.advance(3,error),error);if(cuda){auto gpu=createSimulationBackend3D(BackendKind3D::Cuda,error);check(gpu&&gpu->initialize(curved,error)&&gpu->advance(3,error),error);check(compareFields(terrain.snapshot(),gpu->snapshot())<1e-8,"Curved layered CPU/CUDA mismatch");}}
    curved.layerDepthMode=LayerDepthMode3D::Datum;check(terrain.initialize(curved,error,{1}),error);const auto horizontal=terrain.snapshot();bool different=false;for(std::size_t q=0;q<horizontal.layerIds.size();++q)different|=horizontal.layerIds[q]!=follow.layerIds[q];check(different,"Layer coordinate mode had no effect");
    auto coarse=curved;coarse.spacing=10;coarse.topographyAmplitude=0;check(terrain.initialize(coarse,error,{1})&&terrain.diagnostics().unresolvedLayers>0,"Coarse mesh did not warn about missing thin layers");
    std::cout<<"PASS layered assignment, homogeneous equivalence, four presets, full loess source window, CPU/CUDA, materials export and resolution guards\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
