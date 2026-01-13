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

   uint32_t currentWidth = 0;
   uint32_t currentHeight = 0;

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

      int w, h;
      glfwGetFramebufferSize(window, &w, &h);
      currentWidth = static_cast<uint32_t>(w);
      currentHeight = static_cast<uint32_t>(h);

      surfaceConfig.width = currentWidth;
      surfaceConfig.height = currentHeight;
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

   wgpu::TextureView acquireNextRenderTexture(GLFWwindow* window) {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      if (width == 0 || height == 0) {
         return nullptr;
      }

      if (width != static_cast<int>(currentWidth) || height != static_cast<int>(currentHeight)) {
         currentWidth = width;
         currentHeight = height;
         surfaceConfig.width = currentWidth;
         surfaceConfig.height = currentHeight;
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
