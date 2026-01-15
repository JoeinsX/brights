#pragma once

#include "GLFW/glfw3.h"
#include "gpuContext.hpp"
#include "wgslPreprocessor.hpp"

#include <filesystem>
#include <string>
#include <webgpu/webgpu.hpp>

class GraphicsContext {
public:
   void initialize(GpuContext* context) { gpu = context; }

   [[nodiscard]] bool beginFrame(Window& window) {
      // acquiring the render texture triggers the resize logic in GpuContext if needed
      currentTextureView = gpu->acquireNextRenderTexture(window);
      if (!currentTextureView) {
         return false;
      }

      // Now we just retrieve the already resized depth view
      currentDepthTextureView = gpu->getDepthTextureView();
      if (!currentDepthTextureView) {
         return false;
      }

      currentEncoder = gpu->getDevice().createCommandEncoder({});
      return true;
   }

   wgpu::RenderPassEncoder beginRenderPass(const wgpu::Color& clearColor) {
      wgpu::RenderPassColorAttachment attachment = {};
      attachment.view = currentTextureView;
      attachment.loadOp = wgpu::LoadOp::Clear;
      attachment.storeOp = wgpu::StoreOp::Store;
      attachment.clearValue = clearColor;

      wgpu::RenderPassDescriptor renderPassDesc = {};
      renderPassDesc.colorAttachmentCount = 1;
      renderPassDesc.colorAttachments = &attachment;

      wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;

      // Use the view managed by GpuContext
      depthStencilAttachment.view = currentDepthTextureView;

      // The initial value of the depth buffer, meaning "far"
      depthStencilAttachment.depthClearValue = 1.0f;
      // Operation settings comparable to the color attachment
      depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
      depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
      // we could turn off writing to the depth buffer globally here
      depthStencilAttachment.depthReadOnly = false;

      // Stencil setup, mandatory but unused
      depthStencilAttachment.stencilClearValue = 0;
      depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
      depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
      depthStencilAttachment.stencilReadOnly = true;

      renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

      return currentEncoder.beginRenderPass(renderPassDesc);
   }

   void endFrame() {
      if (currentTextureView) {
         currentTextureView.release();
         currentTextureView = nullptr;
      }
      // Depth view is owned by GpuContext now, we just null our reference
      currentDepthTextureView = nullptr;

      wgpu::CommandBuffer command = currentEncoder.finish({});
      currentEncoder.release();

      gpu->getQueue().submit(1, &command);
      gpu->present();

      command.release();
   }

   [[nodiscard]] wgpu::Device getDevice() const { return gpu->getDevice(); }
   [[nodiscard]] wgpu::Queue getQueue() const { return gpu->getQueue(); }
   [[nodiscard]] wgpu::TextureFormat getSurfaceFormat() const { return gpu->getSurfaceFormat(); }

   static wgpu::ShaderModule createShaderModule(wgpu::Device device, const std::filesystem::path& shaderCodePath) {
      const WGSLPreprocessor preprocessor;
      const std::string code = preprocessor.load(shaderCodePath);

      wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
      shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
      shaderCodeDesc.code = code.c_str();
      wgpu::ShaderModuleDescriptor shaderDesc;
      shaderDesc.nextInChain = &shaderCodeDesc.chain;

      return device.createShaderModule(shaderDesc);
   }

private:
   GpuContext* gpu = nullptr;
   wgpu::CommandEncoder currentEncoder = nullptr;
   wgpu::TextureView currentTextureView = nullptr;
   wgpu::TextureView currentDepthTextureView = nullptr;
};
