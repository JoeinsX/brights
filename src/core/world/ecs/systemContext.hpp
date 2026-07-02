#pragma once

#include "core/graphics/camera.hpp"
#include "core/world/heightField.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <optional>

struct SystemContext {
   entt::registry& registry;
   Camera& camera;
   const HeightField& heightField;
   float dtSeconds{};
   glm::vec2 controlAxis{};
   std::optional<glm::vec2> cursorWorld{};
   glm::ivec2 globalChunkMove{};
   bool spritesDirty = false;
};
