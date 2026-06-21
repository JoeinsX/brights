#pragma once

#include "core/graphics/gameGraphics.hpp"
#include "core/graphics/renderSettings.hpp"
#include "core/resources/resource.hpp"
#include "core/world/tileInspection.hpp"
#include "core/world/world.hpp"
#include "core/world/worldEdit.hpp"
#include "core/world/worldGenerator.hpp"
#include "core/world/worldRenderAdapter.hpp"
#include "core/world/worldView.hpp"
#include "input/input.hpp"
#include "render/gpuTexture.hpp"
#include "util/logger.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>
#include <webgpu/webgpu.hpp>

class Game {
public:
   Game(): threadPool(std::clamp(std::thread::hardware_concurrency() - 1, 1u, 4u)) {}

   ~Game() {
      threadPool.shutdown();
      planets.clear();
   }

   [[nodiscard]] bool initialize(GameGraphics* _graphicsCtx, GpuContext& gpuContext, wgpu::Queue _queue) {
      graphicsCtx = _graphicsCtx;
      queue = _queue;

      initializeGameContent();

      wgpu::Device device = gpuContext.getDevice();
      if (!atlasTexture.load(device, queue, *resourceManager.loadImage("atlas", "assets/atlas.png"))) {
         Logger::error("failed to load texture atlas 'assets/atlas.png'");
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

   void update(float dt, const Input& input, glm::ivec2 windowSize, const RenderSettings& settings, const BrushSettings& brush) {
      worldView.handleInput(input, planets, windowSize, brush.active);

      const int focusedIndex = worldView.getFocusedPlanetIndex();
      for (size_t i = 0; i < planets.size(); ++i) {
         planets[i]->update(dt, std::cmp_equal(i, focusedIndex));
      }

      worldView.update(dt, planets);

      updateEditing(input, windowSize, brush, dt);

      for (size_t i = 0; i < planets.size(); ++i) {
         const float depth = static_cast<float>(i + 1) / static_cast<float>(planets.size() + 1);
         planets[i]->preRender(worldView.getCamera(), windowSize, depth, settings);
      }

      tileInspection = inspectUnderCursor(input.getMousePosition(), windowSize);
   }

   [[nodiscard]] const std::vector<std::unique_ptr<Planet>>& getPlanets() const { return planets; }
   [[nodiscard]] const TileRegistry& getRegistry() const { return registry; }
   [[nodiscard]] WGPUTextureView getAtlasView() const { return atlasTexture.getRawView(); }
   [[nodiscard]] const EditStatus& getEditStatus() const { return editStatus; }
   [[nodiscard]] const std::optional<TileInspection>& getTileInspection() const { return tileInspection; }

private:
   [[nodiscard]] std::optional<TileInspection> inspectUnderCursor(const glm::vec2 screenPos, const glm::ivec2 windowSize) const {
      for (size_t i = 0; i < planets.size(); ++i) {
         const auto tile = planets[i]->pickTile(screenPos, worldView.getCamera(), windowSize);
         if (!tile) {
            continue;
         }
         TileInspection info = planets[i]->inspectTile(*tile);
         info.planet = static_cast<int>(i);
         return info;
      }
      return std::nullopt;
   }

   void updateEditing(const Input& input, glm::ivec2 windowSize, const BrushSettings& brush, float dt) {
      editStatus = {};
      if (!brush.active || !worldView.isLocked()) {
         return;
      }
      const int idx = worldView.getFocusedPlanetIndex();
      if (idx < 0) {
         return;
      }
      Planet& planet = *planets[static_cast<size_t>(idx)];
      editStatus.locked = true;
      editStatus.planet = idx;

      const auto tile = planet.pickTile(input.getMousePosition(), worldView.getCamera(), windowSize);
      if (!tile) {
         return;
      }
      editStatus.hit = true;
      editStatus.tile = *tile;

      if (input.isDragging()) {
         editStatus.lastPainted = planet.applyBrush(brush, *tile, dt);
      }
   }

   void initializeGameContent() {
      registry.registerTile(TileID::Grass, "Grass", 0, 0, 4);
      registry.registerTile(TileID::Water, "Water", 1, 0, 4);
      registry.registerTile(TileID::ColdGrass, "Cold Grass", 2, 0, 4);
      registry.registerTile(TileID::Stone, "Stone", 3, 0, 4, 0.4f);
      registry.registerTile(TileID::HardStone, "Hard Stone", 4, 0, 4, 0.4f);
      registry.registerTile(TileID::Sand, "Sand", 5, 0, 4);
      registry.registerTile(TileID::ColdWater, "Cold Water", 6, 0, 4);
      registry.registerTile(TileID::Ice, "Ice", 7, 0, 4);
      registry.registerTile(TileID::Snow, "Snow", 8, 0, 4, 0.5f);
      registry.registerTile(TileID::RedOre, "Red Ore", 9, 0, 1);
      registry.registerTile(TileID::BlueOre, "Blue Ore", 10, 0, 1);
      registry.registerTile(TileID::BurntGround, "Burnt Ground", 11, 0, 1);
      registry.registerTile(TileID::Gravel, "Gravel", 12, 0, 1, 0.7f);
      registry.registerTile(TileID::HardGravel, "Hard Gravel", 13, 0, 1);
      registry.registerTile(TileID::Planks, "Planks", 14, 0, 1, 0.0f);
      registry.registerTile(TileID::PlankFloor, "Plank Floor", 15, 0, 1);
   }

   ResourceManager resourceManager;
   GpuTexture atlasTexture;

   std::vector<std::unique_ptr<Planet>> planets;

   GameGraphics* graphicsCtx = nullptr;
   wgpu::Queue queue = nullptr;

   Threadpool threadPool;

   TileRegistry registry;
   WorldView worldView;
   EditStatus editStatus;
   std::optional<TileInspection> tileInspection;
};
