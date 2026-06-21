#pragma once

#include "platform/window.hpp"
#include "util/logger.hpp"

#include <glfw3webgpu.h>
#include <string_view>
#include <webgpu/webgpu.hpp>
#include <webgpu/wgpu.h>

class GpuContext {
public:
   GpuContext() = default;
   GpuContext(const GpuContext&) = delete;
   GpuContext(GpuContext&&) = delete;
   GpuContext& operator=(const GpuContext&) = delete;
   GpuContext& operator=(GpuContext&&) = delete;

   ~GpuContext() {
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
      if (instance) {
         instance.release();
      }
   }

   static constexpr WGPUTextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;

   bool initialize(const Window& window) {
      wgpuSetLogLevel(WGPULogLevel_Warn);
      wgpuSetLogCallback(onWgpuLog, nullptr);

      instance = wgpuCreateInstance(nullptr);
      surface = glfwCreateWindowWGPUSurface(instance, window.getHandle());

      wgpu::RequestAdapterOptions adapterOpts = {};
      adapterOpts.compatibleSurface = surface;

      wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
      if (!adapter) {
         return false;
      }

      wgpu::DeviceDescriptor deviceDesc = {};
      deviceDesc.deviceLostCallbackInfo.mode = wgpu::CallbackMode::AllowSpontaneous;
      deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const*, const WGPUDeviceLostReason reason, const WGPUStringView message, void*, void*) {
         if (reason != WGPUDeviceLostReason_Destroyed) {
            Logger::critical("[wgpu] device lost: {}", toStringView(message));
         }
      };
      deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const*, const WGPUErrorType type, const WGPUStringView message, void*, void*) {
         Logger::error("[wgpu] {} error: {}", errorTypeName(static_cast<wgpu::ErrorType>(type)), toStringView(message));
      };

      device = adapter.requestDevice(deviceDesc);
      queue = device.getQueue();

      currentWindowSize = window.framebufferSize();

      surfaceConfig.width = currentWindowSize.x;
      surfaceConfig.height = currentWindowSize.y;
      surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
      wgpu::SurfaceCapabilities capabilities = {};
      surface.getCapabilities(adapter, &capabilities);
      surfaceFormat = capabilities.formats[0];
      capabilities.freeMembers();
      surfaceConfig.format = surfaceFormat;
      surfaceConfig.device = device;
      surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
      surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
      surface.configure(surfaceConfig);

      createDepthTexture(currentWindowSize);

      adapter.release();
      return true;
   }

   wgpu::TextureView acquireNextRenderTexture(Window& window) {
      const glm::ivec2 newWindowSize = window.framebufferSize();

      if (newWindowSize.x == 0 || newWindowSize.y == 0) {
         return nullptr;
      }

      if (newWindowSize != currentWindowSize) {
         currentWindowSize = newWindowSize;
         surfaceConfig.width = newWindowSize.x;
         surfaceConfig.height = newWindowSize.y;
         surface.configure(surfaceConfig);

         createDepthTexture(currentWindowSize);
      }

      wgpu::SurfaceTexture surfaceTexture;
      surface.getCurrentTexture(&surfaceTexture);

      if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
          surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
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

   [[nodiscard]] wgpu::Device getDevice() const { return device; }
   [[nodiscard]] wgpu::Queue getQueue() const { return queue; }
   [[nodiscard]] wgpu::TextureFormat getSurfaceFormat() const { return surfaceFormat; }

private:
   static std::string_view toStringView(const WGPUStringView view) {
      if (!view.data) {
         return {};
      }
      return view.length == WGPU_STRLEN ? std::string_view(view.data) : std::string_view(view.data, view.length);
   }

   static void onWgpuLog(const WGPULogLevel level, const WGPUStringView message, void*) {
      const std::string_view text = toStringView(message);
      switch (level) {
      case WGPULogLevel_Error: Logger::error("[wgpu] {}", text); break;
      case WGPULogLevel_Warn:  Logger::warn("[wgpu] {}", text); break;
      case WGPULogLevel_Info:  Logger::info("[wgpu] {}", text); break;
      default:                 Logger::debug("[wgpu] {}", text); break;
      }
   }

   static constexpr std::string_view errorTypeName(const wgpu::ErrorType type) {
      switch (type) {
      case wgpu::ErrorType::Validation:  return "validation";
      case wgpu::ErrorType::OutOfMemory: return "out-of-memory";
      case wgpu::ErrorType::Internal:    return "internal";
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

   wgpu::Instance instance = nullptr;
   wgpu::Device device = nullptr;
   wgpu::Queue queue = nullptr;
   wgpu::Surface surface = nullptr;
   wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
   wgpu::SurfaceConfiguration surfaceConfig = {};

   wgpu::Texture depthTexture = nullptr;
   wgpu::TextureView depthTextureView = nullptr;

   glm::ivec2 currentWindowSize{};
};
