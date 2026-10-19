// Production API wall-clock benchmark. No changes to solver diagnostics/synchronization.
#include "Seis2D/CurvilinearSolver2D.hpp"
#include "Seis3D/CurvilinearSolver.hpp"
#include <windows.h>
#include <intrin.h>
#include <xmmintrin.h>
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace Seis3D::Curvilinear;
using Clock = std::chrono::steady_clock;
struct Options {std::filesystem::path output; int threads=32,repeats=5,warmup=20,steps=100,dimension=0,size=0;};
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
std::string quote(const std::string& s){std::string out="\"";for(char c:s){if(c=='"'||c=='\\')out+='\\';if(static_cast<unsigned char>(c)>=32)out+=c;}return out+'"';}
double quantile(std::vector<double> x,double q){std::sort(x.begin(),x.end());double p=q*(x.size()-1);auto i=static_cast<size_t>(p);return std::lerp(x[i],x[std::min(i+1,x.size()-1)],p-i);}
struct Fields {std::vector<std::vector<double>> wave,receivers;double dt{};};
struct Error {double l2{},maxScaled{},maxAbs{},peak{};};
Error compare(const std::vector<std::vector<double>>& a,const std::vector<std::vector<double>>& b){
    check(a.size()==b.size(),"Buffer count mismatch");Error e;
    for(size_t f=0;f<a.size();++f){check(a[f].size()==b[f].size(),"Buffer size mismatch");long double norm=0,diff=0;double peak=0,max=0;
        for(size_t i=0;i<a[f].size();++i){double x=a[f][i],y=b[f][i];check(std::isfinite(x)&&std::isfinite(y),"Nonfinite samples");double d=x-y;norm+=static_cast<long double>(x)*x;diff+=static_cast<long double>(d)*d;peak=std::max(peak,std::abs(x));max=std::max(max,std::abs(d));}
        if(norm>0)e.l2=std::max(e.l2,std::sqrt(static_cast<double>(diff/norm)));
        e.maxScaled=std::max(e.maxScaled,max/std::max(peak,1e-25));e.maxAbs=std::max(e.maxAbs,max);e.peak=std::max(e.peak,peak);
    }
    check(e.l2<1e-9&&e.maxScaled<1e-9,"CPU/CUDA error exceeds 1e-9");return e;
}
void metadata(const Options& o){
    int regs[4];char brand[49]{};for(int i=0;i<3;++i){__cpuid(regs,static_cast<int>(0x80000002u+i));memcpy(brand+16*i,regs,16);}
    MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);check(GlobalMemoryStatusEx(&memory),"RAM query failed");
    SYSTEM_POWER_STATUS power{};bool powerKnown=GetSystemPowerStatus(&power)!=0;
    DWORD bytes=0;GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&bytes);std::vector<unsigned char> storage(bytes);int cores=0;
    if(GetLogicalProcessorInformationEx(RelationProcessorCore,reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(storage.data()),&bytes))for(DWORD pos=0;pos<bytes;){auto p=reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(storage.data()+pos);++cores;pos+=p->Size;}
    bool priority=SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS)!=0;
    int actual=0;unsigned csr=0;
    omp_set_dynamic(0);omp_set_num_threads(o.threads);
    #pragma omp parallel num_threads(o.threads)
    {
        _mm_setcsr(_mm_getcsr()&~0x8040u); // Preserve subnormals, as in Cartesian formal run.
        #pragma omp single
        {actual=omp_get_num_threads();csr=_mm_getcsr();}
    }
    check(actual==o.threads,"OpenMP worker count mismatch");
    auto inventory=queryCudaDevices3D();auto selected=selectCudaDevice3D(inventory);check(selected.available,selected.description);
    std::ofstream out(o.output/"hardware.json");out<<"{\n\"cpu\":"<<quote(brand)<<",\"physical_cores\":"<<cores<<",\"logical_processors\":"<<omp_get_num_procs()<<",\"ram_bytes\":"<<memory.ullTotalPhys
        <<",\"available_ram_bytes\":"<<memory.ullAvailPhys<<",\"threads\":"<<actual<<",\"mxcsr\":"<<csr<<",\"flush_subnormals\":false,\"ac_power\":"<<(powerKnown?std::to_string(power.ACLineStatus):"null")
        <<",\"above_normal_priority\":"<<(priority?"true":"false")<<",\"gpu\":"<<quote(selected.description)<<",\"msvc\":"<<_MSC_VER<<",\"precision\":\"Float64\",\"repeats\":"<<o.repeats<<",\"warmup\":"<<o.warmup<<",\"steps\":"<<o.steps
        <<",\"timed_scope\":\"production advance: RK4, curved metrics, sponge/free surface, rollback buffers, diagnostics and one receiver; no initialization/reset/render/export/snapshot\",\"gpu_sync\":\"production per-step diagnostics and sampling synchronize; advance batch=1\"}\n";
    std::cout<<brand<<" threads="<<actual<<" GPU="<<selected.description<<std::endl;
}
struct Suite {
    Options o;std::ofstream trials,summary;
    explicit Suite(Options options):o(std::move(options)){
        std::filesystem::create_directories(o.output);trials.open(o.output/"trials.csv");summary.open(o.output/"summary.csv");
        check(bool(trials)&&bool(summary),"Cannot create result files");trials<<std::setprecision(14);summary<<std::setprecision(14);
        trials<<"dimension,size,physical_nodes,padded_nodes,backend,threads,repeat,init_ms,dt_s,total_ms,ms_per_step,steps\n";
        summary<<"dimension,size,physical_nodes,padded_nodes,backend,threads,init_ms,dt_s,median_ms_step,p25_ms_step,p75_ms_step,min_ms_step,max_ms_step,steps_per_second,physical_mnodes_per_second,cpu_over_cuda,field_relative_l2,field_max_scaled,receiver_relative_l2\n";
    }
    template<class Advance,class Reset,class Capture>
    std::pair<std::vector<double>,Fields> measure(int dim,int size,BackendKind3D backend,size_t padded,double init,double dt,int workers,Advance advance,Reset reset,Capture capture){
        const bool gpu=backend==BackendKind3D::Cuda;const char* name=gpu?"CUDA":"CPU";std::vector<double> values;std::string error;
        for(int repeat=0;repeat<o.repeats;++repeat){
            if(gpu){std::this_thread::sleep_for(std::chrono::milliseconds(600));auto heat=Clock::now();do{check(reset(error),error);check(advance(std::max(1,o.warmup),error),error);}while(elapsed(heat)<300);}
            check(reset(error),error);check(advance(o.warmup,error),error);auto start=Clock::now();
            for(int n=0;n<o.steps;++n)check(advance(1,error),error);
            double ms=elapsed(start);values.push_back(ms/o.steps);size_t physical=static_cast<size_t>(size)*size*(dim==3?size:1);
            trials<<dim<<','<<size<<','<<physical<<','<<padded<<','<<name<<','<<workers<<','<<repeat+1<<','<<init<<','<<dt<<','<<ms<<','<<ms/o.steps<<','<<o.steps<<'\n';trials.flush();
            std::cout<<dim<<"D "<<size<<' '<<name<<" repeat="<<repeat+1<<" ms/step="<<ms/o.steps<<std::endl;
        }
        return {values,capture()};
    }
    void run(int dim,int size){
        std::vector<double> cpu,gpu;Fields reference,device;std::array<double,2> init{},dt{};size_t padded=0;
        for(int mode=0;mode<2;++mode){auto kind=mode?BackendKind3D::Cuda:BackendKind3D::Cpu;std::string error;ComputeSettings3D settings{o.threads,-1};
            if(dim==2){
                Seis2D::Curvilinear::Parameters p;p.nx=p.nz=size;p.spacing=10;p.relief=20;p.wavelength=300;p.frequency=30;p.duration=10;p.maxSteps=o.warmup+o.steps;p.sourceZ=size/4;
                p.layered=true;p.layers={{(size/2)*10.,3000,1700,2200},{0,4200,2400,2500}};p.receiverStart=size/2;p.receiverCount=1;p.receiverStride=1;p.receiverDepth=1;
                Seis2D::Curvilinear::Simulation s;auto start=Clock::now();check(s.initialize(p,error,kind,settings),error);init[mode]=elapsed(start);dt[mode]=s.dt();padded=s.grid().coordinates.size();
                check(s.backend().kind==kind&&s.totalSteps()==p.maxSteps,"Unexpected backend or step cap");if(!mode)check(s.backend().workerThreads==o.threads,"CPU worker mismatch");
                auto result=measure(dim,size,kind,padded,init[mode],dt[mode],mode?0:o.threads,[&](int n,std::string& e){return s.advance(n,e);},[&](std::string& e){return s.reset(e);},[&]{check(s.step()==p.maxSteps&&s.diagnostics().finite,"2D incomplete or nonfinite run");Fields f;auto shot=s.snapshot();for(auto& field:shot.fields)f.wave.push_back(std::move(field));for(auto& r:s.receivers()){f.receivers.push_back(r.vx);f.receivers.push_back(r.vz);}return f;});
                if(mode){gpu=std::move(result.first);device=std::move(result.second);}else{cpu=std::move(result.first);reference=std::move(result.second);}
            }else{
                SimulationParameters p;p.nx=p.ny=p.nz=size;p.spacing=10;p.topographyAmplitude=20;p.topographyWavelength=300;p.dominantFrequency=30;p.duration=10;p.maxSteps=o.warmup+o.steps;p.sourceZ=size/4;
                p.layeredMaterial=true;p.layerDepthMode=LayerDepthMode3D::GridIndex;p.layers={{"upper",size/2*10.,3000,1700,2200},{"lower",0,4200,2400,2500}};p.receiverArray.kind=ReceiverArrayKind3D::XLine;p.receiverArray.startX=size/2;p.receiverArray.countX=1;p.receiverArray.z=1;p.absorbingBoundary.cells={16,16,16};
                auto start=Clock::now();auto s=createSimulationBackend3D(kind,error);check(s&&s->initialize(p,error,settings),error);init[mode]=elapsed(start);dt[mode]=s->dt();padded=s->grid().coordinates.size();
                check(s->backendInfo().kind==kind&&s->totalSteps()==p.maxSteps,"Unexpected backend or step cap");if(!mode)check(s->backendInfo().workerThreads==o.threads,"CPU worker mismatch");
                auto result=measure(dim,size,kind,padded,init[mode],dt[mode],mode?0:o.threads,[&](int n,std::string& e){return s->advance(n,e);},[&](std::string& e){return s->resetFields(e);},[&]{check(s->currentStep()==p.maxSteps&&s->diagnostics().allFinite,"3D incomplete or nonfinite run");Fields f;auto shot=s->snapshot();for(auto* field:{&shot.vx,&shot.vy,&shot.vz,&shot.sxx,&shot.syy,&shot.szz,&shot.sxy,&shot.sxz,&shot.syz})f.wave.push_back(std::move(*field));for(auto& r:s->receivers()){f.receivers.push_back(r.samplesVx);f.receivers.push_back(r.samplesVy);f.receivers.push_back(r.samples);}return f;});
                if(mode){gpu=std::move(result.first);device=std::move(result.second);}else{cpu=std::move(result.first);reference=std::move(result.second);}
            }
        }
        check(dt[0]==dt[1],"CPU/CUDA dt mismatch");auto fields=compare(reference.wave,device.wave),receivers=compare(reference.receivers,device.receivers);check(fields.peak>1e-25,"Wavefield is zero");
        for(int mode=0;mode<2;++mode){auto& x=mode?gpu:cpu;double ms=quantile(x,.5);size_t physical=static_cast<size_t>(size)*size*(dim==3?size:1);
            summary<<dim<<','<<size<<','<<physical<<','<<padded<<','<<(mode?"CUDA":"CPU")<<','<<(mode?0:o.threads)<<','<<init[mode]<<','<<dt[mode]<<','<<ms<<','<<quantile(x,.25)<<','<<quantile(x,.75)<<','<<*std::min_element(x.begin(),x.end())<<','<<*std::max_element(x.begin(),x.end())<<','<<1000/ms<<','<<physical/1000./ms<<','<<quantile(cpu,.5)/ms<<','<<fields.l2<<','<<fields.maxScaled<<','<<receivers.l2<<'\n';summary.flush();}
        std::cout<<"PASS "<<dim<<"D "<<size<<" fields L2="<<fields.l2<<" receivers L2="<<receivers.l2<<std::endl;
    }
};
int main(int argc,char** argv){try{Options o;for(int i=1;i<argc;i+=2){check(i+1<argc,"Missing option value");std::string a=argv[i],v=argv[i+1];if(a=="--output")o.output=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(v.data()),v.size()));else if(a=="--threads")o.threads=std::stoi(v);else if(a=="--repeats")o.repeats=std::stoi(v);else if(a=="--steps")o.steps=std::stoi(v);else if(a=="--warmup")o.warmup=std::stoi(v);else if(a=="--dimension")o.dimension=std::stoi(v);else if(a=="--size")o.size=std::stoi(v);else throw std::runtime_error("Unknown option "+a);}
    auto relative=o.output.lexically_normal().lexically_relative(std::filesystem::path(SEIS3D_OUTPUT_ROOT));
    check(!o.output.empty()&&o.output.is_absolute()&&!relative.empty()&&*relative.begin()!=".."&&relative!=".","Output must be an absolute project out subdirectory");
    check(o.threads>0&&o.threads<=omp_get_num_procs()&&o.steps>0&&o.warmup>=0&&o.warmup+o.steps<=20000&&o.repeats>=3&&o.repeats%2==1,"Invalid settings");
    Suite suite(o);metadata(o);if(o.dimension==0||o.dimension==2)for(int size:o.size?std::vector<int>{o.size}:std::vector<int>{256,1024,2048})suite.run(2,size);if(o.dimension==0||o.dimension==3)for(int size:o.size?std::vector<int>{o.size}:std::vector<int>{48,96,160})suite.run(3,size);std::ofstream(o.output/"COMPLETE.txt")<<"All timing trials and full-field/receiver comparisons passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<std::endl;return 1;}}
