#include "Seis3D/CurvilinearSolver.hpp"
#include "Seis3D/ViewState3D.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace Seis3D::Curvilinear;
// Fails with a concrete diagnostic instead of relying on debug-only assertions.
void require3D(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
// Measures physical-gradient error of an analytic smooth field on the same wavy domain at two resolutions.
double manufacturedGradientError3D(int physicalNodes) {
    Grid3D grid;grid.nx=grid.ny=grid.nz=physicalNodes+2*ghostWidth3D;grid.dx=grid.dy=grid.dz=200.0/(physicalNodes-1);
    grid.topographyAmplitude=10;grid.topographyWavelength=200;grid.terrainTransitionDepth=120;std::string error;
    require3D(grid.buildGeometry(error),"Manufactured curvilinear grid failed");
    const double waveNumber=2.0*3.14159265358979323846/200.0;
    std::vector<double> values(grid.coordinates.size());
    for(std::size_t q=0;q<values.size();++q){const auto& p=grid.coordinates[q];values[q]=std::sin(waveNumber*p[0])*std::cos(waveNumber*p[2]);}
    double maximum=0;
    for(int k=grid.ghost;k<grid.nz-grid.ghost;++k)for(int j=grid.ghost;j<grid.ny-grid.ghost;++j)for(int i=grid.ghost;i<grid.nx-grid.ghost;++i) {
        const auto q=grid.index(i,j,k);std::array<double,3> d{};
        for(int axis=0;axis<3;++axis) {
            const int di=axis==0?1:0,dj=axis==1?1:0,dk=axis==2?1:0;
            d[axis]=(values[grid.index(i-2*di,j-2*dj,k-2*dk)]-8*values[grid.index(i-di,j-dj,k-dk)]+8*values[grid.index(i+di,j+dj,k+dk)]-values[grid.index(i+2*di,j+2*dj,k+2*dk)])/(12*grid.dx);
        }
        const auto& p=grid.coordinates[q];
        const std::array<double,3> exact{waveNumber*std::cos(waveNumber*p[0])*std::cos(waveNumber*p[2]),0,-waveNumber*std::sin(waveNumber*p[0])*std::sin(waveNumber*p[2])};
        const auto& m=grid.inverseMetrics[q];
        for(int axis=0;axis<3;++axis)maximum=std::max(maximum,std::abs(m[axis]*d[0]+m[3+axis]*d[1]+m[6+axis]*d[2]-exact[axis])/waveNumber);
    }
    return maximum;
}
int main() {
    try {
        std::string error;
        require3D(defaultBackend3D()==(queryBackend3D(BackendKind3D::Cuda).available?BackendKind3D::Cuda:BackendKind3D::Cpu),"Application default must prefer usable CUDA");
        Grid3D flat;flat.nx=flat.ny=flat.nz=16;flat.topographyAmplitude=0;
        require3D(flat.buildGeometry(error),"Flat geometry build failed");
        double metricError=0;
        for(int k=flat.ghost;k<flat.nz-flat.ghost;++k)for(int j=flat.ghost;j<flat.ny-flat.ghost;++j)for(int i=flat.ghost;i<flat.nx-flat.ghost;++i) {
            const auto q=flat.index(i,j,k);metricError=std::max(metricError,std::abs(flat.jacobian[q]-1));
            for(int m=0;m<9;++m)metricError=std::max(metricError,std::abs(flat.inverseMetrics[q][m]-(m%4==0?1.0:0.0)));
        }
        require3D(metricError<1e-12,"Flat geometry does not recover Cartesian derivatives");
        const double coarseError=manufacturedGradientError3D(21),fineError=manufacturedGradientError3D(41);
        require3D(coarseError/fineError>10,"Curvilinear physical gradients failed the refinement check");
        static_assert(std::is_trivially_copyable_v<GridLayout3D>);
        require3D(SimulationParameters{}.maxSteps==5000,"Initial maximum steps must be 5000");
        require3D(SimulationParameters{}.cflSafety==.8,"Default DRP CFL coefficient must be 0.8");
        SimulationParameters p;p.maxSteps=0;p.nx=18;p.ny=16;p.nz=18;p.duration=0.16;
        CpuSimulation3D a,b,flatSimulation;
#ifdef SEIS3D_HAS_OPENMP
        require3D(a.initialize(p,error,{2}),"Parallel curved session initialization failed");
#else
        require3D(a.initialize(p,error,{1}),"Curved session initialization failed");
#endif
        require3D(b.initialize(p,error,{1}),"Serial curved session initialization failed");
        auto flatParameters=p;flatParameters.topographyAmplitude=0;
        require3D(flatSimulation.initialize(flatParameters,error),"Flat session initialization failed");
        require3D(a.diagnostics().waveDtLimit<flatSimulation.diagnostics().waveDtLimit&&a.dt()<=flatSimulation.dt(),"Curvilinear wave ceiling did not react to distortion");
        {
            CpuSimulation3D capped,reference;auto limited=p;limited.maxSteps=5;
            require3D(capped.initialize(limited,error,{1}) && reference.initialize(p,error,{1}),"Step-cap initialization failed");
            require3D(capped.totalSteps()==5 && capped.dt()==reference.dt(),"Step cap changed CFL dt");
            require3D(capped.advance(100,error) && reference.advance(5,error),"Capped advance failed");
            require3D(capped.completed() && capped.currentStep()==5 && capped.receivers().front().samples.size()==6,"Step cap overran fields or receiver timeline");
            for(int f=0;f<9;++f)require3D(capped.field(static_cast<Field3D>(f))==reference.field(static_cast<Field3D>(f)),"Cap changed the physical solution");
            const auto revision=capped.revision();limited.maxSteps=-1;
            require3D(!capped.initialize(limited,error) && capped.revision()==revision,"Invalid cap discarded active session");
            limited.maxSteps=20001;require3D(!capped.initialize(limited,error),"Excessive step cap accepted");
            limited.maxSteps=20000;require3D(capped.initialize(limited,error) && capped.totalSteps()==reference.totalSteps(),"Large cap extended duration");
            limited.duration=10;limited.spacing=0.01;limited.topographyAmplitude=0;limited.maxSteps=1;
            require3D(capped.initialize(limited,error) && capped.totalSteps()==1,"Explicit cap failed for long duration");
            std::cout<<"Step-cap PASS: CFL preserved, exact fields, overshoot blocked, receiver timeline, invalid transaction, duration priority\n";
        }
        require3D(a.advance(a.totalSteps(),error),"Whole-batch advance failed");
        while(!b.completed())require3D(b.advance(7,error),"Incremental advance failed");
        for(int f=0;f<9;++f)require3D(a.field(static_cast<Field3D>(f))==b.field(static_cast<Field3D>(f)),"Parallelism or frame batch size changed the computed wavefield");
        require3D(a.diagnostics().allFinite && a.diagnostics().maxAbsVelocity>1e-8,"No finite nonzero elastic response");
        require3D(a.diagnostics().maxTractionRateRelative<1e-9,"Zero-traction-rate surface closure failed");
        const auto& traces=a.receivers();
        require3D(!traces.empty() && traces.front().samples.size()==static_cast<std::size_t>(a.currentStep()+1),"Receiver timeline does not include time zero");
        for(const auto& trace:traces)require3D(trace.samplesVx.size()==trace.samples.size()&&trace.samplesVy.size()==trace.samples.size(),"Three-component receiver timelines differ");
        {
            SimulationParameters sourceTest;sourceTest.nx=sourceTest.ny=12;sourceTest.nz=16;sourceTest.topographyAmplitude=0;sourceTest.maxSteps=1;
            sourceTest.sourceX=3;sourceTest.sourceY=3;sourceTest.sourceZ=4;CpuSimulation3D moved;
            require3D(moved.initialize(sourceTest,error,{1})&&moved.advance(1,error),"Moved source failed");
            const auto& g=moved.grid();const auto q=g.index(3+g.ghost,3+g.ghost,4+g.ghost);
            require3D(std::abs(moved.field(Field3D::Vz)[q])>0,"Moved source did not inject at its selected center");
            const auto sourcePosition=moved.snapshot().sourcePosition;require3D(sourcePosition==std::array<double,3>{30,30,40},"Export source coordinates do not match the configured location");
            const auto revision=moved.revision();sourceTest.sourceZ=0;
            require3D(!moved.initialize(sourceTest,error)&&moved.revision()==revision,"Source kernel crossed surface or invalid source discarded session");
            sourceTest.sourceZ=4;sourceTest.sourceX=-2;require3D(!moved.initialize(sourceTest,error),"Invalid source auto-center sentinel accepted");
        }
        const auto savedStep=a.currentStep();auto invalid=p;invalid.topographyWavelength=std::numeric_limits<double>::quiet_NaN();
        require3D(!a.initialize(invalid,error) && a.currentStep()==savedStep,"Invalid initialization discarded the previous session");
        invalid=p;invalid.vp=invalid.vs;require3D(!a.initialize(invalid,error),"Non-positive bulk modulus accepted");
        invalid=p;invalid.nx=invalid.ny=invalid.nz=301;require3D(!a.initialize(invalid,error),"300-node axis limit not enforced");
        const auto& grid=a.grid();const double bottom=(p.nz-1)*p.spacing;
        for(int k=grid.ghost;k<grid.nz-grid.ghost;++k)for(int j=grid.ghost;j<grid.ny-grid.ghost;++j)for(int i=grid.ghost;i<grid.nx-grid.ghost;++i) {
            if((k-grid.ghost)*p.spacing>=grid.terrainTransitionDepth)require3D(grid.coordinates[grid.index(i,j,k)][2]==(k-grid.ghost)*p.spacing,"Deep grid layer is not horizontal");
            if(k==grid.nz-grid.ghost-1)require3D(grid.coordinates[grid.index(i,j,k)][2]==bottom,"Bottom surface is not horizontal");
        }
        DisplayFrame3D frame;DisplayRequest3D request{p.nx-1,p.ny/2,p.nz-1,0};
        require3D(a.readDisplayFrame(request,frame,error),"Backend slice acquisition failed");
        require3D(frame.bytes==sizeof(float)*static_cast<std::size_t>(p.nx*p.nz+p.ny*p.nz+p.nx*p.ny),"Slice payload unexpectedly contains volume data");
        require3D(frame.planes[0].values[0]==static_cast<float>(a.field(Field3D::Vz)[grid.index(grid.ghost,request.y+grid.ghost,grid.ghost)]),"XZ slice indexing failed");
        require3D(frame.planes[1].values[0]==static_cast<float>(a.field(Field3D::Vz)[grid.index(request.x+grid.ghost,grid.ghost,grid.ghost)]),"YZ slice indexing failed");
        require3D(frame.planes[2].values[0]==static_cast<float>(a.field(Field3D::Vz)[grid.index(grid.ghost,grid.ghost,request.z+grid.ghost)]),"XY slice indexing failed");
        auto badRequest=request;badRequest.x=p.nx;require3D(!a.readDisplayFrame(badRequest,frame,error),"Out-of-range display request accepted");
        if(!queryBackend3D(BackendKind3D::Cuda).available)require3D(!createSimulationBackend3D(BackendKind3D::Cuda,error),"Unavailable CUDA backend silently selected another engine");
        auto unsafe=p;unsafe.cflSafety=maxCflCoefficient3D+.01;
        require3D(!a.initialize(unsafe,error)&&a.currentStep()==savedStep,"Unsafe CFL accepted or discarded session");
        unsafe.cflSafety=std::numeric_limits<double>::quiet_NaN();require3D(!a.initialize(unsafe,error),"Nonfinite CFL accepted");
        auto safer=p;safer.cflSafety=0.11;CpuSimulation3D safe;
        require3D(safe.initialize(safer,error)&&safe.dt()<=a.dt()*0.501,"CFL control failed to shorten dt");
        require3D(a.dt()<=a.diagnostics().waveDtLimit&&a.dt()<=a.diagnostics().dampingDtLimit&&a.diagnostics().maxDampingDt<=0.5,"Combined wave/sponge limits violated");
        SliceIndices3D slice{p.nx/2,p.ny/2,3};std::array<SliceView3D,3> views;views[0].zoom=4;views[0].pan={40,-20};int receiver=5;
        auto resized=p;resized.nx=42;resized.ny=12;resized.nz=36;
        require3D(reconcileViews3D(&p,resized,24,24,slice,views,receiver,true),"Geometry resize did not trigger fit");
        require3D(slice.x==remapIndex3D(p.nx/2,p.nx,42) && views[0].zoom==1 && views[0].pan[0]==0 && receiver==5,"Geometry reset policy lost position or framing");
        views[1].zoom=2.5f;views[1].pan={17,22};auto materialOnly=resized;materialOnly.vp+=200;
        require3D(!reconcileViews3D(&resized,materialOnly,24,24,slice,views,receiver,true) && views[1].zoom==2.5f && views[1].pan[0]==17,"Material update reset view state");
        auto narrow=materialOnly;narrow.nx=8;narrow.ny=8;narrow.nz=8;
        require3D(!reconcileViews3D(&materialOnly,narrow,24,4,slice,views,receiver,false) && views[1].zoom==2.5f && slice.x<8 && slice.y<8 && slice.z<8 && receiver<4,"Keep-view or shrinking-grid policy failed");
        std::cout<<"workers="<<a.backendInfo().workerThreads<<"; OpenMP="<<a.backendInfo().openMpAvailable<<"; slice bytes="<<frame.bytes<<"; deep layers/bottom flat; resize policies PASS\n";
        std::cout<<"PASS: flat metrics, metric-aware CFL, serial/parallel equivalence, finite elastic response, surface traction-rate, receivers, backend display contract, resize policies\n";
        std::cout<<"metric error="<<metricError<<"; traction-rate relative="<<a.diagnostics().maxTractionRateRelative<<"; peak speed="<<a.diagnostics().maxAbsVelocity<<'\n';
        std::cout<<"manufactured gradient error coarse="<<coarseError<<" fine="<<fineError<<" ratio="<<coarseError/fineError<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
