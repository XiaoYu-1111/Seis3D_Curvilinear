#pragma once
#include <cstdint>
#include <type_traits>

namespace Seis3D::Curvilinear {
inline constexpr int ghostWidth3D=3; // MacCormack-DRP reaches three nodes on its biased side.
// Stable scalar-only descriptors shared with a future CUDA translation unit; no ImGui, STL storage or owning pointers.
struct GridLayout3D { std::int32_t nx{},ny{},nz{},ghost{}; std::uint64_t strideY{},strideZ{},nodeCount{}; };
enum class Field3D : int { Vx, Vy, Vz, Sxx, Syy, Szz, Sxy, Sxz, Syz };
static_assert(std::is_trivially_copyable_v<GridLayout3D> && std::is_standard_layout_v<GridLayout3D>);
static_assert(sizeof(GridLayout3D)==40);
}
