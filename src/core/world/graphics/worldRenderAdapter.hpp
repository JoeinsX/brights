#pragma once

#include "core/graphics/camera.hpp"
#include "core/world/chunk.hpp"
#include "core/world/graphics/spriteInstance.hpp"

#include <algorithm>
#include <vector>
#include <webgpu/webgpu.hpp>

class WorldRenderAdapter {
public:
   WorldRenderAdapter(const wgpu::Queue queue, const wgpu::Buffer packedBuffer, const wgpu::Buffer tilemapBuffer, const wgpu::Buffer spriteBuffer):
      queue(queue), packedBuffer(packedBuffer), tilemapBuffer(tilemapBuffer), spriteBuffer(spriteBuffer) {
      displayChunkMaps.resize(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED);
      packedChunkMaps.resize(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED);
   }

   uint32_t uploadStaticSprites(const std::vector<SpriteInstance>& sprites) {
      const uint32_t count = std::min(static_cast<uint32_t>(sprites.size()), staticSpriteCapacity);
      if (count != 0) {
         queue.writeBuffer(spriteBuffer, 0, sprites.data(), static_cast<size_t>(count) * sizeof(SpriteInstance));
      }
      return count;
   }

   uint32_t uploadDynamicSprites(const std::vector<SpriteInstance>& sprites) {
      const uint32_t count = std::min(static_cast<uint32_t>(sprites.size()), dynamicSpriteCapacity);
      if (count != 0) {
         constexpr uint64_t base = static_cast<uint64_t>(staticSpriteCapacity) * sizeof(SpriteInstance);
         queue.writeBuffer(spriteBuffer, base, sprites.data(), static_cast<size_t>(count) * sizeof(SpriteInstance));
      }
      return count;
   }

   void onChunkDataUpdated(const glm::ivec2 chunkPos) {
      const uint16_t bufferOffset = mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2);
      updatedChunkIndices.push_back(bufferOffset);
   }

   void update(Camera& camera, glm::ivec2& globalChunkMove) {
      constexpr glm::ivec2 mapCenter{Chunk::SIZE * Chunk::COUNT / 2};
      const glm::ivec2 camPosOffset = static_cast<glm::ivec2>(camera.getOffset()) - mapCenter;

      if (const glm::ivec2 chunkMove = camPosOffset / Chunk::SIZE; chunkMove != glm::ivec2(0)) {
         globalChunkMove += chunkMove;
         camera.setOffset(camera.getOffset() - glm::vec2(chunkMove * Chunk::SIZE));
      }

      std::sort(updatedChunkIndices.begin(), updatedChunkIndices.end());
      const auto duplicatesBegin = std::unique(updatedChunkIndices.begin(), updatedChunkIndices.end());
      updatedChunkIndices.erase(duplicatesBegin, updatedChunkIndices.end());

      for (const uint16_t chunkIndex : updatedChunkIndices) {
         const uint32_t offset = static_cast<uint32_t>(chunkIndex) * Chunk::SIZE_SQUARED;
         queue.writeBuffer(packedBuffer, offset * sizeof(uint16_t), packedChunkMaps.data() + offset, Chunk::SIZE_SQUARED * sizeof(uint16_t));
         queue.writeBuffer(tilemapBuffer, offset * sizeof(uint8_t), displayChunkMaps.data() + offset, Chunk::SIZE_SQUARED * sizeof(uint8_t));
      }
      updatedChunkIndices.clear();
   }

   uint8_t* getDisplayDataPtrForChunk(const glm::ivec2 chunkPos) { return displayChunkMaps.data() + mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2) * Chunk::SIZE_SQUARED; }

   uint16_t* getPackedDataPtrForChunk(const glm::ivec2 chunkPos) { return packedChunkMaps.data() + mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2) * Chunk::SIZE_SQUARED; }

   [[nodiscard]] uint8_t getDisplayAt(const glm::ivec2 chunkPos, const int localIndex) const {
      return displayChunkMaps[mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2) * Chunk::SIZE_SQUARED + localIndex];
   }

   [[nodiscard]] uint16_t getPackedAt(const glm::ivec2 chunkPos, const int localIndex) const {
      return packedChunkMaps[mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2) * Chunk::SIZE_SQUARED + localIndex];
   }

private:
   // ReSharper disable once CppDFAConstantParameter
   static uint32_t mapChunkPosToBufferIndex(const glm::ivec2 chunkPos, const uint32_t localBufferRadiusChunks) {
      const glm::ivec2 localChunkPos = chunkPos & (Chunk::COUNT - 1);
      const uint32_t localBufferIndex = localChunkPos.y * localBufferRadiusChunks * 2 + localChunkPos.x;
      return localBufferIndex;
   }

   wgpu::Queue queue = nullptr;
   wgpu::Buffer packedBuffer = nullptr;
   wgpu::Buffer tilemapBuffer = nullptr;
   wgpu::Buffer spriteBuffer = nullptr;
   std::vector<uint16_t> packedChunkMaps;
   std::vector<uint8_t> displayChunkMaps;
   std::vector<uint16_t> updatedChunkIndices;
};
