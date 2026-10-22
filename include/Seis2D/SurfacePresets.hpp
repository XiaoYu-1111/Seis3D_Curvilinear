#pragma once
#include <array>
namespace Seis2D::Curvilinear {
struct SurfacePreset {
    const char *file, *en, *zh;
};
inline constexpr std::array<SurfacePreset, 8> surfacePresets{{
    {"model_library_1km/01_valley_then_hill.csv", "Valley then hill", "沟谷—山丘"},
    {"model_library_1km/02_double_hill.csv", "Double hill", "双山丘"},
    {"model_library_1km/03_double_valley.csv", "Double valley", "双沟谷"},
    {"model_library_1km/04_narrow_hill_broad_valley.csv", "Narrow hill / broad valley", "窄山—宽谷"},
    {"model_library_1km/05_asymmetric_hill_valley.csv", "Asymmetric slopes", "非对称坡面"},
    {"model_library_1km/06_flat_top_flat_bottom.csv", "Flat top / flat bottom", "平顶山—平底沟"},
    {"model_library_1km/07_hill_valley_hill.csv", "Hill / valley / hill", "山—谷—山"},
    {"model_library_1km/08_windowed_wave_train.csv", "Windowed wave train", "平滑收尾连续起伏"},
}};
} // namespace Seis2D::Curvilinear
