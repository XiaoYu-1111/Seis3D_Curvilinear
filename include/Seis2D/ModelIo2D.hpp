#pragma once
#include "CurvilinearSolver2D.hpp"

namespace Seis2D::Curvilinear {
struct ModelDocument {
    Parameters parameters;
    ComputeSettings3D compute;
    BackendKind3D backend{BackendKind3D::Cpu};
    double referenceDt{};
};
// Self-contained model (not a wavefield checkpoint); X-fastest, physical nodes only, Z positive down.
bool writeModelVts(const Simulation &, const std::filesystem::path &, const ComputeSettings3D &,
                   std::string &error);
// Reads this application's versioned ASCII VTS schema, validating shape, finite data and checksum.
// Arbitrary third-party VTS or legacy wavefield snapshots are intentionally rejected.
bool readModelVts(const std::filesystem::path &, ModelDocument &, std::string &error);
} // namespace Seis2D::Curvilinear
