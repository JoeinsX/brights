#pragma once

#include "core/graphics/camera.hpp"
#include "core/graphics/renderSettings.hpp"
#include "core/world/entity.hpp"
#include "core/world/world.hpp"
#include "core/world/worldEdit.hpp"
#include "core/world/worldRenderAdapter.hpp"
#include "render/gpuBuffer.hpp"
#include "render/gpuHelpers.hpp"
#include "render/gpuTexture.hpp"

#include <cmath>
#include <memory>
#include <optional>
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
   float planetDepth;
   float simpleModeThreshold;
   int32_t raymarchMaxTiles;
   int32_t raymarchBinarySteps;
   float _pad;
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
   GpuBuffer spriteBuffer;

   wgpu::BindGroup bindGroup = nullptr;
   wgpu::BindGroup spriteBindGroup = nullptr;
};

class Planet {
public:
   Planet(const PlanetConfig& config, TileRegistry& registry, EntityRegistry& entityRegistry):
      config(config), registry(registry), entityRegistry(entityRegistry), generator(config.seed, entityRegistry) {
      if (std::abs(config.orbitParams.x) > 0.001f) {
         currentOrbitAngle = std::atan2(config.position.y, config.position.x);
      }
   }

   ~Planet() { terminate(); }

   void initialize(wgpu::Device device, wgpu::Queue _queue, wgpu::BindGroupLayout layout, wgpu::BindGroupLayout spriteLayout, Threadpool& threadPool, const GpuTexture& sharedAtlas,
                   const GpuTexture& sharedEntity) {
      queue = _queue;

      const uint64_t tileMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED) * sizeof(uint8_t);
      const uint64_t packedMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED) * sizeof(uint16_t);
      const uint64_t uniformSize = sizeof(UniformData);
      const uint64_t spriteBufferSize = static_cast<uint64_t>(spriteCapacity) * sizeof(Entity);

      renderData.tilemapBuffer.init(device, tileMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_TileMap");
      renderData.packedBuffer.init(device, packedMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_PackedMap");
      renderData.uniformBuffer.init(device, uniformSize, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "Planet_Uniforms");
      renderData.spriteBuffer.init(device, spriteBufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_Sprites");

      adapter = std::make_unique<WorldRenderAdapter>(queue, renderData.packedBuffer.getBuffer(), renderData.tilemapBuffer.getBuffer(), renderData.spriteBuffer.getBuffer());

      world = std::make_unique<World>(threadPool, registry, entityRegistry, generator, *adapter, Chunk::COUNT / 2, 0);

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

      std::vector<wgpu::BindGroupEntry> spriteEntries(4);
      spriteEntries[0] = WGPUHelpers::bindBuffer(0, renderData.uniformBuffer.getBuffer(), uniformSize);
      spriteEntries[1] = WGPUHelpers::bindBuffer(1, renderData.spriteBuffer.getBuffer(), spriteBufferSize);
      spriteEntries[2] = WGPUHelpers::bindTexture(2, sharedEntity.getView());
      spriteEntries[3] = WGPUHelpers::bindSampler(3, sharedEntity.getSampler());

      wgpu::BindGroupDescriptor spriteBgDesc;
      spriteBgDesc.layout = spriteLayout;
      spriteBgDesc.entryCount = static_cast<uint32_t>(spriteEntries.size());
      spriteBgDesc.entries = spriteEntries.data();
      renderData.spriteBindGroup = device.createBindGroup(spriteBgDesc);
   }

   void update(float dt, bool focused) {
      const float dtSec = dt / 1000.0f;

      if (std::abs(config.orbitParams.x) > 0.001f) {
         currentOrbitAngle += config.orbitParams.y * dtSec;
         config.position.x = std::cos(currentOrbitAngle) * config.orbitParams.x;
         config.position.y = std::sin(currentOrbitAngle) * config.orbitParams.x;
      }

      if (!focused && glm::length(config.idleScrollSpeed) > 0.0001f) {
         localCamera.setOffset(localCamera.getOffset() + config.idleScrollSpeed / 100.f);
      }

      world->update(localCamera, chunkMove);
      adapter->update(localCamera, chunkMove);
   }

   void preRender(Camera& globalCamera, glm::ivec2 windowSize, float depth, const RenderSettings& settings) {
      updateUniforms(windowSize, chunkMove, globalCamera, depth, settings);
   }

   [[nodiscard]] std::optional<glm::ivec2> pickTile(const glm::vec2 screenPos, const Camera& globalCamera, const glm::ivec2 windowSize) const {
      const glm::vec2 res = static_cast<glm::vec2>(windowSize);
      const glm::vec2 globalDiff = globalCamera.getOffset() - config.position;
      const float scale = globalCamera.getScale();

      const glm::vec2 disk = (screenPos - 0.5f * res + globalDiff * scale) / (scale * getPlanetRadius());
      const float distSq = glm::dot(disk, disk);
      if (distSq > 1.0f) {
         return std::nullopt;
      }

      const float z = std::sqrt(1.0f - distSq);
      const glm::vec2 diskUv = disk / (1.0f + z) * 0.5f;
      const glm::vec2 worldPos = localCamera.getOffset() + diskUv * mapTileSpan * sphereTileCoverage;
      return static_cast<glm::ivec2>(glm::floor(worldPos)) + chunkMove * Chunk::SIZE;
   }

   int applyBrush(const BrushSettings& brush, const glm::ivec2 worldTile, const float dtMs) { return world->applyBrush(worldTile, brush, dtMs); }

   [[nodiscard]] TileInspection inspectTile(const glm::ivec2 worldTile) const { return world->inspect(worldTile); }

   [[nodiscard]] wgpu::BindGroup getBindGroup() const { return renderData.bindGroup; }
   [[nodiscard]] wgpu::BindGroup getSpriteBindGroup() const { return renderData.spriteBindGroup; }
   [[nodiscard]] uint32_t getSpriteCount() const { return world ? world->getEntityCount() : 0u; }
   [[nodiscard]] const PlanetConfig& getConfig() const { return config; }

   [[nodiscard]] float getPlanetRadius() const { return config.baseSize * 0.5f; }

   [[nodiscard]] float getPixelsPerTile(float cameraScale) const { return cameraScale * getPlanetRadius() / centerTileStretch; }

   [[nodiscard]] float getFocusScaleForPixelsPerTile(float targetPixelsPerTile) const { return targetPixelsPerTile * centerTileStretch / getPlanetRadius(); }

   void terminate() {
      if (renderData.bindGroup) {
         renderData.bindGroup.release();
         renderData.bindGroup = nullptr;
      }
      if (renderData.spriteBindGroup) {
         renderData.spriteBindGroup.release();
         renderData.spriteBindGroup = nullptr;
      }
      renderData.tilemapBuffer.destroy();
      renderData.packedBuffer.destroy();
      renderData.uniformBuffer.destroy();
      renderData.spriteBuffer.destroy();
   }

   Camera localCamera{};

private:
   static constexpr float mapTileSpan = static_cast<float>(Chunk::SIZE * Chunk::COUNT);
   static constexpr float sphereTileCoverage = static_cast<float>(Chunk::COUNT - 2) / static_cast<float>(Chunk::COUNT);
   static constexpr float centerTileStretch = 0.25f * mapTileSpan * sphereTileCoverage;

   void updateUniforms(glm::ivec2 windowSize, const glm::ivec2& chunkMove, Camera& globalCamera, float depth, const RenderSettings& settings) {
      static constexpr float baseResolutionX = 640.f;
      static constexpr float baseResolutionY = 480.f;

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
                              .sphereMapScale = sphereTileCoverage,
                              .chunkOffset = chunkMove,
                              .resScale = res / glm::vec2{baseResolutionX, baseResolutionY},
                              .perspectiveStrength = settings.perspectiveStrength,
                              .perspectiveScale = settings.perspectiveStrength / 0.002f,
                              .planetRadius = getPlanetRadius(),
                              .planetDepth = depth,
                              .simpleModeThreshold = settings.simpleModeThreshold,
                              .raymarchMaxTiles = settings.raymarchMaxTiles,
                              .raymarchBinarySteps = settings.raymarchBinarySteps,
                              ._pad = 0.0f};

      queue.writeBuffer(renderData.uniformBuffer.getBuffer(), 0, &uData, sizeof(UniformData));
   }

   float currentOrbitAngle = 0.0f;
   glm::ivec2 chunkMove{};

   PlanetConfig config;
   PlanetRenderData renderData;

   TileRegistry& registry;
   EntityRegistry& entityRegistry;
   WorldGenerator generator;

   wgpu::Queue queue = nullptr;
   std::unique_ptr<World> world;
   std::unique_ptr<WorldRenderAdapter> adapter;
};
