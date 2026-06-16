#pragma once

#include "core/graphics/gameGraphics.hpp"
#include "core/resources/resource.hpp"
#include "core/world/world.hpp"
#include "core/world/worldGenerator.hpp"
#include "core/world/worldRenderAdapter.hpp"
#include "core/world/worldView.hpp"
#include "platform/input.hpp"
#include "render/gpuTexture.hpp"
#include "util/logger.hpp"

#include <algorithm>
#include <memory>
#include <thread>
#include <utility>
#include <vector>
#include <webgpu/webgpu.hpp>

class Game {
public:
   Game(): threadPool(std::clamp(std::thread::hardware_concurrency() - 1, 1u, 4u)) {}

   [[nodiscard]] bool initialize(GameGraphics* _graphicsCtx, GpuContext& gpuContext, wgpu::Queue _queue) {
      graphicsCtx = _graphicsCtx;
      queue = _queue;

      initializeGameContent();

      wgpu::Device device = gpuContext.getDevice();
      if (!atlasTexture.load(device, queue, *resourceManager.loadImage("atlas", "assets/atlas.png"))) {
         Log::error("failed to load texture atlas 'assets/atlas.png'");
         return false;
      }

      auto p1 = std::
         make_unique<Planet>(PlanetConfig{.position = {-1200.0f, 0.0f}, .seed = 42, .baseSize = 512.0f, .idleScrollSpeed = {100.0f, 50.0f}, .orbitParams = {1000.0f, 0.2f}},
                             registry);
      p1->initialize(device, queue, graphicsCtx->getBindGroupLayout(), threadPool, atlasTexture);
      planets.push_back(std::move(p1));

      auto p2 = std::make_unique<Planet>(PlanetConfig{.position = {0.0f, 0.0f}, .seed = 1337, .baseSize = 1024.0f, .idleScrollSpeed = {-28.0f, 0.0f}}, registry);
      p2->initialize(device, queue, graphicsCtx->getBindGroupLayout(), threadPool, atlasTexture);
      planets.push_back(std::move(p2));

      auto p3 = std::
         make_unique<Planet>(PlanetConfig{.position = {1200.0f, 0.0f}, .seed = 2550, .baseSize = 300.0f, .idleScrollSpeed = {/*13.33f, -24.13f*/}, .orbitParams = {1500.0f, -0.4f}},
                             registry);
      p3->initialize(device, queue, graphicsCtx->getBindGroupLayout(), threadPool, atlasTexture);
      planets.push_back(std::move(p3));

      return true;
   }

   void update(float dt, const Input& input, glm::ivec2 windowSize) {
      worldView.handleInput(input, planets, windowSize);

      const int focusedIndex = worldView.getFocusedPlanetIndex();
      for (size_t i = 0; i < planets.size(); ++i) {
         planets[i]->update(dt, std::cmp_equal(i, focusedIndex));
      }

      worldView.update(dt, planets);

      for (size_t i = 0; i < planets.size(); ++i) {
         const float depth = static_cast<float>(i + 1) / static_cast<float>(planets.size() + 1);
         planets[i]->preRender(worldView.getCamera(), windowSize, depth);
      }
   }

   [[nodiscard]] const std::vector<std::unique_ptr<Planet>>& getPlanets() const { return planets; }

   void terminate() {
      threadPool.shutdown();
      planets.clear();
      atlasTexture.destroy();
   }

private:
   void initializeGameContent() {
      registry.registerTile(TileID::Grass, 0, 0, 4);
      registry.registerTile(TileID::Water, 1, 0, 4);
      registry.registerTile(TileID::ColdGrass, 2, 0, 4);
      registry.registerTile(TileID::Stone, 3, 0, 4, 0.4f);
      registry.registerTile(TileID::HardStone, 4, 0, 4, 0.4f);
      registry.registerTile(TileID::Sand, 5, 0, 4);
      registry.registerTile(TileID::ColdWater, 6, 0, 4);
      registry.registerTile(TileID::Ice, 7, 0, 4);
      registry.registerTile(TileID::Snow, 8, 0, 4, 0.5f);
      registry.registerTile(TileID::RedOre, 9, 0, 1);
      registry.registerTile(TileID::BlueOre, 10, 0, 1);
      registry.registerTile(TileID::BurntGround, 11, 0, 1);
      registry.registerTile(TileID::Gravel, 12, 0, 1, 0.7f);
      registry.registerTile(TileID::HardGravel, 13, 0, 1);
      registry.registerTile(TileID::Planks, 14, 0, 1, 0.0f);
      registry.registerTile(TileID::PlankFloor, 15, 0, 1);
   }

   ResourceManager resourceManager;
   GpuTexture atlasTexture;

   std::vector<std::unique_ptr<Planet>> planets;

   GameGraphics* graphicsCtx = nullptr;
   wgpu::Queue queue = nullptr;

   Threadpool threadPool;

   TileRegistry registry;
   WorldView worldView;
};
