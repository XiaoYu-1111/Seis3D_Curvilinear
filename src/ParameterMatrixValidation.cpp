#include "Seis3D/CurvilinearSolver.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
namespace {
struct GridCase {int nx,ny,nz;double h;};
struct Outcome {bool okay{};std::string detail;double finalEnergy{},peak{},tail{},dt{},ppw{},sourceLimit{};int steps{};std::vector<ReceiverTrace3D> traces;};
Outcome run(const SimulationParameters& p,BackendKind3D kind) {
    Outcome o;
    try {
        std::string error;auto s=createSimulationBackend3D(kind,error);
        if(!s||!s->initialize(p,error,{2}))throw std::runtime_error(error);
        o.dt=s->dt();o.steps=s->totalSteps();o.ppw=s->diagnostics().pointsPerShortestWavelength;o.sourceLimit=s->diagnostics().sourceDtLimit;
        if(s->dt()>s->diagnostics().waveDtLimit||s->dt()>s->diagnostics().dampingDtLimit||s->dt()>o.sourceLimit)throw std::runtime_error("Time-step ceiling exceeded");
        if(s->diagnostics().stepsPerDominantPeriod<50-1e-10)throw std::runtime_error("Source time sampling below 50 steps per f0 period");
        while(!s->completed()) {
            if(!s->advance(std::min(32,s->totalSteps()-s->currentStep()),error))throw std::runtime_error(error);
            const auto& d=s->diagnostics();
            if(!d.allFinite||!std::isfinite(d.elasticEnergy)||d.maxTractionRateRelative>1e-8)throw std::runtime_error("Invalid fields/energy/surface-rate residual");
            o.peak=std::max(o.peak,d.peakEnergy);o.finalEnergy=d.elasticEnergy;
            if(s->currentStep()*s->dt()>0.8*p.duration)o.tail=std::max(o.tail,d.elasticEnergy);
        }
        if(o.peak<=0||!std::isfinite(o.peak))throw std::runtime_error("No finite source response");
        if(o.tail>0.3*o.peak)throw std::runtime_error("Late energy exceeds 30% of driven peak");
        for(const auto& r:s->receivers())if(r.samples.size()!=static_cast<std::size_t>(s->currentStep()+1))throw std::runtime_error("Receiver time-axis mismatch");
        o.traces=s->receivers();o.okay=true;o.detail=o.ppw<5?"stable_under_resolved":"stable_sampling_adequate";
    }catch(const std::exception& e){o.detail=e.what();}
    return o;
}
double difference(const Outcome& a,const Outcome& b) {
    if(a.traces.size()!=b.traces.size())return 1;
    double peak=0,diff=0;
    for(std::size_t r=0;r<a.traces.size();++r) {
        if(a.traces[r].samples.size()!=b.traces[r].samples.size())return 1;
        for(std::size_t t=0;t<a.traces[r].samples.size();++t){peak=std::max(peak,std::abs(a.traces[r].samples[t]));diff=std::max(diff,std::abs(a.traces[r].samples[t]-b.traces[r].samples[t]));}
    }
    return diff/std::max(peak,1e-30);
}
}
int main() {
    const auto root=std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"validation";
    std::filesystem::create_directories(root);std::ofstream csv(root/"parameter_matrix.csv");
    if(!csv){std::cerr<<"Could not open matrix report\n";return 1;}
    csv<<std::setprecision(17)<<"backend,nx,ny,nz,spacing_m,f0_hz,duration_s,dt_s,steps,ppw_at_2_5_f0,source_dt_limit_s,peak_energy_j,final_energy_j,tail_peak_ratio,relative_cpu_cuda_error,passed,result\n";
    const bool cuda=queryBackend3D(BackendKind3D::Cuda).available;int cases=0,failures=0,unresolved=0;
    for(const auto g:{GridCase{8,8,8,10},GridCase{12,10,16,20},GridCase{24,20,28,10},GridCase{40,32,36,8}})for(double f:{5.0,12.0,30.0,80.0}) {
        SimulationParameters p;p.maxSteps=0;p.nx=g.nx;p.ny=g.ny;p.nz=g.nz;p.spacing=g.h;p.dominantFrequency=f;
        p.topographyAmplitude=std::min(12.0,0.08*(g.nz-1)*g.h);p.topographyWavelength=240;
        // Includes the complete Ricker pulse, then at least 1.5 S-wave box crossing times.
        p.duration=3.5/f+1.5*std::max({(g.nx-1)*g.h,(g.ny-1)*g.h,(g.nz-1)*g.h})/p.vs;
        const auto cpu=run(p,BackendKind3D::Cpu);
        Outcome gpu;double relative=0;if(cuda){gpu=run(p,BackendKind3D::Cuda);if(cpu.okay&&gpu.okay)relative=difference(cpu,gpu);}
        for(auto kind:{BackendKind3D::Cpu,BackendKind3D::Cuda}) {
            if(kind==BackendKind3D::Cuda&&!cuda)continue;
            const auto& o=kind==BackendKind3D::Cpu?cpu:gpu;
            const bool passed=o.okay&&relative<1e-9;
            csv<<(kind==BackendKind3D::Cpu?"CPU":"CUDA")<<','<<g.nx<<','<<g.ny<<','<<g.nz<<','<<g.h<<','<<f<<','<<p.duration<<','<<o.dt<<','<<o.steps<<','<<o.ppw<<','<<o.sourceLimit<<','<<o.peak<<','<<o.finalEnergy<<','<<o.tail/std::max(o.peak,1e-30)<<','<<relative<<','<<passed<<",\""<<o.detail<<"\"\n";
            csv.flush();++cases;if(!passed)++failures;if(o.ppw<5)++unresolved;
            std::cout<<(passed?"PASS ":"FAIL ")<<(kind==BackendKind3D::Cpu?"CPU ":"CUDA ")<<g.nx<<'x'<<g.ny<<'x'<<g.nz<<" h="<<g.h<<" f0="<<f<<" ppw="<<o.ppw<<" tail/peak="<<o.tail/std::max(o.peak,1e-30)<<" backend_error="<<relative<<" "<<o.detail<<std::endl;
        }
    }
    std::cout<<"Matrix: "<<cases<<" runs, "<<failures<<" failed, "<<unresolved<<" stable-or-tested under-resolved runs. CUDA "<<(cuda?"tested":"unavailable; CPU only")<<'\n';
    return failures?1:0;
}
