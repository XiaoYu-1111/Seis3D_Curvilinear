#pragma once
#include "CurvilinearSolver2D.hpp"
namespace Seis2D::Curvilinear {
struct MaterialPreset {
    const char *en;
    const char *zh;
    std::vector<Layer> layers;
};
inline const std::vector<MaterialPreset> &materialPresets() {
    static const std::vector<MaterialPreset> values{
        {"Reference elastic half-space", "参考均匀弹性半空间", {{0, 3000, 1700, 2200}}},
        {"Alluvium / sandstone / basement",
         "冲积层—砂岩—基岩",
         {{20, 1500, 700, 2050}, {50, 2500, 1300, 2250}, {0, 4000, 2300, 2650}}},
        {"Loess five-layer profile",
         "黄土五层剖面",
         {{5, 550, 250, 1550},
          {5, 800, 350, 1800},
          {20, 1500, 700, 2050},
          {20, 2500, 1300, 2250},
          {0, 4000, 2300, 2650}}},
        {"Weathered rock",
         "风化岩层",
         {{10, 1200, 500, 1900}, {30, 2200, 1100, 2200}, {0, 4500, 2600, 2700}}},
        {"Sedimentary basin",
         "沉积盆地",
         {{50, 1800, 800, 2100}, {100, 2600, 1400, 2300}, {200, 3500, 2000, 2500}, {0, 5000, 2900, 2750}}}};
    return values;
}
} // namespace Seis2D::Curvilinear
