#pragma once

#include "chunk.hpp"
#include "chunkMesher.hpp"
#include "core/graphics/camera.hpp"
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
#include <utility>

class World {
public:
   explicit World(TileRegistry& registry, std::mt19937 /*rng*/, WorldGenerator& worldGenerator, WorldRenderAdapter& renderAdapter, uint32_t loadingRadius,
                  uint32_t unloadingThreshold):
      loadingRadius(loadingRadius), unloadingThreshold(unloadingThreshold), registry(registry), worldGenerator(worldGenerator), renderAdapter(renderAdapter),
      threadPool(std::clamp(std::thread::hardware_concurrency() - 1, 1u, 4u)) {}

   [[nodiscard]] std::shared_ptr<Chunk> getChunk(int x, int y) const {
      const glm::ivec2 chunkPos{x, y};
      const auto it = chunks.find(chunkPos);
      if (it == chunks.end()) {
         return nullptr;
      }
      return it->second;
   }

   void update(const Camera& camera, const glm::ivec2& globalChunkMove) {
      processFinishedTasks();

      const glm::ivec2 cameraChunkPos = static_cast<glm::ivec2>(camera.getOffset()) / Chunk::SIZE + globalChunkMove;

      for (auto it = chunks.begin(); it != chunks.end();) {
         const glm::ivec2 bl = cameraChunkPos - static_cast<int32_t>(loadingRadius + unloadingThreshold);
         const glm::ivec2 ur = cameraChunkPos + static_cast<int32_t>(loadingRadius + unloadingThreshold);

         if (const glm::ivec2 cp = it->second->getPos(); cp.x < bl.x || cp.x >= ur.x || cp.y < bl.y || cp.y >= ur.y) {
            if (pendingMeshing.contains(cp)) {
               ++it;
               continue;
            }
            it = chunks.erase(it);
         } else {
            ++it;
         }
      }

      for (int x = -static_cast<int32_t>(loadingRadius); std::cmp_less(x, loadingRadius); ++x) {
         for (int y = -static_cast<int32_t>(loadingRadius); std::cmp_less(y, loadingRadius); ++y) {
            const glm::ivec2 chunkPos = glm::ivec2{x, y} + cameraChunkPos;

            if (chunks.contains(chunkPos) || pendingGeneration.contains(chunkPos)) {
               continue;
            }

            pendingGeneration.insert(chunkPos);

            threadPool.enqueue([this, chunkPos]() {
               auto newChunk = std::make_shared<Chunk>(chunkPos);
               WorldGenerator::generate(*newChunk, 0);

               const std::lock_guard<std::mutex> lock(resultsMutex);
               finishedQueue.push({TaskResult::Type::Generated, newChunk});
            });
         }
      }
   }

private:
   struct TaskResult {
      enum class Type : uint8_t {
         Generated,
         Meshed
      } type;

      std::shared_ptr<Chunk> chunk;
   };

   void processFinishedTasks() {
      const std::lock_guard<std::mutex> lock(resultsMutex);
      while (!finishedQueue.empty()) {
         const TaskResult result = finishedQueue.front();
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
            result.chunk->setFlag(ChunkState::Meshed);
            result.chunk->setFlag(ChunkState::NeedsGpuUpload);
            pendingMeshing.erase(result.chunk->getPos());
            renderAdapter.onChunkDataUpdated(result.chunk->getPos());
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

      if (chunk->hasFlag(ChunkState::Meshed)) {
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

      static constexpr int32_t chunkSeed = 42;

      threadPool.enqueue([this, chunk, neighbors]() -> void {
         std::seed_seq seed{chunk->getPos().x, chunk->getPos().y, chunkSeed};
         std::mt19937 localRng(seed);

         ChunkMesher::meshChunk(*chunk, this->registry, localRng, neighbors, renderAdapter);

         const std::scoped_lock lock(this->resultsMutex);
         this->finishedQueue.push({TaskResult::Type::Meshed, chunk});
      });
   }

   std::queue<TaskResult> finishedQueue;
   std::mutex resultsMutex;

   std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>> chunks;
   std::unordered_set<glm::ivec2> pendingGeneration;
   std::unordered_set<glm::ivec2> pendingMeshing;

   uint32_t loadingRadius = 0;
   uint32_t unloadingThreshold = 0;
   TileRegistry& registry;
   WorldGenerator& worldGenerator;
   WorldRenderAdapter& renderAdapter;

   ThreadPool threadPool;
};
