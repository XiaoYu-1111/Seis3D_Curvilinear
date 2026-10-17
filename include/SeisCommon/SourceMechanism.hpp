#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
namespace SeisWave {
enum class SourceKind {
    DirectionalForce,
    IsotropicMoment
};
struct SourceMechanism {
    SourceKind kind{SourceKind::DirectionalForce};
    std::array<double, 3> direction{0, 0, 1}; // Physical Cartesian X/Y/Z; normalized on initialization.
    bool operator==(const SourceMechanism &) const = default;
};
inline bool validateSource(const SourceMechanism &source, bool planar, std::string &error) {
    if (source.kind != SourceKind::DirectionalForce && source.kind != SourceKind::IsotropicMoment) {
        error = "Unknown source mechanism.";
        return false;
    }
    if (source.kind == SourceKind::DirectionalForce) {
        const auto &d = source.direction;
        const double norm = std::hypot(d[0], d[1], d[2]);
        if (!std::isfinite(norm) || norm <= 0 || (planar && d[1] != 0)) {
            error = planar ? "2D force direction must be finite, nonzero and inside the X-Z plane."
                           : "Force direction must be finite and nonzero.";
            return false;
        }
    }
    return true;
}
inline std::array<double, 3> unitSourceDirection(const SourceMechanism &source) {
    const auto &d = source.direction;
    if (source.kind == SourceKind::IsotropicMoment)
        return {0, 0, 0};
    const double norm = std::hypot(d[0], d[1], d[2]);
    return {d[0] / norm, d[1] / norm, d[2] / norm};
}
// Equivalent force for M_ij(t)=M(t)*delta_ij: -M(t)*div(delta_h I).
// Conservative mapped fluxes ensure zero net force for complete interior support.
// This centered FD4 operator constructs a fixed source pattern only; propagation
// still uses the alternating MacCormack-DRP operator at every RK stage.
template <class Flux>
std::array<double, 3> isotropicMomentPattern(std::size_t q, int dimensions,
                                             const std::array<std::ptrdiff_t, 3> &strides, double h,
                                             double jacobian, Flux flux) {
    std::array<double, 3> result{};
    for (int c = 0; c < dimensions; ++c)
        for (int a = 0; a < dimensions; ++a) {
            const auto s = strides[a], p = static_cast<std::ptrdiff_t>(q);
            result[c] -= (flux(p - 2 * s, a, c) - 8 * flux(p - s, a, c) + 8 * flux(p + s, a, c) -
                          flux(p + 2 * s, a, c)) /
                         (12 * h * jacobian);
        }
    return result;
}
} // namespace SeisWave
