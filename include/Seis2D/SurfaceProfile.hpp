#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <vector>
namespace Seis2D::Curvilinear {
using SurfaceSamples = std::vector<std::array<double, 2>>; // x (m), elevation (m, positive up).
// Transactional reader: an error leaves the destination unchanged.
bool readSurfaceProfile(const std::filesystem::path &, SurfaceSamples &, std::string &);
class SurfaceProfile {
  public:
    explicit SurfaceProfile(const SurfaceSamples &);
    std::array<double, 2> sample(double x) const; // elevation, d(elevation)/dx; C1 PCHIP.
  private:
    SurfaceSamples points_;
    std::vector<double> slopes_;
};
} // namespace Seis2D::Curvilinear
