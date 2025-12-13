#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>

#include <iostream>
#include <vector>
#include "wgpuHelpers.hpp"

class GraphicsContext {
public:
    GraphicsContext(Window* window)
        : window(window) {
        initializeInstance();
        initializeDevice();
    }

    ~GraphicsContext() {
        if(queue) wgpuQueueRelease(queue);
        if(device) wgpuDeviceRelease(device);
        if(instance) wgpuInstanceRelease(instance);
        wgpuSurfaceRelease(surface);
    }

    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;

    WGPUDevice getDevice() const { return device; }
    WGPUQueue getQueue() const { return queue; }
    WGPUSurface getSurface() { return surface; }

private:
    Window* window = nullptr;
    WGPUInstance instance = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUSurface surface;

    void initializeInstance() {
        WGPUInstanceDescriptor desc = {};
        desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
        // Dawn specific toggles
        WGPUDawnTogglesDescriptor toggles;
        toggles.chain.next = nullptr;
        toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
        toggles.disabledToggleCount = 0;
        toggles.enabledToggleCount = 1;
        const char* toggleName = "enable_immediate_error_handling";
        toggles.enabledToggles = &toggleName;
        desc.nextInChain = &toggles.chain;
#endif

        instance = wgpuCreateInstance(&desc);
        if(!instance) {
            throw std::runtime_error("Could not initialize WebGPU Instance!");
        }
        std::cout << "WGPU instance: " << instance << std::endl;
    }

    void initializeDevice() {
        std::cout << "Requesting adapter..." << std::endl;

        surface = glfwGetWGPUSurface(instance, window->getNativeHandle());

        WGPURequestAdapterOptions adapterOpts = {};
        adapterOpts.nextInChain = nullptr;
        adapterOpts.compatibleSurface = surface;

        WGPUAdapter adapter = wgpu_utils::requestAdapterSync(
            instance, &adapterOpts);
        std::cout << "Got adapter: " << adapter << std::endl;

        wgpu_utils::inspectAdapter(adapter);

        std::cout << "Requesting device..." << std::endl;
        WGPUDeviceDescriptor deviceDesc = {};
        deviceDesc.nextInChain = nullptr;
        deviceDesc.label = "My Device";
        deviceDesc.requiredFeatureCount = 0;
        deviceDesc.requiredLimits = nullptr;
        deviceDesc.defaultQueue.nextInChain = nullptr;
        deviceDesc.defaultQueue.label = "The default queue";

        deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason,
                                           char const* message, void*) {
            std::cout << "Device lost: reason " << reason;
            if(message) std::cout << " (" << message << ")";
            std::cout << std::endl;
        };

        device = wgpu_utils::requestDeviceSync(adapter, &deviceDesc);
        std::cout << "Got device: " << device << std::endl;

        auto onDeviceError = [
            ](WGPUErrorType type, char const* message, void*) {
            std::cout << "Uncaptured device error: type " << type;
            if(message) std::cout << " (" << message << ")";
            std::cout << std::endl;
        };
        wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);

        wgpu_utils::inspectDevice(device);

        WGPUSurfaceConfiguration config = {};
        config.width = 640;
        config.height = 480;

        WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(
            surface, adapter);
        config.format = surfaceFormat;
        config.viewFormatCount = 0;
        config.viewFormats = nullptr;

        config.usage = WGPUTextureUsage_RenderAttachment;
        config.device = device;
        config.presentMode = WGPUPresentMode_Fifo;
        config.alphaMode = WGPUCompositeAlphaMode_Auto;

        config.nextInChain = nullptr;

        wgpuSurfaceConfigure(surface, &config);

        queue = wgpuDeviceGetQueue(device);

        wgpuAdapterRelease(adapter);
    }
};