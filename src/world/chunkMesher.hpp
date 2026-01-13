#pragma once

#include "chunk.hpp"
#include "worldRenderAdapter.hpp"

#include <array>
#include <glm/glm.hpp>
#include <random>
#include <vector>

class ChunkMesher {
public:
   static void meshChunk(Chunk& chunk, const TileRegistry& registry, std::mt19937& rng, const std::array<std::shared_ptr<Chunk>, 8>& neighbors, WorldRenderAdapter& renderAdapter) {
      uint8_t* displayMapData = renderAdapter.getDisplayDataPtrForChunk(chunk.getPos());
      uint16_t* packedMapData = renderAdapter.getPackedDataPtrForChunk(chunk.getPos());
      for (int y = 0; y < Chunk::SIZE; ++y) {
         for (int x = 0; x < Chunk::SIZE; ++x) {
            const int index = y * Chunk::SIZE + x;
            const TileID tID = chunk.terrainMap[index];

            auto terrainCoords = getAtlasCoords(registry, tID, rng);

            const uint8_t tx = static_cast<uint8_t>(std::clamp(terrainCoords.x, 0.0f, 15.0f));
            const uint8_t ty = static_cast<uint8_t>(std::clamp(terrainCoords.y, 0.0f, 15.0f));

            displayMapData[index] = (tx << 4) | (ty & 0x0F);

            const TileDefinition* def = registry.get(tID);
            chunk.heightMap[index] = def ? def->height : 0.0f;
         }
      }

      const int allNeighbors[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
      constexpr int edgeOffsets[4][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
      constexpr int cornerOffsets[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};

      auto getNeighborInfo = [&](int x, int y) -> std::tuple<float, float, TileID> {
         if (x >= 0 && x < Chunk::SIZE && y >= 0 && y < Chunk::SIZE) {
            const int idx = y * Chunk::SIZE + x;
            const float h = chunk.heightMap[idx];
            const TileDefinition* def = registry.get(chunk.terrainMap[idx]);
            const float s = def ? def->softness : 0.0f;
            return {h, s, chunk.terrainMap[idx]};
         }

         int dx = 0;
         int dy = 0;
         int localX = x;
         int localY = y;

         if (x < 0) {
            dx = -1;
            localX = x + Chunk::SIZE;
         } else if (x >= Chunk::SIZE) {
            dx = 1;
            localX = x - Chunk::SIZE;
         }

         if (y < 0) {
            dy = -1;
            localY = y + Chunk::SIZE;
         } else if (y >= Chunk::SIZE) {
            dy = 1;
            localY = y - Chunk::SIZE;
         }

         int mapIndex = (dy + 1) * 3 + (dx + 1);
         if (mapIndex > 4) {
            mapIndex--;
         }

         if (mapIndex < 0 || mapIndex > 7) {
            return {0.0f, 0.0f, TileID::Air};
         }

         const auto& nChunk = neighbors[mapIndex];
         if (!nChunk) {
            return {0.0f, 0.0f, TileID::Air};
         }

         const int nIdx = localY * Chunk::SIZE + localX;
         const TileID nID = nChunk->terrainMap[nIdx];

         if (const TileDefinition* nDef = registry.get(nID)) {
            return {nDef->height, nDef->softness, nID};
         }
         return {0.0f, 0.0f, TileID::Air};
      };

      auto hasSlope = [&](const int x, const int y, const float refHeight, const TileID tID) {
         bool res = false;
         for (const auto& nOffset : allNeighbors) {
            auto info = getNeighborInfo(x + nOffset[0], y + nOffset[1]);
            if (std::get<0>(info) < refHeight && std::get<2>(info) != tID) {
               res = true;
               break;
            }
         }
         return res;
      };

      auto hasSharedSlope = [&](const int x, const int y, const float refHeight, const TileID tID) {
         bool centerHasSlope = hasSlope(x, y, refHeight, TileID::Air);
         if (!centerHasSlope) {
            return false;
         }

         bool res = false;
         for (const auto& nOffset : allNeighbors) {
            auto [nHeight, nSoftness, nID] = getNeighborInfo(x + nOffset[0], y + nOffset[1]);
            if (hasSlope(x + nOffset[0], y + nOffset[1], nHeight, tID)) {
               res = true;
               break;
            }
         }
         return res;
      };

      auto hasSlopeNearby = [&](const int x, const int y, const float refHeight, const TileID tID) {
         bool res = false;
         for (const auto& nOffset : allNeighbors) {
            auto info = getNeighborInfo(x + nOffset[0], y + nOffset[1]);
            if (std::get<0>(info) != refHeight || std::get<2>(info) != tID) {
               res = true;
               break;
            }
         }
         return res;
      };

      for (int y = 0; y < Chunk::SIZE; ++y) {
         for (int x = 0; x < Chunk::SIZE; ++x) {
            int index = y * Chunk::SIZE + x;

            TileID tID = chunk.terrainMap[index];
            const TileDefinition* def = registry.get(tID);
            float defaultSoftness = def ? def->softness : 0.0f;
            float currentHeight = chunk.heightMap[index];

            chunk.softnessMap[index] = defaultSoftness;

            chunk.tileFlags[index] = Chunk::TileFlags{};

            if (defaultSoftness > 0.0001f) {
               bool shouldZeroOut = true;
               for (const auto& offset : allNeighbors) {
                  int nx = x + offset[0];
                  int ny = y + offset[1];

                  auto [nHeight, nSoftness, _] = getNeighborInfo(nx, ny);

                  bool isSameHeight = std::abs(nHeight - currentHeight) < 0.0001f;
                  bool isHigherAndHard = (nHeight > currentHeight) && (nSoftness < 0.0001f);

                  if (!isSameHeight && !isHigherAndHard) {
                     shouldZeroOut = false;
                     break;
                  }
               }
               if (shouldZeroOut) {
                  chunk.tileFlags[index].skipRaymarching = true;
               }
            } else {
               chunk.tileFlags[index].skipRaymarching = true;
            }

            int lowerEdgeCount = 0;
            for (const auto& offset : edgeOffsets) {
               int nx = x + offset[0];
               int ny = y + offset[1];
               auto [nHeight, _, _1] = getNeighborInfo(nx, ny);

               if (nHeight < currentHeight - 0.0001f) {
                  lowerEdgeCount++;
               }
            }

            int lowerCornerCount = 0;
            if (lowerEdgeCount == 1) {
               for (const auto& offset : cornerOffsets) {
                  int nx = x + offset[0];
                  int ny = y + offset[1];
                  auto [nHeight, _, _1] = getNeighborInfo(nx, ny);

                  if (nHeight < currentHeight - 0.0001f) {
                     lowerCornerCount++;
                  }
               }
            }

            if (lowerEdgeCount <= 1 && lowerCornerCount == 0) {
               chunk.tileFlags[index].advancedRaymarching = false;
            } else {
               chunk.tileFlags[index].advancedRaymarching = true;
            }

            if (hasSlopeNearby(x, y, currentHeight, TileID::Air)) {
               chunk.tileFlags[index].blending = true;
               if (hasSharedSlope(x, y, currentHeight, tID) || defaultSoftness < 0.1f) {
                  chunk.tileFlags[index].triplanar = true;
               }
            }

            const uint16_t h = static_cast<uint16_t>(std::clamp(currentHeight * 127.5f, 0.0f, 255.f));
            const uint16_t s = static_cast<uint16_t>(std::clamp(chunk.softnessMap[index] * 15.0f, 0.0f, 15.0f));
            const uint16_t t =
               chunk.tileFlags[index].skipRaymarching * 8 + chunk.tileFlags[index].advancedRaymarching * 4 + chunk.tileFlags[index].blending * 2 + chunk.tileFlags[index].triplanar;

            packedMapData[index] = (h << 8) | (s << 4) | t;
            chunk.packedMap[index] = (h << 8) | (s << 4) | t;
         }
      }
   }

private:
   static glm::vec2 getAtlasCoords(const TileRegistry& registry, const TileID id, std::mt19937& rng) {
      if (id == TileID::Air) {
         return {-1.0f, -1.0f};
      }

      const TileDefinition* def = registry.get(id);
      if (!def) {
         return {-1.0f, -1.0f};
      }

      auto atlasBase = def->atlasBase;

      if (def->variationCount > 1) {
         std::uniform_int_distribution dist(0, def->variationCount - 1);
         atlasBase.y += static_cast<float>(dist(rng));
      }

      return static_cast<glm::vec2>(atlasBase);
   }
};
