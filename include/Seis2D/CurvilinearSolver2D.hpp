#pragma once
#include "SeisCommon/ComputeDevices.hpp"
#include "SurfaceProfile.hpp"
#include "SeisCommon/SourceMechanism.hpp"
#include <array>
#include <filesystem>
#include <memory>
#include <vector>
namespace Seis2D::Curvilinear {
using Seis3D::Curvilinear::BackendInfo3D;
using Seis3D::Curvilinear::BackendKind3D;
using Seis3D::Curvilinear::ComputeSettings3D;
inline constexpr int ghost = 3;
inline constexpr int maxAxisNodes = 16384;
struct Layer {
    double thickness{}, vp{}, vs{}, density{};
    bool operator==(const Layer &) const = default;
};
struct Parameters {
    int nx{200}, nz{200};
    double spacing{2.5}, vp{3000}, vs{1700}, density{2200};
    double relief{10}, wavelength{400}, flattenDepth{}, frequency{30}, duration{1}, cfl{.8},
        forcePerLength{2e5};
    int sourceX{-1}, sourceZ{40}, maxSteps{5000};
    SeisWave::SourceMechanism source;
    double momentPerLength{2e6}; // Isotropic moment per unit Y length, N (= N*m/m).
    bool absorbing{true};
    int spongeX{16}, spongeZ{16};
    double spongeStrength{3};
    int receiverStart{2}, receiverCount{96}, receiverStride{2}, receiverDepth{1};
    bool layered{};
    bool importedSurface{};
    double surfaceDatum{}; // z = datum - elevation; depth is positive downward.
    SurfaceSamples surfaceSamples;
    std::vector<Layer> layers{{20, 1500, 700, 2050}, {50, 2500, 1300, 2250}, {0, 4000, 2300, 2650}};
    bool operator==(const Parameters &) const = default;
};
struct Grid {
    int nx{}, nz{};
    double h{}, flattenDepth{}, maxMetric{}, minJacobian{}, maxEdge{};
    std::vector<std::array<double, 2>> coordinates, normals;
    std::vector<double> metricA, metricB, jacobian, vp, vs, density;
    std::size_t index(int i, int k) const {
        return i + static_cast<std::size_t>(nx) * k;
    }
};
struct Receiver {
    int x{}, z{};
    std::array<double, 2> position{};
    std::vector<double> vx, vz;
};
struct Diagnostics {
    double energy{}, peakEnergy{}, maxVelocity{}, tractionRelative{}, ppw{}, waveDt{}, dampingDt{},
        sourceDt{}, initializationMs{}, totalStepMs{}, lastStepMs{};
    bool finite{true};
};
struct Snapshot {
    std::array<std::vector<double>, 5> fields;
}; // Vx,Vz,Sxx,Szz,Sxz; padded storage.
class Simulation {
  public:
    Simulation();
    ~Simulation();
    Simulation(const Simulation &) = delete;
    bool initialize(const Parameters &, std::string &, BackendKind3D = BackendKind3D::Cpu,
                    const ComputeSettings3D & = {});
    bool advance(int, std::string &);
    bool reset(std::string &);
    bool initialized() const;
    bool completed() const;
    int step() const;
    int totalSteps() const;
    double dt() const;
    const Parameters &parameters() const;
    const Grid &grid() const;
    const Diagnostics &diagnostics() const;
    const std::vector<Receiver> &receivers() const;
    BackendInfo3D backend() const;
    bool readFrame(int component, std::vector<float> &, std::string &) const;
    Snapshot snapshot() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
bool writeSnapshot(const Simulation &, const std::filesystem::path &, std::string &);
bool writeReceivers(const Simulation &, const std::filesystem::path &, std::string &);
} // namespace Seis2D::Curvilinear
