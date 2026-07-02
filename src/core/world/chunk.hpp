#pragma once

#include "core/world/contents/entity.hpp"
#include "core/world/contents/tile.hpp"

#include <glm/glm.hpp>
#include <vector>

class Chunk {
   friend class ChunkMesher;

public:
   static constexpr int SIZE = 32;
   static constexpr int SIZE_SQUARED = SIZE * SIZE;
   static constexpr int COUNT = 32;
   static constexpr int COUNT_SQUARED = COUNT * COUNT;

   explicit Chunk(const glm::ivec2 pos): pos(pos) {
      terrainMap.resize(SIZE_SQUARED, TileID::Water);
      heightMap.resize(SIZE_SQUARED, 0.0f);
   }

   void setTerrain(const int x, const int y, const TileID id, const float height) {
      if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
         return;
      }
      const int idx = y * SIZE + x;
      terrainMap[idx] = id;
      heightMap[idx] = height;
   }

   void addEntity(const EntitySpawn& entity) { entities.push_back(entity); }

   [[nodiscard]] TileID terrainAt(const int x, const int y) const { return terrainMap[y * SIZE + x]; }
   [[nodiscard]] float heightAt(const int x, const int y) const { return heightMap[y * SIZE + x]; }

   [[nodiscard]] const std::vector<EntitySpawn>& getEntities() const { return entities; }

   [[nodiscard]] glm::ivec2 getPos() const { return pos; }

   [[nodiscard]] bool isMeshed() const { return meshed; }

   void markMeshed() { meshed = true; }

private:
   std::vector<TileID> terrainMap;
   std::vector<float> heightMap;
   std::vector<EntitySpawn> entities;

   glm::ivec2 pos{};
   bool meshed = false;
};

inline glm::ivec2 toChunkCoord(const glm::ivec2 worldTile) {
   const auto floorDiv = [](const int v) { return v >= 0 ? v / Chunk::SIZE : -((-v + Chunk::SIZE - 1) / Chunk::SIZE); };
   return {floorDiv(worldTile.x), floorDiv(worldTile.y)};
}
