#pragma once

#include "core/world/ecs/components.hpp"
#include "core/world/graphics/spriteInstance.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>

struct SpriteGather {
   static void collect(const entt::registry& registry, std::vector<SpriteInstance>& out) {
      out.clear();
      append(registry.view<const ecs::Transform, const ecs::Sprite>(entt::exclude<ecs::Parent>), out);
      append(registry.view<const ecs::Transform, const ecs::Sprite, const ecs::Parent>(), out);
   }

private:
   template<typename View>
   static void append(const View& view, std::vector<SpriteInstance>& out) {
      for (const entt::entity entity : view) {
         const ecs::Transform& transform = view.template get<const ecs::Transform>(entity);
         const ecs::Sprite& sprite = view.template get<const ecs::Sprite>(entity);
         out.push_back({.position = glm::vec3(transform.position, transform.height),
                        .rotation = transform.rotation + sprite.rotationOffset,
                        .spriteDimensions = sprite.dimensions,
                        .spriteId = sprite.spriteId,
                        .pivotY = sprite.pivotY});
      }
   }
};
