#pragma once
#include <cstdint>
#include <string>
#include <vector>
// Legacy 3D API names are retained for source compatibility; both dimensions use this module.
namespace Seis3D::Curvilinear {
enum class BackendKind3D {
    Cpu,
    Cuda
};
struct ComputeSettings3D {
    int cpuThreads{};
    int cudaDevice{-1};
    bool operator==(const ComputeSettings3D &) const = default;
}; // CUDA -1 = largest usable VRAM; CPU 0 = auto, 1 = serial.
struct BackendInfo3D {
    BackendKind3D kind{BackendKind3D::Cpu};
    bool openMpAvailable{};
    int workerThreads{1};
    std::string deviceName;
    int cudaDevice{-1};
};
struct BackendAvailability3D {
    bool available{};
    std::string description;
    int cudaDevice{-1};
};
struct CudaDeviceInfo3D {
    int index{-1}, computeMajor{}, computeMinor{};
    std::string name, pciBusId, description;
    std::uint64_t totalMemoryBytes{}, freeMemoryBytes{};
    bool available{};
};
struct CudaDeviceInventory3D {
    std::vector<CudaDeviceInfo3D> devices;
    std::string description;
};
[[nodiscard]] CudaDeviceInventory3D queryCudaDevices3D();
[[nodiscard]] BackendAvailability3D selectCudaDevice3D(const CudaDeviceInventory3D &, int cudaDevice = -1);
[[nodiscard]] std::vector<std::string> queryGraphicsAdapters3D();
struct BackendMemory3D {
    std::uint64_t hostAvailableBytes{}, deviceAvailableBytes{};
}; // Zero means unavailable.
[[nodiscard]] BackendAvailability3D queryBackend3D(BackendKind3D, int cudaDevice = -1);
[[nodiscard]] BackendMemory3D queryBackendMemory3D(BackendKind3D, int cudaDevice = -1);
[[nodiscard]] BackendKind3D defaultBackend3D(); // Application default: usable CUDA first, otherwise CPU.
std::uint64_t queryCudaMemory3D(int device);
} // namespace Seis3D::Curvilinear
