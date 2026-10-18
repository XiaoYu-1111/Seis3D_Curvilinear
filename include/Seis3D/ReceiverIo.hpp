#pragma once
#include "Seis3D/CurvilinearSolver.hpp"
#include <filesystem>

namespace Seis3D::Curvilinear {
// Selected Vx/Vy/Vz in physical units, per receiver and then component order.
[[nodiscard]] bool writeReceiversCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,
    const std::array<bool,3>& components,std::string& error);
// Exports time-zero-inclusive Vz traces with physical receiver coordinates and the exact simulation interval.
[[nodiscard]] bool writeReceiversCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,std::string& error,bool allVelocityComponents=false);
}
