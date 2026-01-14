#pragma once

#include "chunk.hpp"
#include "util/bitmask.hpp"
#include "worldRenderAdapter.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <vector>

enum class AnalysisFlag : uint8_t {
   None = 0,
   HasLowerNeighbor = 1 << 0,
   HasVariance = 1 << 1
};

enum class RenderFlag : uint8_t {
   None = 0,
   Triplanar = 1 << 0,
   Blending = 1 << 1,
   AdvancedRaymarching = 1 << 2,
   SkipRaymarching = 1 << 3
};

ENABLE_BITMASK_OPERATORS(AnalysisFlag)
ENABLE_BITMASK_OPERATORS(RenderFlag)

class ChunkMesher {
private:
   static constexpr float EPSILON = 0.0001f;
   static constexpr int CHUNK_SIZE = Chunk::SIZE;
   static constexpr int PADDED_SIZE = CHUNK_SIZE + 2;

   struct CachedTile {
      float height{};
      float softness{};
      TileID id = TileID::Air;

      AnalysisFlag aFlags = AnalysisFlag::None;
      RenderFlag rFlags = RenderFlag::None;

      void set(AnalysisFlag f) { aFlags |= f; }
      void set(RenderFlag f) { rFlags |= f; }

      [[nodiscard]] bool has(AnalysisFlag f) const { return static_cast<bool>(aFlags & f); }
      [[nodiscard]] bool has(RenderFlag f) const { return static_cast<bool>(rFlags & f); }
   };

   struct TilePacker {
      static uint16_t pack(float height, float softness, RenderFlag flags) {
         // Layout: HHHHHHHH SSSS FFFF
         const uint16_t h = static_cast<uint16_t>(std::clamp(height * 127.5f, 0.0f, 255.0f));
         const uint16_t s = static_cast<uint16_t>(std::clamp(softness * 15.0f, 0.0f, 15.0f));
         const uint16_t f = static_cast<uint8_t>(flags) & 0x0F;

         return (h << 8) | (s << 4) | f;
      }
   };

   struct MeshContext {
      std::array<CachedTile, PADDED_SIZE * PADDED_SIZE> buffer;

      MeshContext() = default;

      void build(const Chunk& centerChunk, const std::array<std::shared_ptr<Chunk>, 8>& neighbors, const TileRegistry& registry) {
         for (int y = 0; y < CHUNK_SIZE; ++y) {
            const int centerRowOffset = y * CHUNK_SIZE;
            const int bufferRowOffset = (y + 1) * PADDED_SIZE + 1;

            for (int x = 0; x < CHUNK_SIZE; ++x) {
               const int idx = centerRowOffset + x;
               const TileID tID = centerChunk.terrainMap[idx];
               const TileDefinition* def = registry.get(tID);

               buffer[bufferRowOffset + x] = {centerChunk.heightMap[idx], def ? def->softness : 0.0f, tID};
            }
         }

         auto copyTile = [&](int nIndex, int srcX, int srcY, int destX, int destY) {
            if (const auto& chunk = neighbors[nIndex]) {
               const int idx = srcY * CHUNK_SIZE + srcX;
               const TileID id = chunk->terrainMap[idx];
               const TileDefinition* def = registry.get(id);

               buffer[destY * PADDED_SIZE + destX] = {def ? def->height : 0.0f, def ? def->softness : 0.0f, id};
            } else {
               buffer[destY * PADDED_SIZE + destX] = {0.0f, 0.0f, TileID::Air};
            }
         };

         for (int i = 0; i < CHUNK_SIZE; ++i) {
            copyTile(1, i, CHUNK_SIZE - 1, i + 1, 0);         // North
            copyTile(6, i, 0, i + 1, CHUNK_SIZE + 1);         // South
            copyTile(3, CHUNK_SIZE - 1, i, 0, i + 1);         // West
            copyTile(4, 0, i, CHUNK_SIZE + 1, i + 1);         // East
         }

         copyTile(0, CHUNK_SIZE - 1, CHUNK_SIZE - 1, 0, 0);   // NW
         copyTile(2, 0, CHUNK_SIZE - 1, CHUNK_SIZE + 1, 0);   // NE
         copyTile(5, CHUNK_SIZE - 1, 0, 0, CHUNK_SIZE + 1);   // SW
         copyTile(7, 0, 0, CHUNK_SIZE + 1, CHUNK_SIZE + 1);   // SE

         analyzeTopology();
      }

      [[nodiscard]] const CachedTile& get(int x, int y) const { return buffer[(y + 1) * PADDED_SIZE + (x + 1)]; }

   private:
      void analyzeTopology();
   };

public:
   static void meshChunk(Chunk& chunk, const TileRegistry& registry, std::mt19937& rng, const std::array<std::shared_ptr<Chunk>, 8>& neighbors, WorldRenderAdapter& renderAdapter) {
      uint8_t* displayMapData = renderAdapter.getDisplayDataPtrForChunk(chunk.getPos());
      uint16_t* packedMapData = renderAdapter.getPackedDataPtrForChunk(chunk.getPos());

      for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE; ++i) {
         const TileID tID = chunk.terrainMap[i];
         const TileDefinition* def = registry.get(tID);

         chunk.heightMap[i] = def ? def->height : 0.0f;

         glm::vec2 terrainCoords = getAtlasCoords(def, rng);
         const uint8_t tx = static_cast<uint8_t>(std::clamp(terrainCoords.x, 0.0f, 15.0f));
         const uint8_t ty = static_cast<uint8_t>(std::clamp(terrainCoords.y, 0.0f, 15.0f));

         displayMapData[i] = (tx << 4) | (ty & 0x0F);
      }

      MeshContext ctx;
      ctx.build(chunk, neighbors, registry);

      for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE; ++i) {
         int x = i % CHUNK_SIZE;
         int y = i / CHUNK_SIZE;

         const auto& tile = ctx.get(x, y);

         chunk.packedMap[i] = TilePacker::pack(tile.height, tile.softness, tile.rFlags);
         packedMapData[i] = chunk.packedMap[i];
      }
   }

private:
   static glm::vec2 getAtlasCoords(const TileDefinition* def, std::mt19937& rng) {
      if (!def) {
         return {-1.0f, -1.0f};
      }

      auto atlasBase = def->atlasBase;
      if (def->variationCount > 1) {
         std::uniform_int_distribution<int> dist(0, def->variationCount - 1);
         atlasBase.y += static_cast<float>(dist(rng));
      }
      return static_cast<glm::vec2>(atlasBase);
   }
};
