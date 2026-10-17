#pragma once
#include "CurvilinearSolver.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace Seis3D::Curvilinear {
inline std::array<double,3> nominalDimensions3D(const SimulationParameters& p) {
    return {(static_cast<double>(p.nx)-1)*p.spacing,(static_cast<double>(p.ny)-1)*p.spacing,(static_cast<double>(p.nz)-1)*p.spacing};
}
struct ModelBounds3D {
    std::array<double,3> minimum{},maximum{};
    double surfaceMinimum{},surfaceMaximum{};
    std::array<double,3> dimensions() const{return {maximum[0]-minimum[0],maximum[1]-minimum[1],maximum[2]-minimum[2]};}
};
// Evaluate once after a successful rebuild. Ghost coordinates are excluded.
inline ModelBounds3D physicalModelBounds3D(const Grid3D& grid) {
    ModelBounds3D b;
    b.minimum.fill(std::numeric_limits<double>::max());b.maximum.fill(std::numeric_limits<double>::lowest());
    b.surfaceMinimum=std::numeric_limits<double>::max();b.surfaceMaximum=std::numeric_limits<double>::lowest();
    const int g=grid.ghost;
    for(int k=g;k<grid.nz-g;++k)for(int j=g;j<grid.ny-g;++j)for(int i=g;i<grid.nx-g;++i) {
        const auto& xyz=grid.coordinates[grid.index(i,j,k)];
        for(int a=0;a<3;++a){b.minimum[a]=std::min(b.minimum[a],xyz[a]);b.maximum[a]=std::max(b.maximum[a],xyz[a]);}
        if(k==g){b.surfaceMinimum=std::min(b.surfaceMinimum,xyz[2]);b.surfaceMaximum=std::max(b.surfaceMaximum,xyz[2]);}
    }
    return b;
}
}
