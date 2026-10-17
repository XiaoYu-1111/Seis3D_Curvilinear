#include "Seis3D/CurvilinearSolver.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void compare(const std::vector<double>& a,const std::vector<double>& b) {
    check(a.size()==b.size(),"Device field sizes differ");double peak=0,error=0;
    for(std::size_t q=0;q<a.size();++q){check(std::isfinite(a[q])&&std::isfinite(b[q]),"Nonfinite device field");peak=std::max(peak,std::abs(a[q]));error=std::max(error,std::abs(a[q]-b[q]));}
    check(error<=1e-9*std::max(peak,1e-25),"Device routing changed the numerical result");
}
void compare(const SimulationResult& a,const SimulationResult& b) {
    compare(a.vx,b.vx);compare(a.vy,b.vy);compare(a.vz,b.vz);
    compare(a.sxx,b.sxx);compare(a.syy,b.syy);compare(a.szz,b.szz);
    compare(a.sxy,b.sxy);compare(a.sxz,b.sxz);compare(a.syz,b.syz);
}
}
int main() {
    try {
        // Nonconsecutive indices and an unusable first device reproduce a common
        // hybrid/multiple-GPU case even on a single-GPU validation machine.
        CudaDeviceInventory3D synthetic;synthetic.description="Synthetic inventory";
        for(int index:{0,3,7}) {
            CudaDeviceInfo3D d;d.index=index;d.name="test "+std::to_string(index);
            d.available=index!=0;d.description=index==0?"Kernel unavailable":"Available";
            d.totalMemoryBytes=index==0?16000:index==3?6000:8000;
            d.freeMemoryBytes=index==3?5000:1000;synthetic.devices.push_back(d);
        }
        check(selectCudaDevice3D(synthetic).cudaDevice==7,"Auto did not scan every usable device");
        check(selectCudaDevice3D(synthetic,3).cudaDevice==3,"Explicit selection was ignored");
        for(int index:{0,2,-2})check(!selectCudaDevice3D(synthetic,index).available,"Invalid device silently fell back");
        check(selectCudaDevice3D(synthetic,0).description.find("Kernel unavailable")!=std::string::npos,"Device rejection lost its reason");
        synthetic.devices.clear();check(!selectCudaDevice3D(synthetic).available,"Empty inventory selected a GPU");
        std::cout<<"PASS: full inventory selection, explicit device, no silent fallback\n";

        for(const auto& adapter:queryGraphicsAdapters3D())std::cout<<"Display adapter: "<<adapter<<'\n';
        const auto inventory=queryCudaDevices3D();std::cout<<inventory.description<<'\n';
        std::set<int> indices;std::string error;
        SimulationParameters p;p.nx=18;p.ny=12;p.nz=20;p.topographyAmplitude=0;p.sourceZ=5;p.maxSteps=8;p.duration=.1;p.dominantFrequency=30;
        CpuSimulation3D cpu;check(cpu.initialize(p,error,{1}),error.c_str());
        std::vector<std::unique_ptr<ISimulationBackend3D>> sessions;
        for(const auto& device:inventory.devices) {
            check(indices.insert(device.index).second,"Duplicate CUDA device index");
            std::cout<<"GPU "<<device.index<<" | "<<device.name<<" | CC "<<device.computeMajor<<'.'<<device.computeMinor<<" | "<<device.description<<" | PCI "<<device.pciBusId<<'\n';
            if(!device.available)continue;
            auto gpu=createSimulationBackend3D(BackendKind3D::Cuda,error);check(gpu!=nullptr,error.c_str());
            check(gpu->initialize(p,error,{1,device.index}),error.c_str());
            check(gpu->backendInfo().cudaDevice==device.index,"Session metadata has the wrong CUDA device");
            check(queryBackendMemory3D(BackendKind3D::Cuda,device.index).deviceAvailableBytes>0,"Selected device memory query failed");
            sessions.push_back(std::move(gpu));
        }
        if(sessions.empty()) {
            check(!queryBackend3D(BackendKind3D::Cuda).available,"No usable device but CUDA backend reported available");
            std::cout<<"SKIP hardware execution: no usable CUDA device. Inventory policy and CPU initialization passed.\n";return 0;
        }
        // Keep every GPU session alive together; alternate computation with
        // enumeration/readback on other devices to exercise CUDA context guards.
        for(int step=0;step<8;++step) {
            check(cpu.advance(1,error),error.c_str());
            for(auto& gpu:sessions) {
                (void)queryCudaDevices3D();
                for(const auto& device:inventory.devices)if(device.available)(void)queryBackendMemory3D(BackendKind3D::Cuda,device.index);
                check(gpu->advance(1,error),error.c_str());
                DisplayFrame3D frame;check(gpu->readDisplayFrame({9,6,5,0},frame,error),error.c_str());
                check(frame.bytes>0,"GPU slice readback was empty");
            }
        }
        const auto reference=cpu.snapshot();
        for(auto& gpu:sessions) {
            compare(reference,gpu->snapshot());
            for(std::size_t r=0;r<cpu.receivers().size();++r){compare(cpu.receivers()[r].samples,gpu->receivers()[r].samples);compare(cpu.receivers()[r].samplesVx,gpu->receivers()[r].samplesVx);compare(cpu.receivers()[r].samplesVy,gpu->receivers()[r].samplesVy);}
            const auto revision=gpu->revision();const auto backend=gpu->backendInfo();
            check(!gpu->initialize(p,error,{1,1000000}),"Missing CUDA device was accepted");
            check(gpu->revision()==revision&&gpu->currentStep()==8&&gpu->backendInfo().cudaDevice==backend.cudaDevice,"Failed device switch discarded active session");
            check(queryBackendMemory3D(BackendKind3D::Cuda,1000000).deviceAvailableBytes==0,"Invalid GPU memory query succeeded");
            compare(reference,gpu->snapshot());
            check(gpu->resetFields(error)&&gpu->currentStep()==0&&gpu->snapshot().maxAbsVz==0,error.c_str());
            check(gpu->advance(1,error),error.c_str());
        }
        const auto usableCount=sessions.size();
        // Destruction of one GPU's buffers must not redirect another session.
        while(sessions.size()>1){sessions.pop_back();check(sessions.front()->advance(1,error),error.c_str());}
        std::cout<<"PASS: "<<inventory.devices.size()<<" detected CUDA device(s); "<<usableCount<<" usable device(s) exercised. CPU/CUDA fields, receivers, failed selection, reset and device-scoped readback\n";
        if(usableCount<2)std::cout<<"Hardware limitation: simultaneous cross-GPU execution requires at least two usable GPUs.\n";
    } catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
