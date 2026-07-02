#pragma once

#include "app/input/input.hpp"
#include "app/worldView.hpp"
#include "core/graphics/gameGraphics.hpp"
#include "core/graphics/renderSettings.hpp"
#include "core/resources/resource.hpp"
#include "core/world/contents/atlasCell.hpp"
#include "core/world/contents/entity.hpp"
#include "core/world/ecs/components.hpp"
#include "core/world/planet.hpp"
#include "core/world/worldArea.hpp"
#include "core/worldInteraction/tileInspection.hpp"
#include "core/worldInteraction/worldEdit.hpp"
#include "render/gpuTexture.hpp"
#include "util/logger.hpp"

#include <algorithm>
#include <entt/entt.hpp>
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

   [[nodiscard]] bool initialize(GameGraphics* graphics, GpuContext& gpuContext, wgpu::Queue gpuQueue) {
      graphicsCtx = graphics;
      queue = gpuQueue;

      initializeGameContent();

      wgpu::Device device = gpuContext.getDevice();
      if (!atlasTexture.load(device, queue, *resourceManager.loadImage("atlas", "assets/textures/atlas.png"))) {
         Logger::error("failed to load texture atlas 'assets/textures/atlas.png'");
         return false;
      }

      if (!entityTexture.load(device, queue, *resourceManager.loadImage("entity", "assets/textures/entity.png"))) {
         Logger::error("failed to load entity texture 'assets/textures/entity.png'");
         return false;
      }

      const PlanetContext planetContext{.device = device,
                                        .queue = queue,
                                        .terrainLayout = graphicsCtx->getBindGroupLayout(),
                                        .spriteLayout = graphicsCtx->getSpriteBindGroupLayout(),
                                        .threadPool = threadPool,
                                        .atlas = atlasTexture,
                                        .entitySheet = entityTexture,
                                        .tileRegistry = tileRegistry,
                                        .entityRegistry = entityRegistry};

      const PlanetConfig configs[] = {
         {.position = {-1200.0f, 0.0f}, .seed = 42, .baseSize = 512.0f, .idleScrollSpeed = {60.0f, 30.0f}, .orbitParams = {1000.0f, 0.2f}},
         {.position = {0.0f, 0.0f}, .seed = 1337, .baseSize = 1024.0f, .idleScrollSpeed = {-17.0f, 0.0f}},
         {.position = {1200.0f, 0.0f}, .seed = 2550, .baseSize = 300.0f, .orbitParams = {1500.0f, -0.4f}},
      };
      for (const PlanetConfig& config : configs) {
         planets.push_back(std::make_unique<Planet>(config, planetContext));
         seedDebugActors(*planets.back());
      }

      return true;
   }

   void update(float dtSeconds, const Input& input, glm::ivec2 windowSize, const RenderSettings& settings, const WorldEditBrush& brush) {
      worldView.handleInput(input, planets, windowSize, brush.active);

      const int focusedIndex = worldView.getFocusedPlanetIndex();
      std::optional<glm::vec2> cursorWorld;
      if (focusedIndex >= 0) {
         cursorWorld = planets[static_cast<size_t>(focusedIndex)]->pickWorld(input.getMousePosition(), worldView.getCamera(), windowSize);
      }
      for (size_t i = 0; i < planets.size(); ++i) {
         const bool focused = std::cmp_equal(i, focusedIndex);
         planets[i]->update(dtSeconds, focused, focused ? worldView.getPlanetControlAxis() : glm::vec2(0.0f), focused ? cursorWorld : std::nullopt);
      }

      worldView.update(dtSeconds, planets);

      updateEditing(input, windowSize, brush, dtSeconds);

      for (size_t i = 0; i < planets.size(); ++i) {
         const float depth = static_cast<float>(i + 1) / static_cast<float>(planets.size() + 1);
         planets[i]->preRender(worldView.getCamera(), windowSize, depth, settings);
      }
      collectDrawData();

      tileInspection = inspectUnderCursor(input.getMousePosition(), windowSize);
   }

   [[nodiscard]] const std::vector<PlanetDrawData>& getDrawData() const { return drawData; }
   [[nodiscard]] const TileRegistry& getTileRegistry() const { return tileRegistry; }
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
         TileInspection info = TileInspection::inspect(planets[i]->area(), *tile);
         info.planet = static_cast<int>(i);
         return info;
      }
      return std::nullopt;
   }

   void updateEditing(const Input& input, glm::ivec2 windowSize, const WorldEditBrush& brush, float dtSeconds) {
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
         editStatus.lastPainted = brush.apply(planet.area(), *tile, dtSeconds);
      }
   }

   void collectDrawData() {
      drawData.clear();
      for (const auto& planet : planets) {
         drawData.push_back({.terrainBindGroup = planet->getBindGroup(),
                             .spriteBindGroup = planet->getSpriteBindGroup(),
                             .staticSpriteCount = planet->area().getStaticSpriteCount(),
                             .dynamicSpriteCount = planet->area().getDynamicSpriteCount()});
      }
   }

   void seedDebugActors(Planet& planet) {
      const EntityDefinition& def = entityRegistry.get(EntityKind::Player);
      entt::registry& registry = planet.area().entities().registry();
      constexpr int count = 6;
      for (int i = 0; i < count; ++i) {
         const glm::vec2 pos = glm::vec2(static_cast<float>(i % 3) * 4.0f - 4.0f, static_cast<float>(i / 3) * 4.0f - 2.0f);

         const entt::entity legs = registry.create();
         registry.emplace<ecs::Transform>(legs, pos, 0.0f, 0.0f);
         registry.emplace<ecs::Sprite>(legs, def.dimensions, static_cast<uint32_t>(packAtlasCell({0, 0})), 0.5f);
         registry.emplace<ecs::Motion>(legs);
         registry.emplace<ecs::AnimationClip>(legs, glm::ivec2{0, 0}, 3, 8.0f);

         const entt::entity head = registry.create();
         registry.emplace<ecs::Transform>(head, pos, 0.0f, 0.0f);
         registry.emplace<ecs::Sprite>(head, def.dimensions, static_cast<uint32_t>(packAtlasCell({0, 1})), 0.5f, glm::radians(270.0f));
         registry.emplace<ecs::Parent>(head, legs, glm::vec2{0.0f, 0.0f});
         registry.emplace<ecs::CursorFacing>(head);
      }
   }

   void initializeGameContent() {
      tileRegistry.add(TileID::Grass, {.atlasBase = {0, 0}, .variationCount = 4, .name = "Grass"});
      tileRegistry.add(TileID::Water, {.atlasBase = {1, 0}, .variationCount = 4, .name = "Water"});
      tileRegistry.add(TileID::ColdGrass, {.atlasBase = {2, 0}, .variationCount = 4, .name = "Cold Grass"});
      tileRegistry.add(TileID::Stone, {.atlasBase = {3, 0}, .variationCount = 4, .softness = 0.4f, .name = "Stone"});
      tileRegistry.add(TileID::HardStone, {.atlasBase = {4, 0}, .variationCount = 4, .softness = 0.4f, .name = "Hard Stone"});
      tileRegistry.add(TileID::Sand, {.atlasBase = {5, 0}, .variationCount = 4, .name = "Sand"});
      tileRegistry.add(TileID::ColdWater, {.atlasBase = {6, 0}, .variationCount = 4, .name = "Cold Water"});
      tileRegistry.add(TileID::Ice, {.atlasBase = {7, 0}, .variationCount = 4, .name = "Ice"});
      tileRegistry.add(TileID::Snow, {.atlasBase = {8, 0}, .variationCount = 4, .name = "Snow"});
      tileRegistry.add(TileID::RedOre, {.atlasBase = {9, 0}, .variationCount = 1, .name = "Red Ore"});
      tileRegistry.add(TileID::BlueOre, {.atlasBase = {10, 0}, .variationCount = 1, .name = "Blue Ore"});
      tileRegistry.add(TileID::BurntGround, {.atlasBase = {11, 0}, .variationCount = 1, .name = "Burnt Ground"});
      tileRegistry.add(TileID::Gravel, {.atlasBase = {12, 0}, .variationCount = 1, .softness = 0.7f, .name = "Gravel"});
      tileRegistry.add(TileID::HardGravel, {.atlasBase = {13, 0}, .variationCount = 1, .name = "Hard Gravel"});
      tileRegistry.add(TileID::Planks, {.atlasBase = {14, 0}, .variationCount = 1, .softness = 0.0f, .name = "Planks"});
      tileRegistry.add(TileID::PlankFloor, {.atlasBase = {15, 0}, .variationCount = 1, .name = "Plank Floor"});

      entityRegistry.add(EntityKind::Player, {.spriteCell = {0, 0}, .dimensions = {1.0f, 1.0f}, .name = "Player"});
      entityRegistry.add(EntityKind::Tree, {.spriteCell = {0, 3}, .dimensions = {1.0f, 1.0f}, .name = "Tree"});
   }

   ResourceManager resourceManager;
   GpuTexture atlasTexture;
   GpuTexture entityTexture;

   std::vector<std::unique_ptr<Planet>> planets;
   std::vector<PlanetDrawData> drawData;

   GameGraphics* graphicsCtx = nullptr;
   wgpu::Queue queue = nullptr;

   Threadpool threadPool;

   TileRegistry tileRegistry;
   EntityRegistry entityRegistry;
   WorldView worldView;
   EditStatus editStatus;
   std::optional<TileInspection> tileInspection;
};
