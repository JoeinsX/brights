#pragma once

#include "core/world/chunk.hpp"
#include "core/world/contents/atlasCell.hpp"
#include "core/world/contents/tile.hpp"
#include "core/world/graphics/worldRenderAdapter.hpp"
#include "core/world/worldArea.hpp"

#include <cstdint>
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

   [[nodiscard]] static TileInspection inspect(const WorldArea& area, const glm::ivec2 worldTile) {
      TileInspection info;
      info.worldTile = worldTile;
      info.chunkPos = toChunkCoord(worldTile);
      info.localTile = worldTile - info.chunkPos * Chunk::SIZE;

      const Chunk* chunk = area.chunkAt(info.chunkPos);
      if (!chunk) {
         return info;
      }
      const int idx = info.localTile.y * Chunk::SIZE + info.localTile.x;

      info.loaded = true;
      info.id = chunk->terrainAt(info.localTile.x, info.localTile.y);
      info.height = chunk->heightAt(info.localTile.x, info.localTile.y);
      info.softness = area.tiles().get(info.id).softness;
      info.meshed = chunk->isMeshed();

      if (info.meshed) {
         info.atlasCoords = unpackAtlasCell(area.render().getDisplayAt(info.chunkPos, idx));
         info.flags = static_cast<RenderFlag>(area.render().getPackedAt(info.chunkPos, idx) & 0x0F);
      } else {
         info.atlasCoords = area.tiles().get(info.id).atlasBase;
      }
      return info;
   }
};
