#pragma once

#include "chunk.hpp"
#include "chunkMesher.hpp"
#include "render/core/camera.hpp"
#include "util/threadpool.hpp"
#include "worldGenerator.hpp"
#include "worldRenderAdapter.hpp"

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <unordered_set>

class World {
public:
   explicit World(TileRegistry& registry, std::mt19937 rng, WorldGenerator& worldGenerator, WorldRenderAdapter& renderAdapter, uint32_t loadingRadius, uint32_t unloadingThreshold):
      threadPool(std::clamp(std::thread::hardware_concurrency() - 1, 1u, 4u)), loadingRadius(loadingRadius), unloadingThreshold(unloadingThreshold), registry(registry),
      worldGenerator(worldGenerator), renderAdapter(renderAdapter) {}

   std::shared_ptr<Chunk> getChunk(int x, int y) const {
      const glm::ivec2 chunkPos{x, y};
      const auto it = chunks.find(chunkPos);
      if (it == chunks.end()) {
         return nullptr;
      }
      return it->second;
   }

   void update(const Camera& camera, const glm::ivec2& globalChunkMove) {
      processFinishedTasks();

      glm::ivec2 cameraChunkPos = static_cast<glm::ivec2>(camera.getOffset()) / Chunk::SIZE + globalChunkMove;

      for (auto it = chunks.begin(); it != chunks.end();) {
         glm::ivec2 bl = cameraChunkPos - static_cast<int>(loadingRadius + unloadingThreshold);
         glm::ivec2 ur = cameraChunkPos + static_cast<int>(loadingRadius + unloadingThreshold);
         glm::ivec2 cp = it->second->getPos();

         if (cp.x < bl.x || cp.x >= ur.x || cp.y < bl.y || cp.y >= ur.y) {
            if (pendingMeshing.contains(cp)) {
               ++it;
               continue;
            }
            it = chunks.erase(it);
         } else {
            ++it;
         }
      }

      for (int x = -loadingRadius; x < loadingRadius; ++x) {
         for (int y = -loadingRadius; y < loadingRadius; ++y) {
            glm::ivec2 chunkPos = glm::ivec2{x, y} + cameraChunkPos;

            if (chunks.contains(chunkPos) || pendingGeneration.contains(chunkPos)) {
               continue;
            }

            pendingGeneration.insert(chunkPos);

            threadPool.enqueue([this, chunkPos]() {
               auto newChunk = std::make_shared<Chunk>(chunkPos);
               this->worldGenerator.generate(*newChunk, 0);

               std::lock_guard<std::mutex> lock(this->resultsMutex);
               this->finishedQueue.push({TaskResult::Type::Generated, newChunk});
            });
         }
      }
   }

private:
   struct TaskResult {
      enum class Type {
         Generated,
         Meshed
      } type;

      std::shared_ptr<Chunk> chunk;
   };

   void processFinishedTasks() {
      std::lock_guard<std::mutex> lock(resultsMutex);
      while (!finishedQueue.empty()) {
         TaskResult result = finishedQueue.front();
         finishedQueue.pop();

         if (result.type == TaskResult::Type::Generated) {
            chunks[result.chunk->getPos()] = result.chunk;
            pendingGeneration.erase(result.chunk->getPos());

            tryQueueMeshing(result.chunk->getPos());

            for (int dy = -1; dy <= 1; ++dy) {
               for (int dx = -1; dx <= 1; ++dx) {
                  if (dx == 0 && dy == 0) {
                     continue;
                  }
                  tryQueueMeshing(result.chunk->getPos() + glm::ivec2(dx, dy));
               }
            }
         } else {
            result.chunk->setFlag(Chunk::ChunkState::Enum::Meshed);
            result.chunk->setFlag(Chunk::ChunkState::Enum::NeedsGpuUpload);
            pendingMeshing.erase(result.chunk->getPos());
         }
      }
   }

   void tryQueueMeshing(const glm::ivec2 pos) {
      if (pendingMeshing.contains(pos)) {
         return;
      }

      const auto it = chunks.find(pos);
      if (it == chunks.end()) {
         return;
      }
      auto chunk = it->second;

      if (chunk->hasFlag(Chunk::ChunkState::Enum::Meshed)) {
         return;
      }

      std::array<std::shared_ptr<Chunk>, 8> neighbors{};
      int nIndex = 0;

      for (int dy = -1; dy <= 1; ++dy) {
         for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
               continue;
            }
            auto nIt = chunks.find(pos + glm::ivec2(dx, dy));
            if (nIt == chunks.end()) {
               return;
            }

            neighbors[nIndex++] = nIt->second;
         }
      }

      pendingMeshing.insert(pos);

      threadPool.enqueue([this, chunk, neighbors]() {
         std::seed_seq seed{chunk->getPos().x, chunk->getPos().y, 42};
         std::mt19937 localRng(seed);

         ChunkMesher::meshChunk(*chunk, this->registry, localRng, neighbors, renderAdapter);
         renderAdapter.onChunkDataUpdated(chunk->getPos());

         std::lock_guard lock(this->resultsMutex);
         this->finishedQueue.push({TaskResult::Type::Meshed, chunk});
      });
   }

   std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>> chunks;
   std::unordered_set<glm::ivec2> pendingGeneration;
   std::unordered_set<glm::ivec2> pendingMeshing;

   ThreadPool threadPool;
   std::queue<TaskResult> finishedQueue;
   std::mutex resultsMutex;

   int32_t loadingRadius = 0;
   uint32_t unloadingThreshold = 0;
   TileRegistry& registry;
   WorldGenerator& worldGenerator;
   WorldRenderAdapter& renderAdapter;
};
