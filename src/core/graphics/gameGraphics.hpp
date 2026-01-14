#pragma once

#include "core/world/chunk.hpp"
#include "platform/window.hpp"
#include "render/gpuBuffer.hpp"
#include "render/gpuHelpers.hpp"
#include "render/gpuTexture.hpp"
#include "render/graphicsContext.hpp"

#include <vector>

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

class GameGraphics {
public:
   void initialize(GraphicsContext& ctx) {
      wgpu::Device device = ctx.getDevice();
      wgpu::Queue queue = ctx.getQueue();

      const uint64_t tileMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX) * sizeof(uint8_t);
      const uint64_t packedMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX) * sizeof(uint16_t);
      const uint64_t chunkRefSize = Chunk::COUNT_SQUARED * sizeof(uint32_t);
      const uint64_t uniformSize = sizeof(UniformData);

      tilemapBuffer.init(device, tileMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "TileMap");
      packedBuffer.init(device, packedMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "PackedMap");
      chunkRefMapBuffer.init(device, chunkRefSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "ChunkRefMap");
      uniformBuffer.init(device, uniformSize, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "Uniforms");

      atlasTexture.load(device, queue, "assets/atlas.png");

      std::vector<wgpu::BindGroupLayoutEntry> layoutEntries(ShaderSlots::Num);
      layoutEntries[ShaderSlots::Uniforms] = WGPUHelpers::
         bufferEntry(ShaderSlots::Uniforms, wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, uniformSize);
      layoutEntries[ShaderSlots::TileMap] = WGPUHelpers::bufferEntry(ShaderSlots::TileMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, tileMapSize);
      layoutEntries[ShaderSlots::TextureAtlas] = WGPUHelpers::textureEntry(ShaderSlots::TextureAtlas, wgpu::ShaderStage::Fragment);
      layoutEntries[ShaderSlots::Sampler] = WGPUHelpers::samplerEntry(ShaderSlots::Sampler, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering);
      layoutEntries[ShaderSlots::PackedMap] = WGPUHelpers::
         bufferEntry(ShaderSlots::PackedMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, packedMapSize);
      layoutEntries[ShaderSlots::ChunkRefMap] = WGPUHelpers::
         bufferEntry(ShaderSlots::ChunkRefMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, chunkRefSize);

      std::vector<wgpu::BindGroupEntry> groupEntries(ShaderSlots::Num);
      groupEntries[ShaderSlots::Uniforms] = WGPUHelpers::bindBuffer(ShaderSlots::Uniforms, uniformBuffer.getBuffer(), uniformSize);
      groupEntries[ShaderSlots::TileMap] = WGPUHelpers::bindBuffer(ShaderSlots::TileMap, tilemapBuffer.getBuffer(), tileMapSize);
      groupEntries[ShaderSlots::TextureAtlas] = WGPUHelpers::bindTexture(ShaderSlots::TextureAtlas, atlasTexture.getView());
      groupEntries[ShaderSlots::Sampler] = WGPUHelpers::bindSampler(ShaderSlots::Sampler, atlasTexture.getSampler());
      groupEntries[ShaderSlots::PackedMap] = WGPUHelpers::bindBuffer(ShaderSlots::PackedMap, packedBuffer.getBuffer(), packedMapSize);
      groupEntries[ShaderSlots::ChunkRefMap] = WGPUHelpers::bindBuffer(ShaderSlots::ChunkRefMap, chunkRefMapBuffer.getBuffer(), chunkRefSize);

      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(device, "assets/shaders/terrain/terrain.wgsl");

      createPipeline(device, ctx.getSurfaceFormat(), layoutEntries, groupEntries, shaderModule);
   }

   void render(GraphicsContext& ctx, Window& window) {
      if (!ctx.beginFrame(window)) {
         return;
      }

      wgpu::RenderPassEncoder pass = ctx.beginRenderPass({0.0, 0.0, 0.0, 1.0});

      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup, 0, nullptr);
      pass.draw(6, 1, 0, 0);

      pass.end();
      pass.release();

      ctx.endFrame();
   }

   void terminate() {
      if (bindGroup) {
         bindGroup.release();
      }
      if (bindGroupLayout) {
         bindGroupLayout.release();
      }
      if (pipeline) {
         pipeline.release();
      }

      tilemapBuffer.destroy();
      packedBuffer.destroy();
      chunkRefMapBuffer.destroy();
      uniformBuffer.destroy();
      atlasTexture.destroy();
   }

   [[nodiscard]] const GpuBuffer& getTilemapBuffer() const { return tilemapBuffer; }
   [[nodiscard]] const GpuBuffer& getPackedBuffer() const { return packedBuffer; }
   [[nodiscard]] const GpuBuffer& getChunkRefMapBuffer() const { return chunkRefMapBuffer; }
   [[nodiscard]] const GpuBuffer& getUniformBuffer() const { return uniformBuffer; }

private:
   void createPipeline(wgpu::Device device, wgpu::TextureFormat surfaceFormat, std::vector<wgpu::BindGroupLayoutEntry>& layoutEntries,
                       std::vector<wgpu::BindGroupEntry>& groupEntries, wgpu::ShaderModule& shaderModule) {
      wgpu::BindGroupLayoutDescriptor bglDesc;
      bglDesc.entryCount = static_cast<uint32_t>(layoutEntries.size());
      bglDesc.entries = layoutEntries.data();
      bindGroupLayout = device.createBindGroupLayout(bglDesc);

      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bindGroupLayout);
      wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(layoutDesc);

      wgpu::BindGroupDescriptor bgDesc;
      bgDesc.layout = bindGroupLayout;
      bgDesc.entryCount = static_cast<uint32_t>(groupEntries.size());
      bgDesc.entries = groupEntries.data();
      bindGroup = device.createBindGroup(bgDesc);

      wgpu::RenderPipelineDescriptor pipelineDesc;
      pipelineDesc.layout = pipelineLayout;
      pipelineDesc.vertex.module = shaderModule;
      pipelineDesc.vertex.entryPoint = "vs_main";
      pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

      wgpu::FragmentState fragmentState;
      fragmentState.module = shaderModule;
      fragmentState.entryPoint = "fs_main";

      wgpu::ColorTargetState colorTarget;
      colorTarget.format = surfaceFormat;
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

      pipeline = device.createRenderPipeline(pipelineDesc);

      shaderModule.release();
      pipelineLayout.release();
   }

   wgpu::RenderPipeline pipeline = nullptr;
   wgpu::BindGroup bindGroup = nullptr;
   wgpu::BindGroupLayout bindGroupLayout = nullptr;

   GpuBuffer tilemapBuffer;
   GpuBuffer packedBuffer;
   GpuBuffer chunkRefMapBuffer;
   GpuBuffer uniformBuffer;
   GpuTexture atlasTexture;
};
