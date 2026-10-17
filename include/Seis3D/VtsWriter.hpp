#pragma once

#include "Seis3D/CurvilinearSolver.hpp"

#include <filesystem>
#include <string>

namespace Seis3D::Curvilinear {

// Writes explicit curvilinear point coordinates and the current field snapshot as VTK XML StructuredGrid.
[[nodiscard]] bool writeVts(const SimulationResult& result, const std::filesystem::path& path,
                            std::string& error);

} // namespace Seis3D::Curvilinear
