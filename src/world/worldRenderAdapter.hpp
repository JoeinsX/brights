#pragma once

#include "render/core/camera.hpp"

#include <stack>

class WorldRenderAdapter {
public:
   WorldRenderAdapter(const wgpu::Queue queue, const wgpu::Buffer chunkOffsetBuffer, const wgpu::Buffer chunkDataBuffer, const wgpu::Buffer tilemapBuffer,
                      const uint32_t renderRadius):
      queue(queue), chunkOffsetBuffer(chunkOffsetBuffer), chunkDataBuffer(chunkDataBuffer), tilemapBuffer(tilemapBuffer), renderRadius(renderRadius) {
      displayChunkMaps.resize(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED);
      packedChunkMaps.resize(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED);
   }

   void onChunkDataUpdated(const glm::ivec2 chunkPos) {
      const uint16_t bufferOffset = mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2);
      updatedDataOffsets.push(bufferOffset);
   }

   void update(Camera& camera, glm::ivec2& globalChunkMove) {
      glm::ivec2 mapCenter{Chunk::SIZE * Chunk::COUNT / 2};
      glm::ivec2 camPosOffset = static_cast<glm::ivec2>(camera.getOffset()) - mapCenter;
      glm::ivec2 chunkMove = camPosOffset / Chunk::SIZE;

      if (chunkMove != glm::ivec2(0)) {
         globalChunkMove += chunkMove;
         camera.setOffset(camera.getOffset() - glm::vec2(chunkMove * Chunk::SIZE));
      }

      while (!updatedDataOffsets.empty()) {
         const uint32_t offset = updatedDataOffsets.top() * Chunk::SIZE_SQUARED;
         updatedDataOffsets.pop();

         queue.writeBuffer(chunkDataBuffer, offset * sizeof(uint16_t), packedChunkMaps.data() + offset, Chunk::SIZE_SQUARED * sizeof(uint16_t));

         queue.writeBuffer(tilemapBuffer, offset * sizeof(uint8_t), displayChunkMaps.data() + offset, Chunk::SIZE_SQUARED * sizeof(uint8_t));
      }
   }

   uint8_t* getDisplayDataPtrForChunk(const glm::ivec2 chunkPos) { return displayChunkMaps.data() + mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2) * Chunk::SIZE_SQUARED; }

   uint16_t* getPackedDataPtrForChunk(const glm::ivec2 chunkPos) { return packedChunkMaps.data() + mapChunkPosToBufferIndex(chunkPos, Chunk::COUNT / 2) * Chunk::SIZE_SQUARED; }

private:
   static uint32_t mapChunkPosToBufferIndex(const glm::ivec2 chunkPos, const uint32_t localBufferRadiusChunks) {
      const glm::ivec2 localChunkPos = chunkPos & (Chunk::COUNT - 1);
      const uint32_t localBufferIndex = localChunkPos.y * localBufferRadiusChunks * 2 + localChunkPos.x;
      return localBufferIndex;
   }

   wgpu::Queue queue = nullptr;
   wgpu::Buffer chunkOffsetBuffer = nullptr;
   wgpu::Buffer chunkDataBuffer = nullptr;
   wgpu::Buffer tilemapBuffer = nullptr;
   std::vector<uint16_t> packedChunkMaps;
   std::vector<uint8_t> displayChunkMaps;
   uint32_t renderRadius = 0;
   std::stack<uint16_t> updatedDataOffsets;
};
