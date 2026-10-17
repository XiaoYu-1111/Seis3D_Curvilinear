#include "Seis3D/ReceiverGeometry3D.hpp"
#include "Seis3D/ReceiverIo.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    SimulationParameters p;p.nx=p.ny=300;p.nz=8;p.topographyAmplitude=0;
    std::vector<std::array<int,3>> nodes;std::string error;
    require(receiverNodes3D(p,nodes,error)&&nodes.size()==296,"Auto X must retain its two-node margins");
    p.receiverArray={ReceiverArrayKind3D::XLine,0,-1,1,300,0,1,0};
    require(receiverNodes3D(p,nodes,error)&&nodes.size()==300&&nodes.back()[0]==299,"Full custom line or inactive stride was handled incorrectly");
    p.receiverArray.startX=-1;require(receiverLimits3D(p).x==150&&!receiverNodes3D(p,nodes,error),"Centered start was mistaken for centered entire array");
    p.receiverArray.startX=2;p.receiverArray.countX=298;require(receiverNodes3D(p,nodes,error)&&nodes.back()[0]==299,"Inclusive endpoint is off by one");
    for(int n:{8,20,50,100,200,300})for(int start:{0,2,n/2,n-1})for(int stride:{1,2,3,50,std::numeric_limits<int>::max()}) {
        p.nx=n;p.receiverArray.startX=start;p.receiverArray.strideX=stride;
        int expected=0;for(std::int64_t at=start;at<n;at+=stride)++expected;
        const auto limits=receiverLimits3D(p);require(limits.x==expected,"Capacity differs from independently enumerated positions");
        p.receiverArray.countX=expected;require(receiverNodes3D(p,nodes,error)&&static_cast<int>(nodes.size())==expected,error);
        ++p.receiverArray.countX;require(!receiverNodes3D(p,nodes,error),"One position beyond capacity was accepted");
        require(fitReceiverCounts3D(p)&&p.receiverArray.countX==expected&&receiverNodes3D(p,nodes,error),"Explicit count fit failed");
    }
    p.nx=p.ny=64;p.receiverArray={ReceiverArrayKind3D::GridXY,0,0,1,32,32,1,1};
    require(receiverNodes3D(p,nodes,error)&&nodes.size()==1024&&nodes[32]==std::array<int,3>{0,1,1}&&nodes.back()==std::array<int,3>{31,31,1},"1024-grid order changed");
    auto invalid=p;invalid.receiverArray.countX=41;invalid.receiverArray.countY=25;require(!receiverNodes3D(invalid,nodes,error),"1025 receivers passed the total cap");
    invalid=p;invalid.receiverArray.countX=invalid.receiverArray.countY=std::numeric_limits<int>::max();require(!receiverNodes3D(invalid,nodes,error),"Huge counts overflowed validation");
    invalid=p;invalid.nx=4;invalid.receiverArray.kind=ReceiverArrayKind3D::AutoXLine;require(!receiverNodes3D(invalid,nodes,error),"Auto mode bypassed grid validation");
    p.maxSteps=8;p.duration=.05;CpuSimulation3D cpu;
    require(cpu.initialize(p,error,{1})&&cpu.advance(8,error),error);double peak=0,maxDifference=0;
    if(queryBackend3D(BackendKind3D::Cuda).available) {
        CpuSimulation3D gpu(BackendKind3D::Cuda);require(gpu.initialize(p,error)&&gpu.advance(8,error),error);const auto snapshot=gpu.snapshot();
        for(std::size_t r=0;r<cpu.receivers().size();++r) {
            const auto& a=cpu.receivers()[r];const auto& b=gpu.receivers()[r];require(b.samples.size()==9&&b.samplesVx.size()==9&&b.samplesVy.size()==9,"1024-channel time axis incomplete");
            const std::vector<double>* ca[]{&a.samplesVx,&a.samplesVy,&a.samples};const std::vector<double>* cb[]{&b.samplesVx,&b.samplesVy,&b.samples};
            for(int c=0;c<3;++c)for(int t=0;t<9;++t){peak=std::max(peak,std::abs((*ca[c])[t]));maxDifference=std::max(maxDifference,std::abs((*ca[c])[t]-(*cb[c])[t]));}
            require(b.samples.back()==snapshot.vz[b.gridIndex]&&b.samplesVx.back()==snapshot.vx[b.gridIndex]&&b.samplesVy.back()==snapshot.vy[b.gridIndex],"CUDA gather mismatches its source grid node");
        }
        require(peak>0&&maxDifference/peak<1e-8,"1024-channel CPU/CUDA mismatch");require(std::abs(gpu.receivers().back().samples.back())>0,"Last CUDA block was not exercised by a nonzero sample");
    }
    require(writeReceiversCsv(cpu,std::filesystem::path(SEIS3D_OUTPUT_ROOT)/"validation"/"receivers_1024_3c.csv",error,true),error);
    std::cout<<"PASS inclusive counts, 120 capacity/stride cases, centered start, 1024 cap, overflow rejection, CPU/CUDA 1024-channel 3C gather; relative difference "<<maxDifference/std::max(peak,1e-30)<<'\n';
}catch(const std::exception& ex){std::cerr<<"FAIL "<<ex.what()<<'\n';return 1;}}
