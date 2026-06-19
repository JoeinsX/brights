#pragma once

#include "tile.hpp"

#include <glm/glm.hpp>

struct TileInspection {
   int planet = -1;
   glm::ivec2 worldTile{};
   glm::ivec2 chunkPos{};
   glm::ivec2 localTile{};

   bool loaded = false;
   bool meshed = false;
   TileID id = TileID::Air;
   float height = 0.0f;
   float softness = 0.0f;
   RenderFlag flags = RenderFlag::None;
   glm::ivec2 atlasCoords{};
};
