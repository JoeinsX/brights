#pragma once

#include "core/graphics/camera.hpp"
#include "core/graphics/renderSettings.hpp"
#include "core/world/chunk.hpp"
#include "core/world/contents/entity.hpp"
#include "core/world/contents/tile.hpp"
#include "core/world/generation/worldGenerator.hpp"
#include "core/world/graphics/planetGraphics.hpp"
#include "core/world/graphics/shaderBindings.hpp"
#include "core/world/graphics/worldRenderAdapter.hpp"
#include "core/world/planetProjection.hpp"
#include "core/world/worldArea.hpp"
#include "render/gpuTexture.hpp"
#include "util/threadpool.hpp"

#include <cmath>
#include <optional>
#include <webgpu/webgpu.hpp>

struct PlanetConfig {
   glm::vec2 position{0.0f, 0.0f};
   uint64_t seed = 0;
   float baseSize = 1024.0f;
   glm::vec2 idleScrollSpeed{0.0f, 0.0f};   // tiles per second
   glm::vec2 orbitParams{0.0f, 0.0f};       // x: radius, y: speed
};

struct PlanetContext {
   wgpu::Device device;
   wgpu::Queue queue;
   wgpu::BindGroupLayout terrainLayout;
   wgpu::BindGroupLayout spriteLayout;
   Threadpool& threadPool;
   const GpuTexture& atlas;
   const GpuTexture& entitySheet;
   TileRegistry& tileRegistry;
   EntityRegistry& entityRegistry;
};

class Planet {
public:
   Planet(const PlanetConfig& config, const PlanetContext& ctx):
      config(config), projection{config.baseSize * 0.5f}, generator(config.seed), gpu(ctx.device, ctx.queue, ctx.terrainLayout, ctx.spriteLayout, ctx.atlas, ctx.entitySheet),
      renderAdapter(ctx.queue, gpu.packedBuffer(), gpu.tilemapBuffer(), gpu.spriteBuffer()),
      worldArea(ctx.threadPool, ctx.tileRegistry, ctx.entityRegistry, generator, renderAdapter, Chunk::COUNT / 2, 0) {
      if (std::abs(config.orbitParams.x) > 0.001f) {
         currentOrbitAngle = std::atan2(config.position.y, config.position.x);
      }
   }

   void update(const float dtSeconds, const bool focused, const glm::vec2 controlAxis, const std::optional<glm::vec2> cursorWorld) {
      if (std::abs(config.orbitParams.x) > 0.001f) {
         currentOrbitAngle += config.orbitParams.y * dtSeconds;
         config.position.x = std::cos(currentOrbitAngle) * config.orbitParams.x;
         config.position.y = std::sin(currentOrbitAngle) * config.orbitParams.x;
      }

      if (!focused && glm::length(config.idleScrollSpeed) > 0.0001f) {
         localCamera.setOffset(localCamera.getOffset() + config.idleScrollSpeed * dtSeconds);
      }

      worldArea.update(localCamera, chunkMove, dtSeconds, controlAxis, cursorWorld);
      renderAdapter.update(localCamera, chunkMove);
   }

   void preRender(Camera& globalCamera, glm::ivec2 windowSize, float depth, const RenderSettings& settings) { updateUniforms(windowSize, globalCamera, depth, settings); }

   [[nodiscard]] std::optional<glm::vec2> pickWorld(const glm::vec2 screenPos, const Camera& globalCamera, const glm::ivec2 windowSize) const {
      return projection.pickWorld(screenPos, windowSize, globalCamera, config.position, localCamera.getOffset(), chunkMove);
   }

   [[nodiscard]] std::optional<glm::ivec2> pickTile(const glm::vec2 screenPos, const Camera& globalCamera, const glm::ivec2 windowSize) const {
      return projection.pickTile(screenPos, windowSize, globalCamera, config.position, localCamera.getOffset(), chunkMove);
   }

   void panLocal(const glm::vec2 deltaPixels, const float galaxyScale) {
      localCamera.setScale(projection.pixelsPerTile(galaxyScale));
      localCamera.pan(deltaPixels);
   }

   void togglePossession() { worldArea.togglePossession(localCamera, chunkMove); }

   [[nodiscard]] bool isPossessing() const { return worldArea.isPossessing(); }

   [[nodiscard]] WorldArea& area() { return worldArea; }
   [[nodiscard]] const WorldArea& area() const { return worldArea; }

   [[nodiscard]] wgpu::BindGroup getBindGroup() const { return gpu.bindGroup(); }
   [[nodiscard]] wgpu::BindGroup getSpriteBindGroup() const { return gpu.spriteBindGroup(); }
   [[nodiscard]] const PlanetConfig& getConfig() const { return config; }

   [[nodiscard]] float getPlanetRadius() const { return projection.planetRadius; }

   [[nodiscard]] float getPixelsPerTile(float cameraScale) const { return projection.pixelsPerTile(cameraScale); }

   [[nodiscard]] float getFocusScaleForPixelsPerTile(float targetPixelsPerTile) const { return projection.focusScaleForPixelsPerTile(targetPixelsPerTile); }

private:
   void updateUniforms(glm::ivec2 windowSize, Camera& globalCamera, float depth, const RenderSettings& settings) {
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
                              .sphereMapScale = PlanetProjection::sphereTileCoverage,
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

      gpu.writeUniforms(uData);
   }

   float currentOrbitAngle = 0.0f;
   glm::ivec2 chunkMove{};

   PlanetConfig config;
   PlanetProjection projection;
   Camera localCamera{};

   WorldGenerator generator;
   PlanetGraphics gpu;
   WorldRenderAdapter renderAdapter;
   WorldArea worldArea;
};
