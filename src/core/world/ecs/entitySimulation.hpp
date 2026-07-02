#pragma once

#include "core/graphics/camera.hpp"
#include "core/world/chunk.hpp"
#include "core/world/ecs/animationSystem.hpp"
#include "core/world/ecs/cameraFollowSystem.hpp"
#include "core/world/ecs/components.hpp"
#include "core/world/ecs/cursorFacingSystem.hpp"
#include "core/world/ecs/hierarchySystem.hpp"
#include "core/world/ecs/movementSystem.hpp"
#include "core/world/ecs/playerInputSystem.hpp"
#include "core/world/ecs/spriteGather.hpp"
#include "core/world/ecs/spriteHeightSystem.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"
#include "core/world/graphics/spriteInstance.hpp"
#include "core/world/heightField.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

class EntitySimulation {
public:
   EntitySimulation() {
      entities.on_construct<ecs::Sprite>().connect<&EntitySimulation::markDirty>(*this);
      entities.on_destroy<ecs::Sprite>().connect<&EntitySimulation::markDirty>(*this);

      systems.push_back(std::make_unique<PlayerInputSystem>());
      systems.push_back(std::make_unique<MovementSystem>());
      systems.push_back(std::make_unique<SpriteHeightSystem>());
      systems.push_back(std::make_unique<HierarchySystem>());
      systems.push_back(std::make_unique<CursorFacingSystem>());
      systems.push_back(std::make_unique<AnimationSystem>());
      systems.push_back(std::make_unique<CameraFollowSystem>());
   }

   void update(Camera& camera, const HeightField& heightField, const float dtSeconds, const glm::vec2 controlAxis, const std::optional<glm::vec2> cursorWorld,
               const glm::ivec2 globalChunkMove) {
      SystemContext ctx{.registry = entities,
                        .camera = camera,
                        .heightField = heightField,
                        .dtSeconds = dtSeconds,
                        .controlAxis = controlAxis,
                        .cursorWorld = cursorWorld,
                        .globalChunkMove = globalChunkMove};
      for (const std::unique_ptr<System>& system : systems) {
         system->run(ctx);
      }
      dirty |= ctx.spritesDirty;
   }

   bool collectSprites(std::vector<SpriteInstance>& out) {
      if (!dirty) {
         return false;
      }
      SpriteGather::collect(entities, out);
      dirty = false;
      return true;
   }

   [[nodiscard]] entt::registry& registry() { return entities; }

   void togglePossession(const Camera& camera, const glm::ivec2 chunkMove) {
      if (isPossessing()) {
         std::vector<entt::entity> controlled;
         for (const entt::entity entity : entities.view<ecs::PlayerInput>()) {
            controlled.push_back(entity);
         }
         for (const entt::entity entity : controlled) {
            if (ecs::Motion* motion = entities.try_get<ecs::Motion>(entity)) {
               motion->velocity = glm::vec2(0.0f);
            }
            entities.remove<ecs::PlayerInput>(entity);
            entities.remove<ecs::CameraTarget>(entity);
         }
         return;
      }

      const glm::vec2 center = camera.getOffset() + glm::vec2(chunkMove * Chunk::SIZE);
      entt::entity best = entt::null;
      float bestDistance = std::numeric_limits<float>::max();
      for (const entt::entity entity : entities.view<ecs::Transform, ecs::Motion>()) {
         const glm::vec2 delta = entities.get<ecs::Transform>(entity).position - center;
         if (const float distance = glm::dot(delta, delta); distance < bestDistance) {
            bestDistance = distance;
            best = entity;
         }
      }
      if (best != entt::null) {
         entities.emplace<ecs::PlayerInput>(best);
         entities.emplace<ecs::CameraTarget>(best);
      }
   }

   [[nodiscard]] bool isPossessing() const { return entities.view<const ecs::PlayerInput>().size() != 0; }

private:
   void markDirty(entt::registry&, entt::entity) { dirty = true; }

   entt::registry entities;
   std::vector<std::unique_ptr<System>> systems;
   bool dirty = true;
};
