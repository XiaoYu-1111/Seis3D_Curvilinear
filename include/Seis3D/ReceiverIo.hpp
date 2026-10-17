#pragma once
#include "Seis3D/CurvilinearSolver.hpp"
#include <filesystem>

namespace Seis3D::Curvilinear {
// Exports time-zero-inclusive Vz traces with physical receiver coordinates and the exact simulation interval.
[[nodiscard]] bool writeReceiversCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,std::string& error,bool allVelocityComponents=false);
}
