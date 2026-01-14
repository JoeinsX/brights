#pragma once

#include "core/graphics/gameGraphics.hpp"
#include "core/world/world.hpp"
#include "core/world/worldGenerator.hpp"
#include "core/world/worldRenderAdapter.hpp"
#include "platform/input.hpp"

#include <webgpu/webgpu.hpp>

class Game {
public:
   Game(): rng(0) {}

   void initialize(GameGraphics* _graphicsCtx, wgpu::Queue _queue) {
      graphicsCtx = _graphicsCtx;
      queue = _queue;

      initializeGameContent();

      worldGenerator = std::make_unique<WorldGenerator>();
      worldRenderAdapter = std::make_unique<WorldRenderAdapter>(
         queue, graphicsCtx->getChunkRefMapBuffer().getBuffer(), graphicsCtx->getPackedBuffer().getBuffer(), graphicsCtx->getTilemapBuffer().getBuffer());
      world = std::make_unique<World>(registry, rng, *worldGenerator, *worldRenderAdapter, Chunk::COUNT / 2, 0);
   }

   void update(float deltaTime, const Input& input, glm::ivec2 windowSize) {
      processInput(input, windowSize);

      static glm::ivec2 globalChunkMove{};
      world->update(camera, globalChunkMove);
      worldRenderAdapter->update(camera, globalChunkMove);

      updateUniforms(windowSize, globalChunkMove);
   }

private:
   void initializeGameContent() {
      registry.registerTile(TileID::Grass, 0, 0, 4, 1.0);
      registry.registerTile(TileID::Water, 1, 0, 4, 0.6);
      registry.registerTile(TileID::ColdGrass, 2, 0, 4, 1.0);
      registry.registerTile(TileID::Stone, 3, 0, 4, 1.8, 0.4);
      registry.registerTile(TileID::HardStone, 4, 0, 4, 1.8, 0.4);
      registry.registerTile(TileID::Gravel, 5, 0, 1, 1.0, 0.7);
      registry.registerTile(TileID::HardGravel, 6, 0, 1, 1.0);
      registry.registerTile(TileID::Snow, 5, 1, 4, 1.0, 0.5);
      registry.registerTile(TileID::Ice, 6, 1, 4, 0.8);
      registry.registerTile(TileID::Planks, 7, 0, 1, 1.8, 0.0);
      registry.registerTile(TileID::PlankFloor, 8, 0, 1, 1.0);
      registry.registerTile(TileID::RedOre, 9, 0, 1, 1.6);
      registry.registerTile(TileID::BlueOre, 10, 0, 1, 1.6);
      registry.registerTile(TileID::ColdWater, 1, 5, 4, 0.7);
      registry.registerTile(TileID::BurntGround, 11, 0, 1, 0.7);
      registry.registerTile(TileID::Sand, 12, 0, 4, 0.8);
   }

   void processInput(const Input& input, const glm::ivec2 windowSize) {
      glm::vec2 movementVector = glm::vec2(static_cast<float>(input.isKeyDown(GLFW_KEY_A)) - static_cast<float>(input.isKeyDown(GLFW_KEY_D)),
                                           static_cast<float>(input.isKeyDown(GLFW_KEY_W)) - static_cast<float>(input.isKeyDown(GLFW_KEY_S)));

      if (glm::length(movementVector) > 0.1f) {
         movementVector = glm::normalize(movementVector);
         camera.pan(movementVector * 10.f);
      }

      if (input.isDragging()) {
         camera.pan(input.getMouseDelta());
      }

      if (input.getScrollDelta().y != 0.0f) {
         camera.zoom(input.getScrollDelta().y, input.getMousePosition(), windowSize);
      }
   }

   void updateUniforms(glm::ivec2 windowSize, const glm::ivec2& globalChunkMove) {
      static constexpr float baseResolutionX = 640.f;
      static constexpr float baseResolutionY = 480.f;
      static constexpr float basePerspectiveStrength = 0.002f;
      static constexpr float perspectiveStrength = 0.002f;

      const glm::vec2 halfScreenWorld = static_cast<glm::vec2>(windowSize) * 0.5f / camera.getScale();

      const glm::vec2 shaderOffset = camera.getOffset() - halfScreenWorld;

      const auto macroOffset = static_cast<glm::ivec2>(glm::floor(shaderOffset));

      const UniformData uData{.macroOffset = macroOffset,
                              .offset = shaderOffset - static_cast<glm::vec2>(macroOffset),
                              .res = static_cast<glm::vec2>(windowSize),
                              .scale = camera.getScale(),
                              .mapSize = static_cast<uint32_t>(Chunk::SIZE) * Chunk::COUNT,
                              .sphereMapScale = static_cast<float>(Chunk::COUNT - 2) / static_cast<float>(Chunk::COUNT),
                              .chunkSize = Chunk::SIZE,
                              .chunkOffset = globalChunkMove,
                              .resScale = static_cast<glm::vec2>(windowSize) / glm::vec2{baseResolutionX, baseResolutionY},
                              .perspectiveStrength = perspectiveStrength,
                              .perspectiveScale = perspectiveStrength / basePerspectiveStrength};

      queue.writeBuffer(graphicsCtx->getUniformBuffer().getBuffer(), 0, &uData, sizeof(UniformData));
   }

   GameGraphics* graphicsCtx = nullptr;
   wgpu::Queue queue = nullptr;

   TileRegistry registry;

   std::unique_ptr<WorldGenerator> worldGenerator;
   std::unique_ptr<WorldRenderAdapter> worldRenderAdapter;
   std::unique_ptr<World> world;

   Camera camera;
   std::mt19937 rng;
};
