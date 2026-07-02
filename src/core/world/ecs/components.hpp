#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace ecs {
   struct Transform {
      glm::vec2 position{};
      float height{};
      float rotation{};
   };

   struct Motion {
      glm::vec2 velocity{};
   };

   struct Sprite {
      glm::vec2 dimensions{1.0f};
      uint32_t spriteId{};
      float pivotY{0.0f};
      float rotationOffset{0.0f};
   };

   struct PlayerInput {
      float moveSpeed{20.0f};
   };

   struct CameraTarget {};

   struct AnimationClip {
      glm::ivec2 firstCell{};
      int frameCount{1};
      float framesPerSecond{8.0f};
      float elapsed{};
      int frame{};
   };

   struct Parent {
      entt::entity entity{entt::null};
      glm::vec2 offset{};
   };

   struct CursorFacing {};
}   // namespace ecs
