#pragma once
#include "CurvilinearSolver.hpp"
#include <filesystem>
namespace Seis3D::Curvilinear {
struct ModelDocument3D {
    SimulationParameters parameters;
    ComputeSettings3D compute;
    BackendKind3D backend{BackendKind3D::Cpu};
    double referenceDt{};
};
// Geometry/materials and all run parameters. No full wavefield/GPU snapshot is needed.
bool writeModelVts3D(const ISimulationBackend3D &, const std::filesystem::path &, const ComputeSettings3D &,
                     std::string &error);
// Versioned self-contained model, not a checkpoint; initialize a fresh backend after reading.
bool readModelVts3D(const std::filesystem::path &, ModelDocument3D &, std::string &error);
} // namespace Seis3D::Curvilinear
