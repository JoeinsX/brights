#pragma once

#include "core/graphics/gameGraphics.hpp"
#include "core/world/world.hpp"
#include "core/world/worldGenerator.hpp"
#include "core/world/worldRenderAdapter.hpp"
#include "core/world/worldView.hpp"
#include "platform/input.hpp"

#include <cmath>
#include <webgpu/webgpu.hpp>

class Game {
public:
   Game(): threadPool(std::clamp(std::thread::hardware_concurrency() - 1, 1u, 4u)) {
      static constexpr uint32_t galaxySeed = 0;
      rng = std::mt19937{galaxySeed};
   }

   void initialize(GameGraphics* _graphicsCtx, GpuContext& gpuContext, wgpu::Queue _queue) {
      graphicsCtx = _graphicsCtx;
      queue = _queue;

      initializeGameContent();

      const wgpu::Device device = gpuContext.getDevice();

      auto p1 = std::
         make_unique<Planet>(PlanetConfig{.position = {-1200.0f, 0.0f}, .seed = 42, .baseSize = 512.0f, .idleScrollSpeed = {100.0f, 50.0f}, .orbitParams = {1000.0f, 0.2f}},
                             registry);
      p1->initialize(device, queue, graphicsCtx->getBindGroupLayout(), threadPool, graphicsCtx->getAtlas());
      planets.push_back(std::move(p1));

      auto p2 = std::make_unique<Planet>(PlanetConfig{.position = {0.0f, 0.0f}, .seed = 1337, .baseSize = 1024.0f, .idleScrollSpeed = {-28.0f, 0.0f}}, registry);
      p2->initialize(device, queue, graphicsCtx->getBindGroupLayout(), threadPool, graphicsCtx->getAtlas());
      planets.push_back(std::move(p2));

      auto p3 = std::
         make_unique<Planet>(PlanetConfig{.position = {1200.0f, 0.0f}, .seed = 2550, .baseSize = 300.0f, .idleScrollSpeed = {/*13.33f, -24.13f*/}, .orbitParams = {1500.0f, -0.4f}},
                             registry);
      p3->initialize(device, queue, graphicsCtx->getBindGroupLayout(), threadPool, graphicsCtx->getAtlas());
      planets.push_back(std::move(p3));
   }

   void update(float dt, const Input& input, glm::ivec2 windowSize) {
      worldView.handleInput(input, planets, windowSize);

      for (auto& planet : planets) {
         planet->update(dt);
      }

      worldView.update(dt, planets, windowSize);

      for (auto& planet : planets) {
         planet->preRender(worldView.getCamera(), windowSize);
      }
   }

   [[nodiscard]] const std::vector<std::unique_ptr<Planet>>& getPlanets() const { return planets; }

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

   std::vector<std::unique_ptr<Planet>> planets;

   GameGraphics* graphicsCtx = nullptr;
   wgpu::Queue queue = nullptr;

   ThreadPool threadPool;

   TileRegistry registry;
   std::mt19937 rng;

   WorldView worldView;
};
