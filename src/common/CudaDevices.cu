#include "CudaSupport.cuh"
#include "SeisCommon/ComputeDevices.hpp"
namespace Seis3D::Curvilinear {
namespace {
using SeisWave::Cuda::checked;
using SeisWave::Cuda::DeviceScope;
__global__ void probeKernel() {
}
} // namespace
CudaDeviceInventory3D queryCudaDevices3D() {
    CudaDeviceInventory3D inventory;
    int count = 0;
    const auto status = cudaGetDeviceCount(&count);
    if (status != cudaSuccess) {
        inventory.description = std::string("CUDA unavailable: ") + cudaGetErrorString(status);
        cudaGetLastError();
        return inventory;
    }
    if (count == 0) {
        inventory.description = "No CUDA device detected.";
        return inventory;
    }
    for (int index = 0; index < count; ++index) {
        CudaDeviceInfo3D device;
        device.index = index;
        device.name = "CUDA GPU " + std::to_string(index);
        try {
            cudaDeviceProp properties{};
            checked(cudaGetDeviceProperties(&properties, index), "device properties");
            device.name = properties.name;
            device.computeMajor = properties.major;
            device.computeMinor = properties.minor;
            device.totalMemoryBytes = properties.totalGlobalMem;
            char bus[32]{};
            if (cudaDeviceGetPCIBusId(bus, sizeof(bus), index) == cudaSuccess)
                device.pciBusId = bus;
            else
                cudaGetLastError();
            if (properties.major < 6)
                device.description = "CUDA backend requires compute capability >= 6.0.";
            else {
                DeviceScope scope(index);
                cudaFuncAttributes attributes{};
                checked(cudaFuncGetAttributes(&attributes, probeKernel),
                        "kernel unavailable for this device");
                std::size_t free = 0, total = 0;
                checked(cudaMemGetInfo(&free, &total), "memory query");
                device.freeMemoryBytes = free;
                device.totalMemoryBytes = total;
                device.available = true;
                device.description = "Available";
            }
        } catch (const std::exception &ex) {
            device.description = ex.what();
            cudaGetLastError();
        }
        inventory.devices.push_back(std::move(device));
    }
    inventory.description = std::to_string(count) + " CUDA device(s) detected.";
    return inventory;
}
std::uint64_t queryCudaMemory3D(int device) {
    try {
        DeviceScope scope(device);
        std::size_t freeBytes = 0, total = 0;
        checked(cudaMemGetInfo(&freeBytes, &total), "memory query");
        return freeBytes;
    } catch (...) {
        cudaGetLastError();
        return 0;
    }
}

} // namespace Seis3D::Curvilinear
