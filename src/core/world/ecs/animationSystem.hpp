#pragma once

#include "core/world/contents/atlasCell.hpp"
#include "core/world/ecs/components.hpp"
#include "core/world/ecs/system.hpp"
#include "core/world/ecs/systemContext.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct AnimationSystem: System {
   void run(SystemContext& ctx) override {
      const auto view = ctx.registry.view<ecs::AnimationClip, ecs::Sprite>();
      for (const entt::entity entity : view) {
         auto [clip, sprite] = view.get(entity);
         if (clip.frameCount <= 1 || clip.framesPerSecond <= 0.0f) {
            continue;
         }

         const ecs::Motion* motion = ctx.registry.try_get<ecs::Motion>(entity);
         if (motion && motion->velocity == glm::vec2(0.0f)) {
            clip.elapsed = 0.0f;
            if (clip.frame != 0) {
               clip.frame = 0;
               sprite.spriteId = packAtlasCell(clip.firstCell);
               ctx.spritesDirty = true;
            }
            continue;
         }

         clip.elapsed += ctx.dtSeconds;
         const float frameDuration = 1.0f / clip.framesPerSecond;
         const int previous = clip.frame;
         while (clip.elapsed >= frameDuration) {
            clip.elapsed -= frameDuration;
            clip.frame = (clip.frame + 1) % clip.frameCount;
         }
         if (clip.frame != previous) {
            sprite.spriteId = packAtlasCell({clip.firstCell.x + clip.frame, clip.firstCell.y});
            ctx.spritesDirty = true;
         }
      }
   }
};
