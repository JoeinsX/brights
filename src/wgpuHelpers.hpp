#pragma once

#include <webgpu/webgpu.h>
#include <iostream>
#include <vector>
#include <cassert>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace wgpu_utils {
    template<typename T>
    struct RequestData {
        T value = nullptr;
        bool requestEnded = false;
    };

    inline WGPUAdapter requestAdapterSync(WGPUInstance instance,
                                          WGPURequestAdapterOptions const*
                                          options) {
        RequestData<WGPUAdapter> userData;

        auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status,
                                        WGPUAdapter adapter,
                                        char const* message, void* pUserData) {
            auto& data = *reinterpret_cast<RequestData<WGPUAdapter>*>(
                pUserData);
            if(status == WGPURequestAdapterStatus_Success) {
                data.value = adapter;
            } else {
                std::cerr << "Could not get WebGPU adapter: " << (
                    message ? message : "Unknown error") << std::endl;
            }
            data.requestEnded = true;
        };

        wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded,
                                   (void*)&userData);
        assert(userData.requestEnded);
        return userData.value;
    }

    inline WGPUDevice requestDeviceSync(WGPUAdapter adapter,
                                        WGPUDeviceDescriptor const*
                                        descriptor) {
        RequestData<WGPUDevice> userData;

        auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                       WGPUDevice device, char const* message,
                                       void* pUserData) {
            auto& data = *reinterpret_cast<RequestData<WGPUDevice>*>(pUserData);
            if(status == WGPURequestDeviceStatus_Success) {
                data.value = device;
            } else {
                std::cerr << "Could not get WebGPU device: " << (
                    message ? message : "Unknown error") << std::endl;
            }
            data.requestEnded = true;
        };

        wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded,
                                 (void*)&userData);

#ifdef __EMSCRIPTEN__
        while(!userData.requestEnded) { emscripten_sleep(100); }
#endif

        assert(userData.requestEnded);
        return userData.value;
    }

    inline void inspectAdapter(WGPUAdapter adapter) {
        std::vector<WGPUFeatureName> features;
        size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
        features.resize(featureCount);
        wgpuAdapterEnumerateFeatures(adapter, features.data());

        std::cout << "Adapter features:" << std::endl;
        std::cout << std::hex;
        for(auto f : features) std::cout << " - 0x" << f << std::endl;
        std::cout << std::dec;

        WGPUAdapterProperties properties = {};

        properties.nextInChain = nullptr;
        wgpuAdapterGetProperties(adapter, &properties);

        std::cout << "Adapter properties:\n";
        std::cout << " - vendorID: " << properties.vendorID << "\n";
        if(properties.vendorName)
            std::cout << " - vendorName: " << properties.vendorName << "\n";
        if(properties.architecture)
            std::cout << " - architecture: " << properties.architecture << "\n";
        std::cout << " - deviceID: " << properties.deviceID << "\n";
        if(properties.name) std::cout << " - name: " << properties.name << "\n";
        if(properties.driverDescription)
            std::cout << " - driverDescription: " << properties.
                driverDescription << "\n";
        std::cout << std::hex << " - adapterType: 0x" << properties.adapterType
            << "\n";
        std::cout << " - backendType: 0x" << properties.backendType << std::dec
            << std::endl;
    }

    inline void inspectDevice(WGPUDevice device) {
        std::vector<WGPUFeatureName> features;
        size_t featureCount = wgpuDeviceEnumerateFeatures(device, nullptr);
        features.resize(featureCount);
        wgpuDeviceEnumerateFeatures(device, features.data());

        std::cout << "Device features:" << std::endl;
        std::cout << std::hex;
        for(auto f : features) std::cout << " - 0x" << f << std::endl;
        std::cout << std::dec;

        WGPUSupportedLimits limits = {};
        limits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
        bool success = wgpuDeviceGetLimits(device, &limits) ==
                       WGPUStatus_Success;
#else
        bool success = wgpuDeviceGetLimits(device, &limits);
#endif

        if(success) {
            std::cout << "Device limits:\n";
            std::cout << " - maxTextureDimension1D: " << limits.limits.
                maxTextureDimension1D << "\n";
            std::cout << " - maxTextureDimension2D: " << limits.limits.
                maxTextureDimension2D << "\n";
            std::cout << " - maxTextureDimension3D: " << limits.limits.
                maxTextureDimension3D << "\n";
            std::cout << " - maxTextureArrayLayers: " << limits.limits.
                maxTextureArrayLayers << std::endl;
        }
    }
}