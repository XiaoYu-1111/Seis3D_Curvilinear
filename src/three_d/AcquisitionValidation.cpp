#include "Seis3D/ReceiverGeometry3D.hpp"
#include "Seis3D/PerformanceRuns3D.hpp"
#include "Seis3D/ReceiverIo.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
namespace {
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
double difference(const std::vector<double>& a,const std::vector<double>& b){check(a.size()==b.size(),"Trace length mismatch");double peak=0,error=0;for(std::size_t q=0;q<a.size();++q){peak=std::max(peak,std::abs(a[q]));error=std::max(error,std::abs(a[q]-b[q]));}return error/std::max(peak,1e-30);}
PerformanceRun3D measured(const ISimulationBackend3D& s,std::uint64_t id){return {id,s.parameters(),s.backendInfo(),s.diagnostics().timing,s.dt(),s.currentStep(),static_cast<int>(s.receivers().size())};}
}
int main(){try{
    SimulationParameters p;p.nx=16;p.ny=12;p.nz=20;p.maxSteps=64;p.topographyAmplitude=0;
    std::vector<std::array<int,3>> nodes;std::string error;
    check(receiverNodes3D(p,nodes,error)&&nodes.size()==12&&nodes.front()==std::array<int,3>{2,6,1}&&nodes.back()==std::array<int,3>{13,6,1},"Automatic receiver line changed");
    p.receiverArray={ReceiverArrayKind3D::XLine,1,-1,4,5,1,3,1};check(receiverNodes3D(p,nodes,error)&&nodes.back()==std::array<int,3>{13,6,4},"Custom X line incorrect");
    p.receiverArray={ReceiverArrayKind3D::YLine,-1,1,2,1,5,1,2};check(receiverNodes3D(p,nodes,error)&&nodes.back()==std::array<int,3>{8,9,2},"Custom Y line incorrect");
    p.receiverArray={ReceiverArrayKind3D::GridXY,1,1,1,12,8,1,1};check(receiverNodes3D(p,nodes,error)&&nodes.size()==96&&nodes[12]==std::array<int,3>{1,2,1},"Receiver plane order incorrect");
    CpuSimulation3D cpu;check(cpu.initialize(p,error,{1}),error);check(cpu.receivers().size()==nodes.size(),"Backend ignored receiver geometry");
    for(std::size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];const auto q=cpu.grid().index(n[0]+cpu.grid().ghost,n[1]+cpu.grid().ghost,n[2]+cpu.grid().ghost);check(cpu.receivers()[i].gridIndex==q&&cpu.receivers()[i].position==cpu.grid().coordinates[q],"Receiver physical coordinates incorrect");}
    check(cpu.diagnostics().timing.measuredSteps==0&&cpu.diagnostics().timing.totalStepMs==0,"Initialization counted as time stepping");
    check(cpu.advance(17,error),error);const auto before=cpu.diagnostics().timing;check(before.measuredSteps==17&&before.totalStepMs>0&&before.minStepMs<=before.averageStepMs()&&before.averageStepMs()<=before.maxStepMs,"Accepted-step timing invalid");
    check(cpu.advance(0,error)&&cpu.diagnostics().timing.totalStepMs==before.totalStepMs,"No-op advance changed timing");
    const auto revision=cpu.revision();auto invalid=p;invalid.receiverArray.strideX=std::numeric_limits<int>::max();check(!cpu.initialize(invalid,error)&&cpu.revision()==revision&&cpu.diagnostics().timing.totalStepMs==before.totalStepMs,"Invalid receiver geometry discarded state/timing");
    invalid=p;invalid.receiverArray.z=p.nz;check(!cpu.initialize(invalid,error),"Receiver depth outside model accepted");invalid=p;invalid.receiverArray.countY=-1;check(!cpu.initialize(invalid,error),"Negative receiver count accepted");
    check(cpu.advance(100,error)&&cpu.currentStep()==64&&cpu.diagnostics().timing.measuredSteps==64,error);const auto full=cpu.diagnostics().timing;check(cpu.advance(10,error)&&cpu.diagnostics().timing.totalStepMs==full.totalStepMs,"Completed run kept accumulating timing");
    std::vector<PerformanceRun3D> runs{measured(cpu,1)};double maxError=0;
    if(queryBackend3D(BackendKind3D::Cuda).available){auto gpu=createSimulationBackend3D(BackendKind3D::Cuda,error);check(gpu&&gpu->initialize(p,error)&&gpu->advance(64,error),error);runs.push_back(measured(*gpu,2));check(comparableRuns3D(runs[0],runs[1]),"Identical CPU/CUDA runs not comparable");
        for(std::size_t r=0;r<nodes.size();++r){const auto& a=cpu.receivers()[r];const auto& b=gpu->receivers()[r];maxError=std::max({maxError,difference(a.samples,b.samples),difference(a.samplesVx,b.samplesVx),difference(a.samplesVy,b.samplesVy)});check(b.samples.size()==65,"CUDA plane timeline incomplete");}check(maxError<1e-8,"CUDA receiver plane lost traces");
        auto mismatch=runs[1];++mismatch.steps;check(!comparableRuns3D(runs[0],mismatch),"Different step counts compared");mismatch=runs[1];++mismatch.parameters.receiverArray.z;check(!comparableRuns3D(runs[0],mismatch),"Different receiver arrays compared");mismatch=runs[1];mismatch.parameters.dominantFrequency*=2;check(!comparableRuns3D(runs[0],mismatch),"Different source frequencies compared");
        mismatch=runs[1];++mismatch.parameters.absorbingBoundary.cells[0];check(!comparableRuns3D(runs[0],mismatch)&&configurationId3D(mismatch.parameters)!=configurationId3D(p),"Different sponge settings compared or given the same configuration ID");
    }
    const auto root=std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"validation";check(writePerformanceCsv3D(runs,root/"acquisition_timing.csv",error),error);check(writeReceiversCsv(cpu,root/"receiver_grid_3c.csv",error,true),error);
    check(cpu.initialize(p,error,{1})&&cpu.diagnostics().timing.measuredSteps==0&&cpu.diagnostics().timing.totalStepMs==0,"Rebuild retained previous timing");
    p.density=1e-300;p.maxSteps=1;check(cpu.initialize(p,error,{1}),error);check(!cpu.advance(1,error)&&cpu.diagnostics().timing.measuredSteps==0&&cpu.diagnostics().timing.totalStepMs==0,"Rejected step counted as accepted compute time");
    std::cout<<"PASS four receiver modes, 96-receiver CPU/CUDA grid, 3C relative error="<<maxError<<", complete-step timing, reset/rollback and comparable-run guards\n";
    for(const auto& r:runs)std::cout<<(r.backend.kind==BackendKind3D::Cpu?"CPU":"CUDA")<<" accepted steps="<<r.steps<<" compute="<<r.timing.totalStepMs<<" ms mean="<<r.timing.averageStepMs()<<" ms/step\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
