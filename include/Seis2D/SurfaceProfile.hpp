#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <vector>
namespace Seis2D::Curvilinear {
using SurfaceSamples = std::vector<std::array<double, 2>>; // x (m), elevation (m, positive up).
// Transactional reader: an error leaves the destination unchanged.
bool readSurfaceProfile(const std::filesystem::path &, SurfaceSamples &, std::string &);
// Stretch X to [0, targetSpan], preserve elevation, and PCHIP-sample uniformly.
// Always pass the original profile to avoid cumulative resampling loss.
SurfaceSamples resampleSurfaceProfile(const SurfaceSamples &, double targetSpan, std::size_t sampleCount);
class SurfaceProfile {
  public:
    explicit SurfaceProfile(const SurfaceSamples &);
    std::array<double, 2> sample(double x) const; // elevation, d(elevation)/dx; C1 PCHIP.
  private:
    SurfaceSamples points_;
    std::vector<double> slopes_;
};
} // namespace Seis2D::Curvilinear
