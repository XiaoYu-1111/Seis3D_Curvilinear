#include "Seis3D/CurvilinearSolver.hpp"
#include "Seis3D/VtsWriter.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace Seis3D::Curvilinear;
#ifdef SEIS3D_OUTPUT_ROOT
    const std::filesystem::path output = argc > 1 ? argv[1] : std::filesystem::path(SEIS3D_OUTPUT_ROOT) / "curvilinear_demo.vts";
#else
    const std::filesystem::path output = argc > 1 ? argv[1] : "out/curvilinear_demo.vts";
#endif
    SimulationParameters parameters;
    SimulationResult result;
    std::string error;
    if (!runDemo(parameters, result, error)) {
        std::cerr << "Seis3D curvilinear demo failed: " << error << '\n';
        return 1;
    }
    std::error_code ec;
    if (!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path(), ec);
    if (ec) {
        std::cerr << "Could not create output directory: " << ec.message() << '\n';
        return 2;
    }
    if (!writeVts(result, output, error)) {
        std::cerr << "VTS export failed: " << error << '\n';
        return 3;
    }
    std::cout << std::fixed << std::setprecision(6)
              << "Seis3D Curvilinear completed | backend: " << (result.computeBackend.kind==BackendKind3D::Cuda?"CUDA Float64":"CPU Float64") << '\n'
              << "Device: " << result.computeBackend.deviceName << " | workers: " << result.computeBackend.workerThreads << '\n'
              << "Grid (physical): " << parameters.nx << " x " << parameters.ny << " x " << parameters.nz << '\n'
              << "RK4 steps: " << result.steps << " | dt: " << result.dt * 1000.0 << " ms\n"
              << "Peak |Vz|: " << result.maxAbsVz << " m/s\n"
              << "Minimum Jacobian: " << result.diagnostics.minJacobian << '\n'
              << "Metric norm for CFL: " << result.diagnostics.maxMetricNorm << '\n'
              << std::scientific << "Unprojected relative traction-rate residual: " << result.diagnostics.maxTractionRateRelative << '\n' << std::fixed
              << "Free-surface traction residual: " << result.maxFreeSurfaceTraction << " Pa\n"
              << "All fields finite: " << (result.allFinite ? "yes" : "no") << '\n'
              << "VTS: " << std::filesystem::absolute(output).string() << '\n';
    return 0;
}
