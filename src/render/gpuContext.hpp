#pragma once

#include "GLFW/glfw3.h"

#include <glfw3webgpu.h>
#include <queue>
#include <webgpu/webgpu.hpp>

class GpuContext {
public:
   bool initialize(wgpu::Instance instance, GLFWwindow* window) {
      surface = glfwGetWGPUSurface(instance, window);

      wgpu::RequestAdapterOptions adapterOpts = {};
      adapterOpts.compatibleSurface = surface;

      wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
      if (!adapter) {
         return false;
      }

      const wgpu::DeviceDescriptor deviceDesc = {};
      device = adapter.requestDevice(deviceDesc);
      queue = device.getQueue();

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
   }

   wgpu::Device& getDevice() { return device; }
   wgpu::Queue& getQueue() { return queue; }
   wgpu::TextureFormat& getSurfaceFormat() { return surfaceFormat; }

private:
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
      depthTextureDesc.format = wgpu::TextureFormat::Depth24Plus;
      depthTextureDesc.mipLevelCount = 1;
      depthTextureDesc.sampleCount = 1;
      depthTextureDesc.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
      depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
      depthTextureDesc.viewFormatCount = 1;
      depthTextureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureDesc.format;

      depthTexture = device.createTexture(depthTextureDesc);

      wgpu::TextureViewDescriptor depthTextureViewDesc;
      depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
      depthTextureViewDesc.baseArrayLayer = 0;
      depthTextureViewDesc.arrayLayerCount = 1;
      depthTextureViewDesc.baseMipLevel = 0;
      depthTextureViewDesc.mipLevelCount = 1;
      depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
      depthTextureViewDesc.format = wgpu::TextureFormat::Depth24Plus;

      depthTextureView = depthTexture.createView(depthTextureViewDesc);
   }

   wgpu::Device device = nullptr;
   wgpu::Queue queue = nullptr;
   wgpu::Surface surface = nullptr;
   wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
   wgpu::SurfaceConfiguration surfaceConfig = {};

   wgpu::Texture depthTexture = nullptr;
   wgpu::TextureView depthTextureView = nullptr;

   glm::ivec2 currentWindowSize{};
};
