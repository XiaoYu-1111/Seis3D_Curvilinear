#include "SeisCommon/ComputeDevices.hpp"
#ifdef _WIN32
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>
#elif defined(__linux__)
#include <unistd.h>
#endif
#include <algorithm>
namespace Seis3D::Curvilinear {
#ifndef SEIS3D_HAS_CUDA
CudaDeviceInventory3D queryCudaDevices3D() {
    return {{}, "CUDA is not compiled in this build. Reconfigure with SEIS3D_ENABLE_CUDA=ON."};
}
#endif
BackendAvailability3D selectCudaDevice3D(const CudaDeviceInventory3D &inventory, int requested) {
    if (requested < -1)
        return {false, "Invalid CUDA device index."};
    const CudaDeviceInfo3D *chosen = nullptr;
    for (const auto &device : inventory.devices) {
        if (requested >= 0 && device.index == requested) {
            if (!device.available)
                return {false,
                        "GPU " + std::to_string(requested) + " | " + device.name + ": " + device.description};
            chosen = &device;
            break;
        }
        if (requested < 0 && device.available &&
            (!chosen || device.totalMemoryBytes > chosen->totalMemoryBytes))
            chosen = &device;
    }
    if (chosen)
        return {true, "GPU " + std::to_string(chosen->index) + " | " + chosen->name, chosen->index};
    if (requested >= 0)
        return {false, "CUDA device " + std::to_string(requested) + " is not present."};
    std::string why = inventory.description;
    for (const auto &device : inventory.devices)
        why += "\nGPU " + std::to_string(device.index) + " | " + device.name + ": " + device.description;
    return {false, why};
}
std::vector<std::string> queryGraphicsAdapters3D() {
    std::vector<std::string> names;
#ifdef _WIN32
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())))) {
        for (UINT index = 0;; ++index) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            const auto result = factory->EnumAdapters1(index, adapter.GetAddressOf());
            if (result == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(result))
                break;
            DXGI_ADAPTER_DESC1 description{};
            if (FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                continue;
            const int bytes =
                WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, nullptr, 0, nullptr, nullptr);
            if (bytes > 1) {
                std::string name(bytes, '\0');
                WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, name.data(), bytes, nullptr,
                                    nullptr);
                name.pop_back();
                names.push_back(std::move(name));
            }
        }
    }
#else
    for (const auto &device : queryCudaDevices3D().devices)
        names.push_back(device.name);
#endif
    return names;
}
BackendMemory3D queryBackendMemory3D(BackendKind3D kind, int cudaDevice) {
    BackendMemory3D memory;
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        memory.hostAvailableBytes = status.ullAvailPhys;
#elif defined(__linux__)
    const auto pages = sysconf(_SC_AVPHYS_PAGES), bytes = sysconf(_SC_PAGESIZE);
    if (pages > 0 && bytes > 0)
        memory.hostAvailableBytes = static_cast<std::uint64_t>(pages) * bytes;
#endif
#ifdef SEIS3D_HAS_CUDA
    if (kind == BackendKind3D::Cuda) {
        const auto selected = cudaDevice < 0 ? selectCudaDevice3D(queryCudaDevices3D(), cudaDevice)
                                             : BackendAvailability3D{true, "", cudaDevice};
        if (selected.available)
            memory.deviceAvailableBytes = queryCudaMemory3D(selected.cudaDevice);
    }
#else
    (void)kind;
    (void)cudaDevice;
#endif
    return memory;
}
BackendAvailability3D queryBackend3D(BackendKind3D kind, int cudaDevice) {
    if (kind == BackendKind3D::Cpu)
        return {true, "CPU Float64"};
    return selectCudaDevice3D(queryCudaDevices3D(), cudaDevice);
}

BackendKind3D defaultBackend3D() {
    return queryBackend3D(BackendKind3D::Cuda).available ? BackendKind3D::Cuda : BackendKind3D::Cpu;
}
} // namespace Seis3D::Curvilinear
