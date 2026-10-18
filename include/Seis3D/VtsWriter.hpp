#pragma once

#include "Seis3D/CurvilinearSolver.hpp"

#include <filesystem>
#include <array>
#include <string>

namespace Seis3D::Curvilinear {

struct VtsExportOptions3D {
    // Vx, Vy, Vz, |V|, pressure, Sxx, Syy, Szz, Sxy, Sxz, Syz.
    std::array<bool,11> fields{true,true,true,true,true,true,true,true,true,true,true};
    bool includeMaterial{true};
    bool includeVelocityVector{true};
};

[[nodiscard]] bool writeVts(const SimulationResult& result, const std::filesystem::path& path,
                            const VtsExportOptions3D& options, std::string& error);

// Writes explicit curvilinear point coordinates and the current field snapshot as VTK XML StructuredGrid.
[[nodiscard]] bool writeVts(const SimulationResult& result, const std::filesystem::path& path,
                            std::string& error);

} // namespace Seis3D::Curvilinear
