#pragma once

#include "core/world/ecs/commonMath.hpp"
#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct CursorFacingSystem: System {
   void run(SystemContext& ctx) override {
      if (!ctx.cursorWorld) {
         return;
      }
      const glm::vec2 cursor = *ctx.cursorWorld;
      const auto view = ctx.registry.view<ecs::Transform, const ecs::Sprite, ecs::CursorFacing>();
      for (const entt::entity entity : view) {
         auto [transform, sprite] = view.get<ecs::Transform, const ecs::Sprite>(entity);
         const glm::vec2 center = transform.position - glm::vec2(0.0f, sprite.pivotY * sprite.dimensions.y);
         const glm::vec2 aim = cursor - center;
         if (glm::dot(aim, aim) < 1e-6f) {
            continue;
         }
         if (const float rotation = facingAngle(aim); rotation != transform.rotation) {
            transform.rotation = rotation;
            ctx.spritesDirty = true;
         }
      }
   }
};
