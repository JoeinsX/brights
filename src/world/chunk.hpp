#pragma once
#include "tile.hpp"

#include <glm/glm.hpp>
#include <vector>

class Chunk {
   friend class ChunkMesher;

public:
   static constexpr int SIZE = 32;
   static constexpr int SIZE_SQUARED = SIZE * SIZE;
   static constexpr int COUNT = 32;
   static constexpr int COUNT_SQUARED = COUNT * COUNT;
   static constexpr int COUNT_SQUARED_EX = COUNT * COUNT;

   class ChunkState {
   public:
      enum class Enum : std::uint8_t {
         Generated = 1,
         Meshed = Generated << 1,
         NeedsGpuUpload = Meshed << 1
      };

      Enum state = static_cast<Enum>(0);

      void set(Enum flag) { state = static_cast<Enum>(static_cast<uint8_t>(state) | static_cast<uint8_t>(flag)); }

      void clear(Enum flag) { state = static_cast<Enum>(static_cast<uint8_t>(state) ^ (static_cast<uint8_t>(state) & static_cast<uint8_t>(flag))); }

      bool is(Enum flag) const { return static_cast<uint8_t>(state) & static_cast<uint8_t>(flag); }
   };

   explicit Chunk(const glm::ivec2 pos): pos(pos) {
      terrainMap.resize(SIZE_SQUARED, TileID::Water);
      wallMap.resize(SIZE_SQUARED, TileID::Air);
      heightMap.resize(SIZE_SQUARED, 0.0f);
      softnessMap.resize(SIZE_SQUARED, 0.0f);
      tileFlags.resize(SIZE_SQUARED, {});
      packedMap.resize(SIZE_SQUARED, 0);
   }

   void setTerrain(const int x, const int y, const TileID id) {
      if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
         return;
      }
      terrainMap[y * SIZE + x] = id;
   }

   void setWall(const int x, const int y, const TileID id) {
      if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
         return;
      }
      wallMap[y * SIZE + x] = id;
   }

   [[nodiscard]] const std::vector<uint16_t>& getPackedData() const { return packedMap; }

   [[nodiscard]] glm::ivec2 getPos() const { return pos; }

   [[nodiscard]] bool hasFlag(const ChunkState::Enum flag) const { return state.is(flag); }

   void setFlag(const ChunkState::Enum flag) { state.set(flag); }

   void clearFlag(const ChunkState::Enum flag) { state.clear(flag); }

private:
   std::vector<TileID> terrainMap;
   std::vector<TileID> wallMap;
   std::vector<float> heightMap;
   std::vector<float> softnessMap;

   struct TileFlags {
      bool skipRaymarching = false;
      bool advancedRaymarching = false;
      bool blending = false;
      bool triplanar = false;
   };

   std::vector<TileFlags> tileFlags;
   std::vector<uint16_t> packedMap;

   glm::ivec2 pos{};
   ChunkState state;
};
