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
      currentTextureView = gpu->acquireNextRenderTexture(window);
      if (!currentTextureView) {
         return false;
      }

      currentEncoder = gpu->device.createCommandEncoder({});
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

      return currentEncoder.beginRenderPass(renderPassDesc);
   }

   void endFrame() {
      if (currentTextureView) {
         currentTextureView.release();
         currentTextureView = nullptr;
      }

      wgpu::CommandBuffer command = currentEncoder.finish({});
      currentEncoder.release();

      gpu->queue.submit(1, &command);
      gpu->present();

      command.release();
   }

   [[nodiscard]] wgpu::Device getDevice() const { return gpu->device; }
   [[nodiscard]] wgpu::Queue getQueue() const { return gpu->queue; }
   [[nodiscard]] wgpu::TextureFormat getSurfaceFormat() const { return gpu->surfaceFormat; }

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
};
