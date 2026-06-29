#pragma once

#include "util/bitmask.hpp"
#include "util/registry.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <string_view>

enum class TileID : uint8_t {
   Air = 0,
   Grass,
   Water,
   ColdGrass,
   Stone,
   HardStone,
   Gravel,
   HardGravel,
   Snow,
   Ice,
   Planks,
   PlankFloor,
   RedOre,
   BlueOre,
   ColdWater,
   BurntGround,
   Sand
};

enum class RenderFlag : uint8_t {
   None = 0,
   Triplanar = 1 << 0,
   Blending = 1 << 1,
   AdvancedRaymarching = 1 << 2,
   SkipRaymarching = 1 << 3
};

ENABLE_BITMASK_OPERATORS(RenderFlag)

struct TileDefinition {
   glm::ivec2 atlasBase{};
   int variationCount = 1;
   float softness = 0.5f;
   std::string_view name{};
};

using TileRegistry = Registry<TileID, TileDefinition>;
