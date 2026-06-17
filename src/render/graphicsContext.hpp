#pragma once

#include "util/wgslPreprocessor.hpp"
#include "GLFW/glfw3.h"
#include "gpuContext.hpp"

#include <filesystem>
#include <string>
#include <webgpu/webgpu.hpp>

class GraphicsContext {
public:
   void initialize(GpuContext* context) { gpu = context; }

   [[nodiscard]] bool beginFrame(Window& window) {
      currentTextureView = gpu->acquireNextRenderTexture(window);
      if (!currentTextureView) {
         return false;
      }

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
      attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
      attachment.loadOp = wgpu::LoadOp::Clear;
      attachment.storeOp = wgpu::StoreOp::Store;
      attachment.clearValue = clearColor;

      wgpu::RenderPassDescriptor renderPassDesc = {};
      renderPassDesc.colorAttachmentCount = 1;
      renderPassDesc.colorAttachments = &attachment;

      wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;

      depthStencilAttachment.view = currentDepthTextureView;

      depthStencilAttachment.depthClearValue = 1.0f;
      depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
      depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
      depthStencilAttachment.depthReadOnly = false;

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
      WGSLPreprocessor preprocessor;
      preprocessor.addIncludeRoot(shaderCodePath.parent_path().parent_path());   // shaders root, so modules can #include "lib/..." directly
      const std::string code = preprocessor.load(shaderCodePath);

      wgpu::ShaderSourceWGSL shaderCodeDesc;
      shaderCodeDesc.chain.sType = wgpu::SType::ShaderSourceWGSL;
      shaderCodeDesc.code = wgpu::StringView(code);
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
