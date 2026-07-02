#pragma once

#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <entt/entt.hpp>

struct PlayerInputSystem: System {
   void run(SystemContext& ctx) override {
      const auto view = ctx.registry.view<ecs::PlayerInput, ecs::Motion>();
      for (const entt::entity entity : view) {
         auto [input, motion] = view.get(entity);
         motion.velocity = ctx.controlAxis * input.moveSpeed;
      }
   }
};
