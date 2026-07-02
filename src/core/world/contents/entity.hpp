#pragma once

#include "util/registry.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <string_view>

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

struct EntitySpawn {
   EntityKind kind{};
   glm::vec3 position{};
};
