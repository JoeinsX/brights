#pragma once

#include "GLFW/glfw3.h"

#include <glfw3webgpu.h>
#include <webgpu/webgpu.hpp>

class GpuContext {
public:
   wgpu::Device device = nullptr;
   wgpu::Queue queue = nullptr;
   wgpu::Surface surface = nullptr;
   wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
   wgpu::SurfaceConfiguration surfaceConfig = {};

   glm::ivec2 currentWindowSize{};

   bool initialize(wgpu::Instance instance, GLFWwindow* window) {
      surface = glfwGetWGPUSurface(instance, window);

      wgpu::RequestAdapterOptions adapterOpts = {};
      adapterOpts.compatibleSurface = surface;

      wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
      if (!adapter) {
         return false;
      }

      wgpu::DeviceDescriptor deviceDesc = {};
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

   void present() { surface.present(); }

   void terminate() {
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
};
