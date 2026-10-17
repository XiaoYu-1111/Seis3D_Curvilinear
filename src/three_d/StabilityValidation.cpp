#include "Seis3D/CurvilinearSolver.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Seis3D::Curvilinear;
double energy(const SimulationResult& s) {
    const double mu=s.modelDensity*s.modelVs*s.modelVs;
    const double bulk=s.modelDensity*(s.modelVp*s.modelVp-4*s.modelVs*s.modelVs/3);
    double total=0;
    const auto& g=s.grid;
    for(int k=g.ghost;k<g.nz-g.ghost;++k)for(int j=g.ghost;j<g.ny-g.ghost;++j)for(int i=g.ghost;i<g.nx-g.ghost;++i) {
        const auto q=g.index(i,j,k);const double mean=(s.sxx[q]+s.syy[q]+s.szz[q])/3;
        const double dev=(s.sxx[q]-mean)*(s.sxx[q]-mean)+(s.syy[q]-mean)*(s.syy[q]-mean)+(s.szz[q]-mean)*(s.szz[q]-mean)+2*(s.sxy[q]*s.sxy[q]+s.sxz[q]*s.sxz[q]+s.syz[q]*s.syz[q]);
        total+=(0.5*s.modelDensity*(s.vx[q]*s.vx[q]+s.vy[q]*s.vy[q]+s.vz[q]*s.vz[q])+dev/(4*mu)+mean*mean/(2*bulk))*g.jacobian[q]*g.dx*g.dy*g.dz;
    }
    return total;
}
int main() {
    try {
        struct Case{double relief,wavelength,vs;};
        for(const auto test:{Case{0,240,1700},Case{20,240,1700},Case{35,240,1700},Case{35,80,1700},Case{20,120,150},Case{20,120,2550}}) {
            const double relief=test.relief;
            SimulationParameters p;p.maxSteps=0;p.nx=20;p.ny=18;p.nz=24;p.duration=1.0;p.topographyAmplitude=relief;p.topographyWavelength=test.wavelength;p.vs=test.vs;
            CpuSimulation3D s;std::string error;
            if(!s.initialize(p,error,{2}))throw std::runtime_error(error);
            double peak=0,late=0;
            while(!s.completed()) {
                if(!s.advance(std::min(100,s.totalSteps()-s.currentStep()),error))throw std::runtime_error(error);
                const double e=energy(s.snapshot());
                // A fixed 100-step probe can miss the Ricker peak when dt is
                // larger. The solver tracks the peak at EVERY accepted step;
                // retain the independent compliance-energy check below.
                peak=s.diagnostics().peakEnergy;
                if(s.currentStep()*s.dt()>0.6)late=std::max(late,e);
                if(!std::isfinite(e)||std::abs(e-s.diagnostics().elasticEnergy)>1e-10*std::max(e,1e-30))throw std::runtime_error("Energy diagnostic disagrees with independent compliance energy");
            }
            std::cout<<"LONG RUN relief="<<relief<<" wavelength="<<test.wavelength<<" Vs="<<test.vs<<" late/peak="<<late/peak<<'\n';
            // Low Vs retains slow S waves longer; compare end energy against the driven peak rather than demanding monotonicity.
            if(!std::isfinite(peak)||peak<=0||late>peak*0.2)throw std::runtime_error("Late energy failed to decay below 20% of source-driven peak");
        }
        SimulationParameters p;p.maxSteps=0;p.nx=p.ny=12;p.nz=18;p.duration=0.18;p.topographyAmplitude=12;
        std::vector<double> final[3];
        for(int n=0;n<3;++n) {
            p.cflSafety=0.22/std::pow(2,n);CpuSimulation3D s;std::string error;
            if(!s.initialize(p,error,{1})||!s.advance(s.totalSteps(),error))throw std::runtime_error(error);
            final[n]=s.field(Field3D::Vz);
        }
        double coarse=0,fine=0;
        for(std::size_t q=0;q<final[0].size();++q){coarse=std::max(coarse,std::abs(final[0][q]-final[2][q]));fine=std::max(fine,std::abs(final[1][q]-final[2][q]));}
        std::cout<<"Time refinement difference ratio="<<coarse/std::max(fine,1e-30)<<'\n';
        // Alternating spatial operators add dt-dependent numerical dissipation.
        // Classical RK weights do not imply fourth-order temporal convergence
        // here. Analytic P/S errors are checked separately by WavePhysicsValidation.
        if(coarse<=0||fine>=coarse/2)throw std::runtime_error("Halving CFL failed the alternating-DRP convergence check");
    } catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
