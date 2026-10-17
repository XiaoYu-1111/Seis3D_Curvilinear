#pragma once
#include "CurvilinearSolver.hpp"
#include <algorithm>
namespace Seis3D::Curvilinear {
inline std::array<int,3> absorbingWidths3D(const SimulationParameters& p) {
    if(!p.absorbingBoundary.enabled)return {};
    const int nodes[3]{p.nx,p.ny,p.nz};std::array<int,3> widths{};
    for(int a=0;a<3;++a)widths[a]=std::min(p.absorbingBoundary.cells[a],nodes[a]/3);
    return widths;
}
inline std::array<double,3> absorbingFractions3D(const SimulationParameters& p,int i,int j,int k) {
    const auto widths=absorbingWidths3D(p);const int distances[3]{std::min(i,p.nx-1-i),std::min(j,p.ny-1-j),p.nz-1-k};
    std::array<double,3> fractions{};
    for(int a=0;a<3;++a)if(widths[a]>0)fractions[a]=std::clamp(1.0-static_cast<double>(distances[a])/widths[a],0.0,1.0);
    return fractions;
}
inline double absorbingRate3D(const SimulationParameters& p,int i,int j,int k,double vp) {
    const auto widths=absorbingWidths3D(p);const auto f=absorbingFractions3D(p,i,j,k);double rate=0;
    for(int a=0;a<3;++a)if(widths[a]>0)rate+=p.absorbingBoundary.strength*vp/(widths[a]*p.spacing)*f[a]*f[a];
    return rate;
}
}
