#pragma once

#include "util/bitmask.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

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
   float softness = 0.0f;
   std::string_view name{};
};

class TileRegistry {
public:
   void registerTile(const TileID id, const std::string_view name, const int x, const int y, const int variations, const float softness = 0.5f) {
      defs[static_cast<size_t>(id)] = {{x, y}, variations, softness, name};
      order.push_back(id);
   }

   [[nodiscard]] const TileDefinition& get(const TileID id) const { return defs[static_cast<size_t>(id)]; }

   [[nodiscard]] const std::vector<TileID>& list() const { return order; }

private:
   std::array<TileDefinition, 256> defs{};
   std::vector<TileID> order;
};
