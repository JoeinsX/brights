#pragma once

#include "core/world/chunk.hpp"
#include "core/world/planet.hpp"
#include "platform/window.hpp"
#include "render/gpuHelpers.hpp"
#include "render/graphicsContext.hpp"

#include <set>
#include <string>
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
   GameGraphics() = default;
   GameGraphics(const GameGraphics&) = delete;
   GameGraphics(GameGraphics&&) = delete;
   GameGraphics& operator=(const GameGraphics&) = delete;
   GameGraphics& operator=(GameGraphics&&) = delete;

   ~GameGraphics() {
      if (bindGroupLayout) {
         bindGroupLayout.release();
      }
      if (pipeline) {
         pipeline.release();
      }
   }

   void initialize(GraphicsContext& ctx, const std::set<std::string>& defines = {}) {
      wgpu::Device device = ctx.getDevice();

      const uint64_t tileMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED) * sizeof(uint8_t);
      const uint64_t packedMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED) * sizeof(uint16_t);
      const uint64_t uniformSize = sizeof(UniformData);

      std::vector<wgpu::BindGroupLayoutEntry> layoutEntries(ShaderSlots::Num);
      layoutEntries[ShaderSlots::Uniforms] = WGPUHelpers::
         bufferEntry(ShaderSlots::Uniforms, wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, uniformSize);
      layoutEntries[ShaderSlots::TileMap] = WGPUHelpers::bufferEntry(ShaderSlots::TileMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, tileMapSize);
      layoutEntries[ShaderSlots::TextureAtlas] = WGPUHelpers::textureEntry(ShaderSlots::TextureAtlas, wgpu::ShaderStage::Fragment);
      layoutEntries[ShaderSlots::Sampler] = WGPUHelpers::samplerEntry(ShaderSlots::Sampler, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering);
      layoutEntries[ShaderSlots::PackedMap] = WGPUHelpers::
         bufferEntry(ShaderSlots::PackedMap, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::ReadOnlyStorage, packedMapSize);

      wgpu::BindGroupLayoutDescriptor bglDesc;
      bglDesc.entryCount = static_cast<uint32_t>(layoutEntries.size());
      bglDesc.entries = layoutEntries.data();
      bindGroupLayout = device.createBindGroupLayout(bglDesc);

      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(device, "assets/shaders/terrain/terrain.wgsl", defines);
      buildRenderPipeline(device, ctx.getSurfaceFormat(), shaderModule);
   }

   void rebuildPipeline(GraphicsContext& ctx, const std::set<std::string>& defines) {
      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(ctx.getDevice(), "assets/shaders/terrain/terrain.wgsl", defines);
      if (pipeline) {
         pipeline.release();
         pipeline = nullptr;
      }
      buildRenderPipeline(ctx.getDevice(), ctx.getSurfaceFormat(), shaderModule);
   }

   void draw(wgpu::RenderPassEncoder pass, const std::vector<std::unique_ptr<Planet>>& planets) {
      pass.setPipeline(pipeline);

      for (const auto& planet : planets) {
         pass.setBindGroup(0, planet->getBindGroup(), 0, nullptr);
         pass.draw(6, 1, 0, 0);
      }
   }

   [[nodiscard]] wgpu::BindGroupLayout getBindGroupLayout() const { return bindGroupLayout; }

private:
   void buildRenderPipeline(wgpu::Device device, wgpu::TextureFormat surfaceFormat, wgpu::ShaderModule& shaderModule) {
      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bindGroupLayout);
      wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(layoutDesc);

      wgpu::RenderPipelineDescriptor pipelineDesc;
      pipelineDesc.layout = pipelineLayout;
      pipelineDesc.vertex.module = shaderModule;
      pipelineDesc.vertex.entryPoint = wgpu::StringView("vs_main");
      pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

      wgpu::FragmentState fragmentState;
      fragmentState.module = shaderModule;
      fragmentState.entryPoint = wgpu::StringView("fs_main");

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
      depthStencilState.depthWriteEnabled = wgpu::OptionalBool::True;
      depthStencilState.format = GpuContext::depthTextureFormat;

      depthStencilState.stencilReadMask = 0;
      depthStencilState.stencilWriteMask = 0;

      pipelineDesc.depthStencil = &depthStencilState;
      pipeline = device.createRenderPipeline(pipelineDesc);

      shaderModule.release();
      pipelineLayout.release();
   }

   wgpu::BindGroupLayout bindGroupLayout = nullptr;
   wgpu::RenderPipeline pipeline = nullptr;
};
