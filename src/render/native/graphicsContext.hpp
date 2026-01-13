#pragma once

#include "GLFW/glfw3.h"
#include "gpuBuffer.hpp"
#include "gpuContext.hpp"
#include "gpuHelpers.hpp"
#include "gpuTexture.hpp"
#include "render/core/wgslPreprocessor.hpp"
#include "world/chunk.hpp"
#include "world/world.hpp"
#include "world/worldGenerator.hpp"
#include "world/worldRenderAdapter.hpp"

namespace ShaderSlots {
   constexpr uint32_t Uniforms = 0;
   constexpr uint32_t TileMap = 1;
   constexpr uint32_t TextureAtlas = 2;
   constexpr uint32_t Sampler = 3;
   constexpr uint32_t PackedMap = 4;
   constexpr uint32_t ChunkRefMap = 5;
   constexpr uint32_t Num = 6;
}   // namespace ShaderSlots

struct UniformData {
   glm::ivec2 macroOffset;
   glm::vec2 offset;
   glm::vec2 res;
   float scale;
   uint32_t mapSize;
   float sphereMapScale;
   uint32_t chunkSize;
   glm::ivec2 chunkOffset;
   glm::vec2 resScale;
   float perspectiveStrength;
   float perspectiveScale;
};

class GraphicsContext {
public:
   GpuContext* gpu = nullptr;

   wgpu::RenderPipeline pipeline = nullptr;

   GpuBuffer tilemapBuffer;
   GpuBuffer packedBuffer;
   GpuBuffer chunkRefMapBuffer;
   GpuBuffer uniformBuffer;

   GpuTexture atlasTexture;
   wgpu::BindGroup bindGroup = nullptr;
   wgpu::BindGroupLayout bindGroupLayout = nullptr;

   WorldRenderAdapter* worldRenderAdapter = nullptr;

   bool initialize(GpuContext* context) {
      this->gpu = context;
      return true;
   }

   bool initializeTexture() { return atlasTexture.load(gpu->device, gpu->queue, "assets/atlas.png"); }

   void initializePipeline() {
      Chunk chunk{{0, 0}};
      const auto& packedData = chunk.getPackedData();

      uint64_t tileMapSize = Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX * sizeof(uint8_t);
      uint64_t packedMapSize = packedData.size() * Chunk::COUNT_SQUARED_EX * sizeof(uint16_t);
      uint64_t chunkRefSize = Chunk::COUNT_SQUARED * sizeof(uint32_t);
      uint64_t uniformSize = sizeof(UniformData);

      tilemapBuffer.init(gpu->device, tileMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "TileMap");
      packedBuffer.init(gpu->device, packedMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "PackedMap");
      chunkRefMapBuffer.init(gpu->device, chunkRefSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "ChunkRefMap");
      uniformBuffer.init(gpu->device, uniformSize, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "Uniforms");

      WGSLPreprocessor preprocessor;
      std::string code = preprocessor.load("assets/shaders/terrain/terrain.wgsl");
      wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
      shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
      shaderCodeDesc.code = code.c_str();
      wgpu::ShaderModuleDescriptor shaderDesc;
      shaderDesc.nextInChain = &shaderCodeDesc.chain;
      wgpu::ShaderModule shaderModule = gpu->device.createShaderModule(shaderDesc);

      std::vector<wgpu::BindGroupLayoutEntry> bgEntries(ShaderSlots::Num);

      bgEntries[ShaderSlots::Uniforms] = WGPUHelpers::
         bufferEntry(ShaderSlots::Uniforms, wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, uniformSize);
      bgEntries[ShaderSlots::TileMap] = WGPUHelpers::bufferEntry(ShaderSlots::TileMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, tileMapSize);
      bgEntries[ShaderSlots::TextureAtlas] = WGPUHelpers::textureEntry(ShaderSlots::TextureAtlas, wgpu::ShaderStage::Fragment);
      bgEntries[ShaderSlots::Sampler] = WGPUHelpers::samplerEntry(ShaderSlots::Sampler, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering);
      bgEntries[ShaderSlots::PackedMap] = WGPUHelpers::bufferEntry(ShaderSlots::PackedMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, packedMapSize);
      bgEntries[ShaderSlots::ChunkRefMap] = WGPUHelpers::bufferEntry(ShaderSlots::ChunkRefMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, chunkRefSize);

      wgpu::BindGroupLayoutDescriptor bglDesc;
      bglDesc.entryCount = ShaderSlots::Num;
      bglDesc.entries = bgEntries.data();
      bindGroupLayout = gpu->device.createBindGroupLayout(bglDesc);

      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bindGroupLayout);
      wgpu::PipelineLayout pipelineLayout = gpu->device.createPipelineLayout(layoutDesc);

