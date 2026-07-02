#pragma once

#include "core/world/ecs/commonMath.hpp"
#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct MovementSystem: System {
   void run(SystemContext& ctx) override {
      const auto view = ctx.registry.view<ecs::Transform, ecs::Motion>();
      for (const entt::entity entity : view) {
         auto [transform, motion] = view.get(entity);
         if (motion.velocity == glm::vec2(0.0f)) {
            continue;
         }
         transform.position += motion.velocity * ctx.dtSeconds;
         transform.rotation = facingAngle(motion.velocity);
         ctx.spritesDirty = true;
      }
   }
};
