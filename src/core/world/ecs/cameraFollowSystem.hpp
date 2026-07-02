#pragma once

#include "core/world/chunk.hpp"
#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <cmath>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct CameraFollowSystem: System {
   void run(SystemContext& ctx) override {
      const auto view = ctx.registry.view<const ecs::Transform, const ecs::CameraTarget>();
      for (const entt::entity entity : view) {
         const ecs::Transform& transform = view.get<const ecs::Transform>(entity);
         const glm::vec2 target = transform.position - glm::vec2(ctx.globalChunkMove * Chunk::SIZE);
         const float t = 1.0f - std::exp(-lerpSpeed * ctx.dtSeconds);
         ctx.camera.setOffset(glm::mix(ctx.camera.getOffset(), target, t));
      }
   }

private:
   static constexpr float lerpSpeed = 8.0f;
};
