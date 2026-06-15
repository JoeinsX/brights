#pragma once

#include "GLFW/glfw3.h"
#include "util/logger.hpp"

#include <glfw3webgpu.h>
#include <memory>
#include <webgpu/webgpu.hpp>
#include <webgpu/wgpu.h>

class GpuContext {
public:
   static constexpr WGPUTextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;

   bool initialize(wgpu::Instance instance, GLFWwindow* window) {
      wgpuSetLogLevel(WGPULogLevel_Warn);
      wgpuSetLogCallback(onWgpuLog, nullptr);

      surface = glfwGetWGPUSurface(instance, window);

      wgpu::RequestAdapterOptions adapterOpts = {};
      adapterOpts.compatibleSurface = surface;

      wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
      if (!adapter) {
         return false;
      }

      wgpu::DeviceDescriptor deviceDesc = {};
      deviceDesc.deviceLostCallback = [](const WGPUDeviceLostReason reason, const char* message, void*) {
         if (reason != WGPUDeviceLostReason_Destroyed) {
            Log::critical("[wgpu] device lost: {}", message);
         }
      };
      device = adapter.requestDevice(deviceDesc);
      queue = device.getQueue();

      errorCallbackHandle =
         device.setUncapturedErrorCallback([](const wgpu::ErrorType type, const char* message) { Log::error("[wgpu] {} error: {}", errorTypeName(type), message); });

      glfwGetFramebufferSize(window, &currentWindowSize.x, &currentWindowSize.y);

      surfaceConfig.width = currentWindowSize.x;
      surfaceConfig.height = currentWindowSize.y;
      surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
      surfaceFormat = surface.getPreferredFormat(adapter);
      surfaceConfig.format = surfaceFormat;
      surfaceConfig.device = device;
      surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
      surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
      surface.configure(surfaceConfig);

      // Initialize depth texture
      createDepthTexture(currentWindowSize);

      adapter.release();
      return true;
   }

   wgpu::TextureView acquireNextRenderTexture(Window& window) {
      glm::ivec2 newWindowSize;
      glfwGetFramebufferSize(window.handle, &newWindowSize.x, &newWindowSize.y);

      if (newWindowSize.x == 0 || newWindowSize.y == 0) {
         return nullptr;
      }

      if (newWindowSize != currentWindowSize) {
         currentWindowSize = newWindowSize;
         surfaceConfig.width = newWindowSize.x;
         surfaceConfig.height = newWindowSize.y;
         surface.configure(surfaceConfig);

         // Resize depth texture when window resizes
         createDepthTexture(currentWindowSize);
      }

      wgpu::SurfaceTexture surfaceTexture;
      surface.getCurrentTexture(&surfaceTexture);

      if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
         return nullptr;
      }

      wgpu::TextureViewDescriptor viewDesc;
      viewDesc.format = surfaceConfig.format;
      viewDesc.dimension = wgpu::TextureViewDimension::_2D;
      viewDesc.mipLevelCount = 1;
      viewDesc.arrayLayerCount = 1;

      return wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
   }

   [[nodiscard]] wgpu::TextureView getDepthTextureView() const { return depthTextureView; }

   void present() { surface.present(); }

   void terminate() {
      if (depthTextureView) {
         depthTextureView.release();
      }
      if (depthTexture) {
         depthTexture.destroy();
         depthTexture.release();
      }
      if (surface) {
         surface.unconfigure();
         surface.release();
      }
      if (queue) {
         queue.release();
      }
      if (device) {
         device.release();
      }
      errorCallbackHandle.reset();
   }

   [[nodiscard]] wgpu::Device getDevice() const { return device; }
   [[nodiscard]] wgpu::Queue getQueue() const { return queue; }
   [[nodiscard]] wgpu::TextureFormat getSurfaceFormat() const { return surfaceFormat; }

private:
   static void onWgpuLog(const WGPULogLevel level, const char* message, void*) {
      switch (level) {
      case WGPULogLevel_Error: Log::error("[wgpu] {}", message); break;
      case WGPULogLevel_Warn:  Log::warn("[wgpu] {}", message); break;
      case WGPULogLevel_Info:  Log::info("[wgpu] {}", message); break;
      default:                 Log::debug("[wgpu] {}", message); break;
      }
   }

   static constexpr std::string_view errorTypeName(const wgpu::ErrorType type) {
      switch (type) {
      case wgpu::ErrorType::Validation:  return "validation";
      case wgpu::ErrorType::OutOfMemory: return "out-of-memory";
      case wgpu::ErrorType::Internal:    return "internal";
      case wgpu::ErrorType::DeviceLost:  return "device-lost";
      default:                           return "unknown";
      }
   }

   void createDepthTexture(const glm::ivec2& size) {
      if (depthTexture) {
         depthTexture.destroy();
         depthTexture.release();
      }
      if (depthTextureView) {
         depthTextureView.release();
      }

      wgpu::TextureDescriptor depthTextureDesc;
      depthTextureDesc.dimension = wgpu::TextureDimension::_2D;
      depthTextureDesc.format = depthTextureFormat;
      depthTextureDesc.mipLevelCount = 1;
      depthTextureDesc.sampleCount = 1;
      depthTextureDesc.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
      depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
      depthTextureDesc.viewFormatCount = 1;
      depthTextureDesc.viewFormats = &depthTextureDesc.format;

      depthTexture = device.createTexture(depthTextureDesc);

      wgpu::TextureViewDescriptor depthTextureViewDesc;
      depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
      depthTextureViewDesc.baseArrayLayer = 0;
      depthTextureViewDesc.arrayLayerCount = 1;
      depthTextureViewDesc.baseMipLevel = 0;
      depthTextureViewDesc.mipLevelCount = 1;
      depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
      depthTextureViewDesc.format = depthTextureFormat;

      depthTextureView = depthTexture.createView(depthTextureViewDesc);
   }

   wgpu::Device device = nullptr;
   wgpu::Queue queue = nullptr;
   wgpu::Surface surface = nullptr;
   wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
   wgpu::SurfaceConfiguration surfaceConfig = {};

   wgpu::Texture depthTexture = nullptr;
   wgpu::TextureView depthTextureView = nullptr;

   std::unique_ptr<wgpu::ErrorCallback> errorCallbackHandle;

   glm::ivec2 currentWindowSize{};
};