      std::vector<wgpu::BindGroupEntry> bgGroupEntries(ShaderSlots::Num);

      bgGroupEntries[ShaderSlots::Uniforms] = WGPUHelpers::bindBuffer(ShaderSlots::Uniforms, uniformBuffer.getBuffer(), uniformSize);
      bgGroupEntries[ShaderSlots::TileMap] = WGPUHelpers::bindBuffer(ShaderSlots::TileMap, tilemapBuffer.getBuffer(), tileMapSize);
      bgGroupEntries[ShaderSlots::TextureAtlas] = WGPUHelpers::bindTexture(ShaderSlots::TextureAtlas, atlasTexture.getView());
      bgGroupEntries[ShaderSlots::Sampler] = WGPUHelpers::bindSampler(ShaderSlots::Sampler, atlasTexture.getSampler());
      bgGroupEntries[ShaderSlots::PackedMap] = WGPUHelpers::bindBuffer(ShaderSlots::PackedMap, packedBuffer.getBuffer(), packedMapSize);
      bgGroupEntries[ShaderSlots::ChunkRefMap] = WGPUHelpers::bindBuffer(ShaderSlots::ChunkRefMap, chunkRefMapBuffer.getBuffer(), chunkRefSize);

      wgpu::BindGroupDescriptor bgDesc;
      bgDesc.layout = bindGroupLayout;
      bgDesc.entryCount = ShaderSlots::Num;
      bgDesc.entries = bgGroupEntries.data();
      bindGroup = gpu->device.createBindGroup(bgDesc);

      wgpu::RenderPipelineDescriptor pipelineDesc;
      pipelineDesc.layout = pipelineLayout;
      pipelineDesc.vertex.module = shaderModule;
      pipelineDesc.vertex.entryPoint = "vs_main";
      pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

      wgpu::FragmentState fragmentState;
      fragmentState.module = shaderModule;
      fragmentState.entryPoint = "fs_main";
      wgpu::ColorTargetState colorTarget;

      colorTarget.format = gpu->surfaceFormat;

      wgpu::BlendState blendState;
      blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
      blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      blendState.color.operation = wgpu::BlendOperation::Add;
      blendState.alpha.srcFactor = wgpu::BlendFactor::One;
      blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      blendState.alpha.operation = wgpu::BlendOperation::Add;
      colorTarget.blend = &blendState;
      colorTarget.writeMask = wgpu::ColorWriteMask::All;
      fragmentState.targetCount = 1;
      fragmentState.targets = &colorTarget;
      pipelineDesc.fragment = &fragmentState;
      pipelineDesc.multisample.count = 1;
      pipelineDesc.multisample.mask = ~0u;

      pipeline = gpu->device.createRenderPipeline(pipelineDesc);
      shaderModule.release();
      pipelineLayout.release();
   }

   void render(GLFWwindow* window) const {
      wgpu::TextureView targetView = gpu->acquireNextRenderTexture(window);
      if (!targetView) {
         return;
      }

      wgpu::CommandEncoder encoder = gpu->device.createCommandEncoder({});
      wgpu::RenderPassColorAttachment attachment = {};
      attachment.view = targetView;
      attachment.loadOp = wgpu::LoadOp::Clear;
      attachment.storeOp = wgpu::StoreOp::Store;
      attachment.clearValue = wgpu::Color{0.0, 0.0, 0.0, 1.0};

      wgpu::RenderPassDescriptor renderPassDesc = {};
      renderPassDesc.colorAttachmentCount = 1;
      renderPassDesc.colorAttachments = &attachment;

      wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
      renderPass.setPipeline(pipeline);
      renderPass.setBindGroup(0, bindGroup, 0, nullptr);
      renderPass.draw(6, 1, 0, 0);
      renderPass.end();
      renderPass.release();

      wgpu::CommandBuffer command = encoder.finish({});
      encoder.release();

      gpu->queue.submit(1, &command);
      gpu->present();

      command.release();
      targetView.release();
   }

   void terminate() {
      if (bindGroup) {
         bindGroup.release();
      }
      if (bindGroupLayout) {
         bindGroupLayout.release();
      }

      tilemapBuffer.destroy();
      packedBuffer.destroy();
      chunkRefMapBuffer.destroy();
      uniformBuffer.destroy();
      atlasTexture.destroy();

      if (pipeline) {
         pipeline.release();
      }
   }
};
