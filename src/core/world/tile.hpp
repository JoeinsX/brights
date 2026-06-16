#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

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

struct TileDefinition {
   glm::ivec2 atlasBase{};
   int variationCount = 1;
   float softness = 0.0f;
};

class TileRegistry {
public:
   void registerTile(const TileID id, const int x, const int y, const int variations, const float softness = 0.5f) {
      defs[static_cast<size_t>(id)] = {{x, y}, variations, softness};
   }

   [[nodiscard]] const TileDefinition& get(const TileID id) const { return defs[static_cast<size_t>(id)]; }

private:
   std::array<TileDefinition, 256> defs{};
};
