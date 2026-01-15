#pragma once

#include "core/graphics/camera.hpp"
#include "core/world/world.hpp"
#include "core/world/worldRenderAdapter.hpp"
#include "render/gpuBuffer.hpp"
#include "render/gpuHelpers.hpp"   // For binding helpers
#include "render/gpuTexture.hpp"

#include <cmath>
#include <memory>
#include <webgpu/webgpu.hpp>

struct UniformData {
   glm::ivec2 macroOffset;
   glm::vec2 offset;
   glm::vec2 centerOffset;
   glm::vec2 res;
   float scale;
   float sphereMapScale;
   glm::ivec2 chunkOffset;
   glm::vec2 resScale;
   float perspectiveStrength;
   float perspectiveScale;
   float planetRadius;
   glm::vec3 _pad;
};

struct PlanetConfig {
   glm::vec2 position{0.0f, 0.0f};
   uint64_t seed = 0;
   float baseSize = 1024.0f;
   glm::vec2 idleScrollSpeed{0.0f, 0.0f};
   glm::vec2 orbitParams{0.0f, 0.0f};   // x: radius, y: speed

   glm::vec2 getVelocity() const { return {-position.y * orbitParams.y, position.x * orbitParams.y}; }
};

struct PlanetRenderData {
   GpuBuffer tilemapBuffer;
   GpuBuffer packedBuffer;
   GpuBuffer uniformBuffer;

   wgpu::BindGroup bindGroup = nullptr;
};

class Planet {
public:
   Planet(const PlanetConfig& config, TileRegistry& registry): config(config), registry(registry), generator(config.seed) {
      if (std::abs(config.orbitParams.x) > 0.001f) {
         currentOrbitAngle = std::atan2(config.position.y, config.position.x);
      }
   }

   ~Planet() { terminate(); }

   void initialize(wgpu::Device device, wgpu::Queue _queue, wgpu::BindGroupLayout layout, ThreadPool& threadPool, const GpuTexture& sharedAtlas) {
      queue = _queue;

      const uint64_t tileMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX) * sizeof(uint8_t);
      const uint64_t packedMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED_EX) * sizeof(uint16_t);
      const uint64_t uniformSize = sizeof(UniformData);

      renderData.tilemapBuffer.init(device, tileMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_TileMap");
      renderData.packedBuffer.init(device, packedMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_PackedMap");
      renderData.uniformBuffer.init(device, uniformSize, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "Planet_Uniforms");

      adapter = std::make_unique<WorldRenderAdapter>(queue, renderData.packedBuffer.getBuffer(), renderData.tilemapBuffer.getBuffer());

      world = std::make_unique<World>(threadPool, registry, generator, *adapter, Chunk::COUNT / 2, 0);

      std::vector<wgpu::BindGroupEntry> entries(5);
      entries[0] = WGPUHelpers::bindBuffer(0, renderData.uniformBuffer.getBuffer(), uniformSize);
      entries[1] = WGPUHelpers::bindBuffer(1, renderData.tilemapBuffer.getBuffer(), tileMapSize);
      entries[2] = WGPUHelpers::bindTexture(2, sharedAtlas.getView());
      entries[3] = WGPUHelpers::bindSampler(3, sharedAtlas.getSampler());
      entries[4] = WGPUHelpers::bindBuffer(4, renderData.packedBuffer.getBuffer(), packedMapSize);

      wgpu::BindGroupDescriptor bgDesc;
      bgDesc.layout = layout;
      bgDesc.entryCount = static_cast<uint32_t>(entries.size());
      bgDesc.entries = entries.data();
      renderData.bindGroup = device.createBindGroup(bgDesc);
   }

   void update(float dt) {
      const float dtSec = dt / 1000.0f;

      if (std::abs(config.orbitParams.x) > 0.001f) {
         currentOrbitAngle += config.orbitParams.y * dtSec;
         config.position.x = std::cos(currentOrbitAngle) * config.orbitParams.x;
         config.position.y = std::sin(currentOrbitAngle) * config.orbitParams.x;
      }

      if (glm::length(config.idleScrollSpeed) > 0.0001f) {
         localCamera.setOffset(localCamera.getOffset() + config.idleScrollSpeed / 100.f);
      }

      world->update(localCamera, chunkMove);
      adapter->update(localCamera, chunkMove);
   }

   void preRender(Camera& globalCamera, glm::ivec2 windowSize) { updateUniforms(windowSize, chunkMove, globalCamera); }

   [[nodiscard]] wgpu::BindGroup getBindGroup() const { return renderData.bindGroup; }
   [[nodiscard]] const PlanetConfig& getConfig() const { return config; }

   void terminate() {
      if (renderData.bindGroup) {
         renderData.bindGroup.release();
      }
      renderData.tilemapBuffer.destroy();
      renderData.packedBuffer.destroy();
      renderData.uniformBuffer.destroy();
   }

   Camera localCamera{};

private:
   void updateUniforms(glm::ivec2 windowSize, const glm::ivec2& chunkMove, Camera& globalCamera) {
      static constexpr float baseResolutionX = 640.f;
      static constexpr float baseResolutionY = 480.f;
      static constexpr float basePerspectiveStrength = 0.002f;
      static constexpr float perspectiveStrength = 0.002f;

      const glm::vec2 localOffset = localCamera.getOffset();
      const auto macroOffset = static_cast<glm::ivec2>(glm::floor(localOffset));
      const glm::vec2 shaderOffset = localOffset - static_cast<glm::vec2>(macroOffset);

      const glm::vec2 res = static_cast<glm::vec2>(windowSize);

      const glm::vec2 globalDiff = globalCamera.getOffset() - config.position;
      const glm::vec2 centerOffset = globalDiff * globalCamera.getScale() / res - glm::vec2(0.5f);

      const UniformData uData{.macroOffset = macroOffset,
                              .offset = shaderOffset,
                              .centerOffset = centerOffset,
                              .res = res,
                              .scale = globalCamera.getScale(),
                              .sphereMapScale = static_cast<float>(Chunk::COUNT - 2) / static_cast<float>(Chunk::COUNT),
                              .chunkOffset = chunkMove,
                              .resScale = res / glm::vec2{baseResolutionX, baseResolutionY},
                              .perspectiveStrength = perspectiveStrength,
                              .perspectiveScale = perspectiveStrength / basePerspectiveStrength,
                              .planetRadius = config.baseSize / 2.0f,
                              ._pad = {0.0f, 0.0f, 0.0f}};

      queue.writeBuffer(renderData.uniformBuffer.getBuffer(), 0, &uData, sizeof(UniformData));
   }

   float currentOrbitAngle = 0.0f;
   glm::ivec2 chunkMove{};

   PlanetConfig config;
   PlanetRenderData renderData;

   TileRegistry& registry;
   WorldGenerator generator;

   wgpu::Queue queue = nullptr;
   std::unique_ptr<World> world;
   std::unique_ptr<WorldRenderAdapter> adapter;
};
