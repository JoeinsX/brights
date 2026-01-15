#pragma once
#include "tile.hpp"
#include "util/bitmask.hpp"

#include <glm/glm.hpp>
#include <vector>

enum class ChunkState : uint8_t {
   None = 0,
   Generated = 1,
   Meshed = Generated << 1,
   NeedsGpuUpload = Meshed << 1
};

ENABLE_BITMASK_OPERATORS(ChunkState);

class Chunk {
   friend class ChunkMesher;

public:
   static constexpr int SIZE = 32;
   static constexpr int SIZE_SQUARED = SIZE * SIZE;
   static constexpr int COUNT = 32;
   static constexpr int COUNT_SQUARED = COUNT * COUNT;
   static constexpr int COUNT_SQUARED_EX = COUNT * COUNT;

   enum class State : uint8_t {
      None = 0,
      Generated = 1,
      Meshed = Generated << 1,
      NeedsGpuUpload = Meshed << 1
   };

   explicit Chunk(const glm::ivec2 pos): pos(pos) {
      terrainMap.resize(SIZE_SQUARED, TileID::Water);
      heightMap.resize(SIZE_SQUARED, 0.0f);
      packedMap.resize(SIZE_SQUARED, 0);
   }

   void setTerrain(const int x, const int y, const TileID id) {
      if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
         return;
      }
      terrainMap[y * SIZE + x] = id;
   }

   [[nodiscard]] const std::vector<uint16_t>& getPackedData() const { return packedMap; }

   [[nodiscard]] glm::ivec2 getPos() const { return pos; }

   [[nodiscard]] bool hasFlag(const ChunkState flag) const { return static_cast<bool>(state & flag); }

   void setFlag(const ChunkState flag) { state |= flag; }

   void clearFlag(const ChunkState flag) { state &= (~flag); }

private:
   std::vector<TileID> terrainMap;
   std::vector<float> heightMap;
   std::vector<uint16_t> packedMap;

   glm::ivec2 pos{};
   ChunkState state{};
};
