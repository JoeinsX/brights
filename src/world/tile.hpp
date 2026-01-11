#pragma once

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
   glm::ivec2 atlasBase;
   int variationCount = 1;
   float height = 0.5f;
   float softness = 0.5f;
};

class TileRegistry {
public:
   void registerTile (const TileID id, const int x, const int y, const int variations, float height = 0.5f, float softness = 0.5f) {
      defs[id] = {
         {x, y},
         variations, height, softness
      };
   }

   [[nodiscard]] const TileDefinition* get (const TileID id) const {
      const auto it = defs.find (id);
      if (it != defs.end ()) {
         return &it->second;
      }
      return nullptr;
   }

private:
   std::unordered_map<TileID, TileDefinition> defs{};
};
