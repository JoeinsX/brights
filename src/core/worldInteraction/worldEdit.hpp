#pragma once

#include "core/world/chunk.hpp"
#include "core/world/contents/tile.hpp"
#include "core/world/worldArea.hpp"

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_set>

enum class EditTool : uint8_t {
   TileBrush,
   HeightBrush,
   Trimmer
};

enum class TrimMode : uint8_t {
   Lower,
   Lift
};

struct WorldEditBrush {
   EditTool tool = EditTool::TileBrush;
   int radius = 3;
   float paintHeight = 1.0f;
   float heightRate = 1.0f;
   TileID tile = TileID::Grass;
   TrimMode trimMode = TrimMode::Lower;
   bool active = false;

   int apply(WorldArea& area, const glm::ivec2 centerTile, const float dtSeconds) const {
      float trimHeight = 0.0f;
      if (tool == EditTool::Trimmer) {
         const auto sampled = area.sampleHeight(centerTile);
         if (!sampled) {
            return 0;
         }
         trimHeight = *sampled;
      }
      const float heightStep = heightRate * dtSeconds;

      std::unordered_set<glm::ivec2> dirty;
      int painted = 0;
      for (int dy = -radius; dy <= radius; ++dy) {
         for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
               continue;
            }
            const glm::ivec2 worldTile = centerTile + glm::ivec2{dx, dy};
            if (editTile(area, worldTile, trimHeight, heightStep)) {
               dirty.insert(toChunkCoord(worldTile));
               ++painted;
            }
         }
      }

      area.remeshDirty(dirty);
      return painted;
   }

private:
   bool editTile(WorldArea& area, const glm::ivec2 worldTile, const float trimHeight, const float heightStep) const {
      const glm::ivec2 chunkPos = toChunkCoord(worldTile);
      const Chunk* chunk = area.chunkAt(chunkPos);
      if (!chunk) {
         return false;
      }
      const glm::ivec2 local = worldTile - chunkPos * Chunk::SIZE;
      const TileID id = chunk->terrainAt(local.x, local.y);
      const float current = chunk->heightAt(local.x, local.y);

      switch (tool) {
      case EditTool::TileBrush:   area.setTerrain(worldTile, tile, paintHeight); break;
      case EditTool::HeightBrush: area.setTerrain(worldTile, id, std::clamp(current + heightStep, 0.0f, maxTerrainHeight)); break;
      case EditTool::Trimmer:     {
         const float trimmed = trimMode == TrimMode::Lift ? std::max(current, trimHeight) : std::min(current, trimHeight);
         area.setTerrain(worldTile, id, trimmed);
         break;
      }
      }
      return true;
   }
};

struct EditStatus {
   bool locked = false;
   bool hit = false;
   glm::ivec2 tile{};
   int planet = -1;
   int lastPainted = 0;
};
