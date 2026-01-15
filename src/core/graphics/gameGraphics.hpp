#pragma once

#include "core/world/chunk.hpp"
#include "core/world/planet.hpp"
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
   constexpr uint32_t Num = 5;
}   // namespace ShaderSlots

class GameGraphics {
public:
   void initialize(GraphicsContext& ctx) {
      wgpu::Device device = ctx.getDevice();
      wgpu::Queue queue = ctx.getQueue();

      const uint64_t tileMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX) * sizeof(uint8_t);
      const uint64_t packedMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX) * sizeof(uint16_t);
      const uint64_t chunkRefSize = Chunk::COUNT_SQUARED * sizeof(uint32_t);
      const uint64_t uniformSize = sizeof(UniformData);

      atlasTexture.load(device, queue, "assets/atlas.png");

      std::vector<wgpu::BindGroupLayoutEntry> layoutEntries(ShaderSlots::Num);
      layoutEntries[ShaderSlots::Uniforms] = WGPUHelpers::
         bufferEntry(ShaderSlots::Uniforms, wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, uniformSize);
      layoutEntries[ShaderSlots::TileMap] = WGPUHelpers::bufferEntry(ShaderSlots::TileMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, tileMapSize);
      layoutEntries[ShaderSlots::TextureAtlas] = WGPUHelpers::textureEntry(ShaderSlots::TextureAtlas, wgpu::ShaderStage::Fragment);
      layoutEntries[ShaderSlots::Sampler] = WGPUHelpers::samplerEntry(ShaderSlots::Sampler, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering);
      layoutEntries[ShaderSlots::PackedMap] = WGPUHelpers::
         bufferEntry(ShaderSlots::PackedMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, packedMapSize);

      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(device, "assets/shaders/terrain/terrain.wgsl");

      createPipeline(device, ctx.getSurfaceFormat(), layoutEntries, shaderModule);
   }

   void render(GraphicsContext& ctx, Window& window, const std::vector<std::unique_ptr<Planet>>& planets) {
      if (!ctx.beginFrame(window)) {
         return;
      }

      wgpu::RenderPassEncoder pass = ctx.beginRenderPass({0.0, 0.0, 0.0, 1.0});

      pass.setPipeline(pipeline);

      // Simply iterate and draw
      for (const auto& planet : planets) {
         pass.setBindGroup(0, planet->getBindGroup(), 0, nullptr);
         pass.draw(6, 1, 0, 0);
      }

      pass.end();
      pass.release();
      ctx.endFrame();
   }

   void terminate() {
      if (bindGroupLayout) {
         bindGroupLayout.release();
      }
      if (pipeline) {
         pipeline.release();
      }

      atlasTexture.destroy();
   }

   [[nodiscard]] wgpu::BindGroupLayout getBindGroupLayout() const { return bindGroupLayout; }
   [[nodiscard]] const GpuTexture& getAtlas() const { return atlasTexture; }

private:
   void createPipeline(wgpu::Device device, wgpu::TextureFormat surfaceFormat, std::vector<wgpu::BindGroupLayoutEntry>& layoutEntries, wgpu::ShaderModule& shaderModule) {
      wgpu::BindGroupLayoutDescriptor bglDesc;
      bglDesc.entryCount = static_cast<uint32_t>(layoutEntries.size());
      bglDesc.entries = layoutEntries.data();
      bindGroupLayout = device.createBindGroupLayout(bglDesc);

      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bindGroupLayout);
      wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(layoutDesc);

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

      wgpu::DepthStencilState depthStencilState = wgpu::Default;

      depthStencilState.depthCompare = wgpu::CompareFunction::Less;
      depthStencilState.depthWriteEnabled = true;

      wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
      depthStencilState.format = depthTextureFormat;

      depthStencilState.stencilReadMask = 0;
      depthStencilState.stencilWriteMask = 0;

      pipelineDesc.depthStencil = &depthStencilState;
      pipeline = device.createRenderPipeline(pipelineDesc);

      shaderModule.release();
      pipelineLayout.release();
   }

   wgpu::BindGroupLayout bindGroupLayout = nullptr;
   wgpu::RenderPipeline pipeline = nullptr;
   GpuTexture atlasTexture;
};
