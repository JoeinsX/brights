#pragma once

#include "glm/glm.hpp"
#include "util/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

struct Entity {
   glm::vec3 position{};
   float rotation{};
   glm::vec2 spriteDimensions{};
   uint32_t spriteId{};
};

static_assert(sizeof(Entity) == 28);
static_assert(offsetof(Entity, rotation) == 12);
static_assert(offsetof(Entity, spriteDimensions) == 16);
static_assert(offsetof(Entity, spriteId) == 24);

inline constexpr uint32_t spriteCapacity = 8192;

inline uint32_t encodeSpriteCell(const glm::ivec2 cell) {
   return static_cast<uint32_t>(cell.x << 4 | cell.y);
}

enum class EntityKind : uint8_t {
   Player = 0,
   Tree
};

struct EntityDefinition {
   glm::ivec2 spriteCell{};
   glm::vec2 dimensions{1.0f};
   std::string_view name{};
};

using EntityRegistry = Registry<EntityKind, EntityDefinition>;
