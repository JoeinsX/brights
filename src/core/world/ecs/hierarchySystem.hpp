#pragma once

#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct HierarchySystem: System {
   void run(SystemContext& ctx) override {
      const auto view = ctx.registry.view<ecs::Transform, ecs::Parent>();
      for (const entt::entity entity : view) {
         auto [transform, parent] = view.get(entity);
         const ecs::Transform* parentTransform = ctx.registry.try_get<ecs::Transform>(parent.entity);
         if (!parentTransform) {
            continue;
         }
         const glm::vec2 position = parentTransform->position + parent.offset;
         if (position != transform.position || parentTransform->height != transform.height) {
            transform.position = position;
            transform.height = parentTransform->height;
            ctx.spritesDirty = true;
         }
      }
   }
};
