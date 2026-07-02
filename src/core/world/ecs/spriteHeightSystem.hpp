#pragma once

#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <cmath>
#include <entt/entt.hpp>

struct SpriteHeightSystem: System {
   void run(SystemContext& ctx) override {
      const auto view = ctx.registry.view<ecs::Transform>(entt::exclude<ecs::Parent>);
      for (const entt::entity entity : view) {
         ecs::Transform& transform = view.get<ecs::Transform>(entity);
         if (const auto sampled = ctx.heightField.sampleAt(transform.position); sampled && std::abs(*sampled - transform.height) > 1e-5f) {
            transform.height = *sampled;
            ctx.spritesDirty = true;
         }
      }
   }
};
