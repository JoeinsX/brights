#pragma once

#include "app/settings/settings.hpp"
#include "core/graphics/renderSettings.hpp"
#include "core/world/chunk.hpp"
#include "core/world/graphics/shaderBindings.hpp"
#include "core/world/graphics/spriteInstance.hpp"
#include "render/gpuHelpers.hpp"
#include "render/graphicsContext.hpp"

#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <vector>

struct PlanetDrawData {
   wgpu::BindGroup terrainBindGroup;
   wgpu::BindGroup spriteBindGroup;
   uint32_t staticSpriteCount = 0;
   uint32_t dynamicSpriteCount = 0;
};

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

      initializeSpriteLayout(device);

      appliedDefines = renderSettings->getDefines();
      compilePipelines();
   }

   void refreshDefines() {
      std::set<std::string> defines = renderSettings->getDefines();
      if (defines == appliedDefines) {
         return;
      }
      appliedDefines = std::move(defines);
      compilePipelines();
   }

   void draw(wgpu::RenderPassEncoder pass, std::span<const PlanetDrawData> planets) {
      pass.setPipeline(pipeline);

      for (const PlanetDrawData& planet : planets) {
         pass.setBindGroup(0, planet.terrainBindGroup, 0, nullptr);
         pass.draw(6, 1, 0, 0);
      }
   }

   void drawSprites(wgpu::RenderPassEncoder pass, std::span<const PlanetDrawData> planets) {
      if (!spritePipeline) {
         return;
      }
      pass.setPipeline(spritePipeline);

      for (const PlanetDrawData& planet : planets) {
         if (planet.staticSpriteCount == 0 && planet.dynamicSpriteCount == 0) {
            continue;
         }
         pass.setBindGroup(0, planet.spriteBindGroup, 0, nullptr);
         if (planet.staticSpriteCount > 0) {
            pass.draw(6, planet.staticSpriteCount, 0, 0);
         }
         if (planet.dynamicSpriteCount > 0) {
            pass.draw(6, planet.dynamicSpriteCount, 0, staticSpriteCapacity);
         }
      }
   }

   [[nodiscard]] wgpu::BindGroupLayout getBindGroupLayout() const { return bindGroupLayout; }
   [[nodiscard]] wgpu::BindGroupLayout getSpriteBindGroupLayout() const { return spriteBindGroupLayout; }

private:
   static constexpr const char* terrainShaderPath = "assets/shaders/terrain/terrain.wgsl";
   static constexpr const char* spriteShaderPath = "assets/shaders/sprite/sprite.wgsl";

   void initializeSpriteLayout(wgpu::Device device) {
      const uint64_t uniformSize = sizeof(UniformData);
      const uint64_t instanceSize = static_cast<uint64_t>(totalSpriteCapacity) * sizeof(SpriteInstance);

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
   }

   void compilePipelines() {
      compilePipeline(terrainShaderPath, bindGroupLayout, wgpu::CompareFunction::Less, true, pipeline);
      compilePipeline(spriteShaderPath, spriteBindGroupLayout, wgpu::CompareFunction::LessEqual, false, spritePipeline);
   }

   void compilePipeline(const char* shaderPath, wgpu::BindGroupLayout layout, const wgpu::CompareFunction depthCompare, const bool depthWrite, wgpu::RenderPipeline& target) {
      wgpu::Device device = context->getDevice();
      wgpu::ShaderModule shaderModule = GraphicsContext::createShaderModule(device, shaderPath, appliedDefines);
      if (target) {
         target.release();
         target = nullptr;
      }

      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&layout);
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
      colorTarget.format = context->getSurfaceFormat();
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
      depthStencilState.depthCompare = depthCompare;
      depthStencilState.depthWriteEnabled = depthWrite ? wgpu::OptionalBool::True : wgpu::OptionalBool::False;
      depthStencilState.format = GpuContext::depthTextureFormat;
      depthStencilState.stencilReadMask = 0;
      depthStencilState.stencilWriteMask = 0;

      pipelineDesc.depthStencil = &depthStencilState;
      target = device.createRenderPipeline(pipelineDesc);

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
