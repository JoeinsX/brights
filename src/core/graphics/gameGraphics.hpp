#pragma once

#include "app/settings/settings.hpp"
#include "core/graphics/renderSettings.hpp"
#include "core/world/chunk.hpp"
#include "core/world/entity.hpp"
#include "core/world/planet.hpp"
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

namespace SpriteSlots {
   constexpr uint32_t Uniforms = 0;
   constexpr uint32_t Instances = 1;
   constexpr uint32_t TextureAtlas = 2;
   constexpr uint32_t Sampler = 3;
   constexpr uint32_t Num = 4;
}   // namespace SpriteSlots

class GameGraphics {
public:
   GameGraphics() = default;
   GameGraphics(const GameGraphics&) = delete;
   GameGraphics(GameGraphics&&) = delete;
   GameGraphics& operator =(const GameGraphics&) = delete;
   GameGraphics& operator =(GameGraphics&&) = delete;

   ~GameGraphics() {
      if (bindGroupLayout) {
         bindGroupLayout.release();
      }
      if (pipeline) {
         pipeline.release();
      }
      if (spriteBindGroupLayout) {
         spriteBindGroupLayout.release();
      }
      if (spritePipeline) {
         spritePipeline.release();
      }
   }

   void initAppComponent(Settings& settings) {
      settings.addSection<RenderSettings>();
      renderSettings = &settings.accessSection<RenderSettings>();
   }

   void initialize(GraphicsContext& ctx) {
      context = &ctx;
      wgpu::Device device = context->getDevice();

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

      appliedDefines = renderSettings->getDefines();
      compilePipeline(appliedDefines);

      initializeSprites(device);
   }

   void refreshDefines() {
      std::set<std::string> defines = renderSettings->getDefines();
      if (defines == appliedDefines) {
         return;
      }
      appliedDefines = std::move(defines);
      compilePipeline(appliedDefines);
      compileSpritePipeline(appliedDefines);
   }

   void draw(wgpu::RenderPassEncoder pass, const std::vector<std::unique_ptr<Planet>>& planets) {
      pass.setPipeline(pipeline);

      for (const auto& planet : planets) {
         pass.setBindGroup(0, planet->getBindGroup(), 0, nullptr);
         pass.draw(6, 1, 0, 0);
      }
   }

   void drawSprites(wgpu::RenderPassEncoder pass, const std::vector<std::unique_ptr<Planet>>& planets) {
      if (!spritePipeline) {
         return;
      }
      pass.setPipeline(spritePipeline);

      for (const auto& planet : planets) {
         const uint32_t count = planet->getSpriteCount();
         if (count == 0) {
            continue;
         }
         pass.setBindGroup(0, planet->getSpriteBindGroup(), 0, nullptr);
         pass.draw(6, count, 0, 0);
      }
   }

   [[nodiscard]] wgpu::BindGroupLayout getBindGroupLayout() const { return bindGroupLayout; }
   [[nodiscard]] wgpu::BindGroupLayout getSpriteBindGroupLayout() const { return spriteBindGroupLayout; }

private:
   void initializeSprites(wgpu::Device device) {
      const uint64_t uniformSize = sizeof(UniformData);
      const uint64_t instanceSize = static_cast<uint64_t>(spriteCapacity) * sizeof(Entity);

      std::vector<wgpu::BindGroupLayoutEntry> entries(SpriteSlots::Num);
      entries[SpriteSlots::Uniforms] = WGPUHelpers::
         bufferEntry(SpriteSlots::Uniforms, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, uniformSize);
      entries[SpriteSlots::Instances] = WGPUHelpers::bufferEntry(SpriteSlots::Instances, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::ReadOnlyStorage, instanceSize);
      entries[SpriteSlots::TextureAtlas] = WGPUHelpers::textureEntry(SpriteSlots::TextureAtlas, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
      entries[SpriteSlots::Sampler] = WGPUHelpers::samplerEntry(SpriteSlots::Sampler, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering);

      wgpu::BindGroupLayoutDescriptor bglDesc;
      bglDesc.entryCount = static_cast<uint32_t>(entries.size());
      bglDesc.entries = entries.data();
      spriteBindGroupLayout = device.createBindGroupLayout(bglDesc);

      compileSpritePipeline(appliedDefines);
   }

   void compileSpritePipeline(const std::set<std::string>& defines) {
      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(context->getDevice(), "assets/shaders/sprite/sprite.wgsl", defines);
      if (spritePipeline) {
         spritePipeline.release();
         spritePipeline = nullptr;
      }
      buildSpritePipeline(context->getDevice(), context->getSurfaceFormat(), shaderModule);
   }

   void buildSpritePipeline(wgpu::Device device, wgpu::TextureFormat surfaceFormat, wgpu::ShaderModule& shaderModule) {
      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&spriteBindGroupLayout);
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

      depthStencilState.depthCompare = wgpu::CompareFunction::LessEqual;
      depthStencilState.depthWriteEnabled = wgpu::OptionalBool::False;
      depthStencilState.format = GpuContext::depthTextureFormat;

      depthStencilState.stencilReadMask = 0;
      depthStencilState.stencilWriteMask = 0;

      pipelineDesc.depthStencil = &depthStencilState;
      spritePipeline = device.createRenderPipeline(pipelineDesc);

      shaderModule.release();
      pipelineLayout.release();
   }

   void compilePipeline(const std::set<std::string>& defines) {
      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(context->getDevice(), "assets/shaders/terrain/terrain.wgsl", defines);
      if (pipeline) {
         pipeline.release();
         pipeline = nullptr;
      }
      buildRenderPipeline(context->getDevice(), context->getSurfaceFormat(), shaderModule);
   }

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

   GraphicsContext* context = nullptr;
   RenderSettings* renderSettings = nullptr;
   wgpu::BindGroupLayout bindGroupLayout = nullptr;
   wgpu::RenderPipeline pipeline = nullptr;
   wgpu::BindGroupLayout spriteBindGroupLayout = nullptr;
   wgpu::RenderPipeline spritePipeline = nullptr;
   std::set<std::string> appliedDefines;
};
