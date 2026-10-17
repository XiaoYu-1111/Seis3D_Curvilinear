#include "Seis3D/ModelPresets3D.hpp"
#include "Seis3D/TraceProcessing.hpp"
#include "Seis3D/ReceiverIo.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double compare(const std::vector<double>& a,const std::vector<double>& b) {
    check(a.size()==b.size(),"Receiver vector lengths differ");double peak=0,error=0;
    for(std::size_t i=0;i<a.size();++i){check(std::isfinite(a[i])&&std::isfinite(b[i]),"Nonfinite receiver");peak=std::max(peak,std::abs(a[i]));error=std::max(error,std::abs(a[i]-b[i]));}
    return error/std::max(peak,1e-30);
}
int main() {
    try {
        std::vector<double> values(20001),times(values.size());for(std::size_t i=0;i<times.size();++i)times[i]=i*.001;
        values[17]=9;values[18]=-11;values[19001]=3;values.back()=-2;
        const auto original=values;const auto reduced=TraceProcessing::reduceExtrema(values,times);
        check(reduced.values.size()<=2048&&reduced.values.front()==values.front()&&reduced.values.back()==values.back(),"Record reduction dropped endpoints or exceeded budget");
        check(*std::max_element(reduced.values.begin(),reduced.values.end())==9&&*std::min_element(reduced.values.begin(),reduced.values.end())==-11,"Record reduction lost signed impulse extrema");
        auto agc=TraceProcessing::AgcSettings{true};agc.maxBoost=20;auto processed=values;
        TraceProcessing::applyAgc(processed,times,agc);check(values==original,"Display processing changed raw samples");
        for(std::size_t i=0;i<values.size();++i)check(std::isfinite(processed[i])&&std::abs(processed[i])<=20*std::abs(values[i])+1e-14,"AGC exceeded boost cap or injected energy into silence");
        const std::vector<double> shortTimes{0,.1,.2},positive{0,2,0},negative{0,-3,0};
        const auto density=TraceProcessing::buildTraceDensity(2,4,0,.2,3,false,[&](int)->const auto&{return shortTimes;},[&](int c)->const auto&{return c==0?positive:negative;},[&](int c){return c==0?2.0:3.0;});
        check(*std::max_element(density.begin(),density.end())>=2.0/3&&*std::min_element(density.begin(),density.end())==-1,"Record density lost polarity or sample timing");
        const bool cuda=queryBackend3D(BackendKind3D::Cuda).available;
        {
            auto seed=SimulationParameters{};seed.layeredMaterial=true;seed.layers={{"old",0,1500,600,2000}};seed.dominantFrequency=50;seed.duration=.1;seed.maxSteps=3;seed.absorbingBoundary.enabled=false;
            const auto p=applyModelPreset3D(0,seed);
            check(p.nx==200&&p.ny==20&&p.nz==200&&p.spacing==10&&p.topographyAmplitude==20&&p.topographyWavelength==300&&p.terrainTransitionDepth==0,"Benchmark geometry does not match the figure");
            check(p.dominantFrequency==12&&sourceIndices3D(p)==std::array<int,3>{100,10,50}&&p.duration==.5&&p.maxSteps==5000&&p.cflSafety==defaultCflCoefficient3D,"Benchmark source/stop/CFL defaults incorrect");
            check(!p.layeredMaterial&&p.layers.empty()&&p.vp==3000&&p.vs==1700&&p.density==2200&&p.absorbingBoundary==AbsorbingBoundarySettings3D{true,{6,6,6},3}&&p.receiverArray==ReceiverArraySettings3D{},"Benchmark failed to reset material/acquisition/boundary settings");
            check(estimateModelMemory3D(p).withinBudget,"Benchmark exceeds the node budget");
            for(int other=1;other<static_cast<int>(modelPresets3D.size());++other){const auto smaller=applyModelPreset3D(other,p);const auto source=sourceIndices3D(smaller);check(source[0]>=2&&source[0]<=smaller.nx-3&&source[1]>=2&&source[1]<=smaller.ny-3&&source[2]>=2&&source[2]<=smaller.nz-3,"Source from benchmark outside a smaller preset");}
            std::string error;CpuSimulation3D cpu;check(cpu.initialize(p,error,{4})&&cpu.advance(2,error),error);
            check(cpu.totalSteps()==450&&cpu.receivers().size()==196&&std::abs(cpu.dt()-.5/450)<1e-15,"Benchmark time grid/receivers do not reproduce calibrated CFL");
            const auto& g=cpu.grid();check(std::abs(cpu.field(Field3D::Vz)[g.index(100+g.ghost,10+g.ghost,50+g.ghost)])>0,"Benchmark source was not injected at its configured location");
            if(cuda){CpuSimulation3D gpu(BackendKind3D::Cuda);check(gpu.initialize(p,error)&&gpu.advance(2,error),error);DisplayFrame3D a,b;check(cpu.readDisplayFrame({100,10,50,0},a,error)&&gpu.readDisplayFrame({100,10,50,0},b,error),error);for(int plane=0;plane<3;++plane)check(a.planes[plane].values==b.planes[plane].values,"Benchmark CPU/CUDA slice mismatch");}
            std::cout<<"PASS full benchmark preset, 450-step calibrated grid, selected source, CPU/CUDA slices, shrink-source fitting\n";
        }
        const auto output=std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"validation";std::filesystem::create_directories(output);std::ofstream report(output/"preset_validation.csv");
        report<<"nx,ny,nz,padded_nodes,cpu_estimate_mib,gpu_estimate_mib,steps,receivers,cpu_finite,cuda_tested,max_receiver_relative_error\n";
        for(int preset=1;preset<4;++preset) {
            SimulationParameters seed;seed.maxSteps=40;seed.dominantFrequency=12;
            const auto p=applyModelPreset3D(preset,seed);const auto memory=estimateModelMemory3D(p);
            check(memory.withinBudget&&p.maxSteps==40&&p.dominantFrequency==seed.dominantFrequency,"Preset exceeded limits or changed source/stop settings");
            std::cout<<"Testing preset "<<p.nx<<'x'<<p.ny<<'x'<<p.nz<<" CPU ~"<<memory.cpuMiB<<" MiB"<<std::endl;
            std::string error;CpuSimulation3D cpu;check(cpu.initialize(p,error),error);check(cpu.advance(40,error),error);
            check(cpu.currentStep()==40&&cpu.diagnostics().allFinite&&cpu.diagnostics().maxTractionRateRelative<1e-8,"CPU preset failed numerical guards");
            check(cpu.receivers().size()==static_cast<std::size_t>(p.nx-4),"Preset did not deploy all X receivers");
            double relative=0;
            if(cuda) {
                auto gpu=createSimulationBackend3D(BackendKind3D::Cuda,error);check(gpu&&gpu->initialize(p,error),error);check(gpu->advance(40,error),error);
                check(gpu->diagnostics().allFinite&&gpu->backendInfo().kind==BackendKind3D::Cuda,"CUDA preset was not computed on GPU");
                for(std::size_t r=0;r<cpu.receivers().size();++r) {
                    const auto& a=cpu.receivers()[r];const auto& b=gpu->receivers()[r];
                    relative=std::max({relative,compare(a.samples,b.samples),compare(a.samplesVx,b.samplesVx),compare(a.samplesVy,b.samplesVy)});
                }
                check(relative<1e-8,"Preset CPU/CUDA 3C receiver mismatch");
                const auto middle=gpu->receivers().size()/2;
                if(middle>32)check(std::any_of(gpu->receivers()[middle].samples.begin(),gpu->receivers()[middle].samples.end(),[](double x){return std::abs(x)>1e-20;}),"CUDA multi-block receiver sampling missed central traces");
                DisplayFrame3D frame;check(gpu->readDisplayFrame({p.nx/2,p.ny/2,3,0},frame,error),error);
                check(frame.bytes==sizeof(float)*static_cast<std::size_t>(p.nx*p.nz+p.ny*p.nz+p.nx*p.ny),"Large-preset slice buffer size mismatch");
            }
            if(preset==2) {
                check(writeReceiversCsv(cpu,output/"receivers_3c.csv",error,true),error);
                std::ifstream file(output/"receivers_3c.csv");std::string line;int lines=0;
                while(std::getline(file,line))if(!line.empty()&&line[0]!='#'){
                    check(std::count(line.begin(),line.end(),',')==3*static_cast<int>(cpu.receivers().size()),"3C CSV column count mismatch");
                    if(lines==0)check(line.find("receiver_1_Vx,receiver_1_Vy,receiver_1_Vz")!=std::string::npos,"3C CSV component order incorrect");++lines;
                }
                check(lines==cpu.currentStep()+2,"3C CSV time-zero timeline mismatch");
            }
            report<<p.nx<<','<<p.ny<<','<<p.nz<<','<<memory.points<<','<<memory.cpuMiB<<','<<memory.deviceMiB<<','<<cpu.currentStep()<<','<<cpu.receivers().size()<<",1,"<<cuda<<','<<relative<<'\n';report.flush();
            std::cout<<"PASS preset "<<p.nx<<'x'<<p.ny<<'x'<<p.nz<<" receivers="<<cpu.receivers().size()<<" 3C relative error="<<relative<<std::endl;
        }
        std::cout<<"PASS record processing, large presets, 3C timeline and raw export\n";
    } catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
