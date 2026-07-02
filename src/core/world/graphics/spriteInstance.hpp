#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

struct SpriteInstance {
   glm::vec3 position{};
   float rotation{};
   glm::vec2 spriteDimensions{};
   uint32_t spriteId{};
   float pivotY{};
};

static_assert(sizeof(SpriteInstance) == 32);
static_assert(offsetof(SpriteInstance, rotation) == 12);
static_assert(offsetof(SpriteInstance, spriteDimensions) == 16);
static_assert(offsetof(SpriteInstance, spriteId) == 24);
static_assert(offsetof(SpriteInstance, pivotY) == 28);

inline constexpr uint32_t dynamicSpriteCapacity = 8192;
inline constexpr uint32_t staticSpriteCapacity = 65536;
inline constexpr uint32_t totalSpriteCapacity = staticSpriteCapacity + dynamicSpriteCapacity;
