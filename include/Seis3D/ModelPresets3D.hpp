#pragma once
#include "Seis3D/CurvilinearSolver.hpp"
#include "Seis3D/ReceiverGeometry3D.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace Seis3D::Curvilinear {
struct ModelPreset3D {const char* name;int nx,ny,nz;double spacing,relief,wavelength;};
inline SimulationParameters resizeModel3D(const SimulationParameters&,std::array<int,3>);
inline constexpr std::array<ModelPreset3D,7> modelPresets3D{{
    {"Wavefield benchmark: 200 x 20 x 200",200,20,200,10,20,300},
    {"Narrow Y: 100 x 20 x 100",100,20,100,10,20,300},
    {"Compact: 50 x 20 x 50",50,20,50,10,12,240},
    {"Wide X: 200 x 50 x 100",200,50,100,10,20,500},
    {"Cube: 100 x 100 x 100",100,100,100,10,20,300},
    {"Cube: 200 x 200 x 200",200,200,200,10,20,300},
    {"Cube: 300 x 300 x 300",300,300,300,10,20,300}
}};
inline SimulationParameters applyModelPreset3D(int index,const SimulationParameters& current) {
    if(index<0||index>=static_cast<int>(modelPresets3D.size()))throw std::invalid_argument("Unknown model preset");
    const auto& preset=modelPresets3D[index];auto next=resizeModel3D(current,{preset.nx,preset.ny,preset.nz});
    next.nx=preset.nx;next.ny=preset.ny;next.nz=preset.nz;next.spacing=preset.spacing;
    next.topographyAmplitude=preset.relief;next.topographyWavelength=preset.wavelength;next.terrainTransitionDepth=0;
    if(index==0) {
        // Complete, reproducible case from the raw-field comparison figure.
        next.vp=3000;next.vs=1700;next.density=2200;next.layeredMaterial=false;next.layers.clear();next.layerDepthMode=LayerDepthMode3D::GridIndex;
        next.dominantFrequency=12;next.sourceX=100;next.sourceY=10;next.sourceZ=50;
        next.duration=.5;next.maxSteps=5000;next.cflSafety=defaultCflCoefficient3D;
        next.receiverArray=ReceiverArraySettings3D{};
        next.absorbingBoundary={true,{6,6,6},3};
    }
    return next;
}
struct ModelMemory3D {std::size_t points{};double cpuMiB{},deviceMiB{},cudaHostMiB{};bool withinBudget{};};
inline ModelMemory3D estimateModelMemory3D(const SimulationParameters& p) {
    ModelMemory3D result;
    if(p.nx<8||p.ny<8||p.nz<8||p.nx>maxPhysicalNodesPerAxis3D||p.ny>maxPhysicalNodesPerAxis3D||p.nz>maxPhysicalNodesPerAxis3D)return result;
    result.points=static_cast<std::size_t>(p.nx+2*ghostWidth3D)*(p.ny+2*ghostWidth3D)*(p.nz+2*ghostWidth3D);
    std::vector<std::array<int,3>> receivers;std::string error;if(!receiverNodes3D(p,receivers,error))return result;
    const double receiverBytes=receivers.size()*3.0*(p.maxSteps>0?p.maxSteps+1.0:20001.0)*sizeof(double);
    result.cpuMiB=(result.points*(p.layeredMaterial?84.0:81.0)*sizeof(double)+receiverBytes)/(1024*1024);
    result.cudaHostMiB=(result.points*(p.layeredMaterial?21.0:18.0)*sizeof(double)+receiverBytes+(result.points+127)/128*72.0)/(1024*1024);
    result.deviceMiB=(result.points*(p.layeredMaterial?81.0:78.0)*sizeof(double)+(result.points+127)/128*72.0)/(1024*1024);
    result.withinBudget=result.points<=maxPaddedNodes3D;return result;
}
inline bool fitsMemory3D(const ModelMemory3D& m,BackendKind3D kind,const BackendMemory3D& available) {
    if(!m.withinBudget)return false;
    constexpr double mib=1024.0*1024;
    const double host=kind==BackendKind3D::Cuda?m.cudaHostMiB:m.cpuMiB;
    return (!available.hostAvailableBytes||host*mib<=.8*available.hostAvailableBytes)&&
           (kind!=BackendKind3D::Cuda||(available.deviceAvailableBytes&&m.deviceMiB*mib<=.9*available.deviceAvailableBytes));
}
inline SimulationParameters resizeModel3D(const SimulationParameters& current,std::array<int,3> dims) {
    auto p=current;
    const int old[3]{current.nx,current.ny,current.nz};
    for(int& count:dims)count=std::clamp(count,8,maxPhysicalNodesPerAxis3D);
    p.nx=dims[0];p.ny=dims[1];p.nz=dims[2];
    const auto remap=[&](int value,int axis,int margin){if(value<0)return -1;if(value>=margin&&value<=dims[axis]-1-margin)return value;const double relative=old[axis]>1?double(value)/(old[axis]-1):.5;return std::clamp(int(std::lround(std::clamp(relative,0.0,1.0)*(dims[axis]-1))),margin,dims[axis]-1-margin);};
    p.sourceX=remap(current.sourceX,0,2);p.sourceY=remap(current.sourceY,1,2);p.sourceZ=remap(current.sourceZ,2,2);
    auto& r=p.receiverArray;r.z=remap(r.z,2,0);
    if(r.kind!=ReceiverArrayKind3D::AutoXLine) {
        r.startX=remap(r.startX,0,0);r.startY=remap(r.startY,1,0);r.strideX=std::max(1,r.strideX);r.strideY=std::max(1,r.strideY);
        const int x=r.startX<0?p.nx/2:r.startX,y=r.startY<0?p.ny/2:r.startY;
        r.countX=std::clamp(r.countX,1,1+(p.nx-1-x)/r.strideX);r.countY=std::clamp(r.countY,1,1+(p.ny-1-y)/r.strideY);
    }
    return p;
}
// Explicit UI action: retain spacing and axis proportions, report resulting dimensions.
inline bool fitModelToMemory3D(const SimulationParameters& requested,BackendKind3D kind,const BackendMemory3D& available,SimulationParameters& fitted) {
    const int dims[3]{std::clamp(requested.nx,8,maxPhysicalNodesPerAxis3D),std::clamp(requested.ny,8,maxPhysicalNodesPerAxis3D),std::clamp(requested.nz,8,maxPhysicalNodesPerAxis3D)};
    auto candidate=[&](double scale){return resizeModel3D(requested,{std::max(8,int(dims[0]*scale)),std::max(8,int(dims[1]*scale)),std::max(8,int(dims[2]*scale))});};
    if(!fitsMemory3D(estimateModelMemory3D(candidate(0)),kind,available))return false;
    double low=0,high=1;for(int n=0;n<40;++n){const double middle=(low+high)*.5;if(fitsMemory3D(estimateModelMemory3D(candidate(middle)),kind,available))low=middle;else high=middle;}
    fitted=candidate(fitsMemory3D(estimateModelMemory3D(candidate(1)),kind,available)?1:low);return true;
}
}
