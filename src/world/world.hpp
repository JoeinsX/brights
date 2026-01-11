#pragma once
#include "chunk.hpp"

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <unordered_set>
#include <webgpu/webgpu.hpp>

class ThreadPool {
public:
   explicit ThreadPool (size_t threads): stop (false) {
      for (size_t i = 0; i < threads; ++i) {
         workers.emplace_back ([this] {
            for (;;) {
               std::function<void ()> task;
               {
                  std::unique_lock<std::mutex> lock (this->queue_mutex);
                  this->condition.wait (lock, [this] { return this->stop || !this->tasks.empty (); });
                  if (this->stop && this->tasks.empty ()) {
                     return;
                  }
                  task = std::move (this->tasks.front ());
                  this->tasks.pop ();
               }
               task ();
            }
         });
      }
   }

   template<class F>
   void enqueue (F&& f) {
      {
         std::unique_lock<std::mutex> lock (queue_mutex);
         tasks.emplace (std::forward<F> (f));
      }
      condition.notify_one ();
   }

   ~ThreadPool () {
      {
         std::unique_lock<std::mutex> lock (queue_mutex);
         stop = true;
      }
      condition.notify_all ();
      for (std::thread& worker : workers) {
         worker.join ();
      }
   }

private:
   std::vector<std::thread> workers;
   std::queue<std::function<void ()>> tasks;
   std::mutex queue_mutex;
   std::condition_variable condition;
   bool stop;
};

class World {
public:
   explicit World (TileRegistry& registry, std::mt19937 rng, WorldGenerator& worldGenerator, uint32_t loadingRadius, uint32_t unloadingThreshold):
      threadPool (std::clamp (std::thread::hardware_concurrency () - 1, 1u, 4u)), loadingRadius (loadingRadius), unloadingThreshold (unloadingThreshold), registry (registry), worldGenerator (worldGenerator) {}

   std::shared_ptr<Chunk> getChunk (int x, int y) const {
      glm::ivec2 chunkPos{x, y};
      auto it = chunks.find (chunkPos);
      if (it == chunks.end ()) {
         return nullptr;
      }
      return it->second;
   }

   void update (const Camera& camera, const glm::ivec2& globalChunkMove) {
      processFinishedTasks ();

      glm::ivec2 cameraChunkPos = static_cast<glm::ivec2> (camera.getOffset ()) / Chunk::SIZE + globalChunkMove;

      for (auto it = chunks.begin (); it != chunks.end ();) {
         glm::ivec2 bl = cameraChunkPos - static_cast<int> (loadingRadius + unloadingThreshold);
         glm::ivec2 ur = cameraChunkPos + static_cast<int> (loadingRadius + unloadingThreshold);
         glm::ivec2 cp = it->second->pos;

         if (cp.x < bl.x || cp.x >= ur.x || cp.y < bl.y || cp.y >= ur.y) {
            if (pendingMeshing.contains (cp)) {
               ++it;
               continue;
            }
            it = chunks.erase (it);
         } else {
            ++it;
         }
      }

      for (int x = -loadingRadius; x < loadingRadius; ++x) {
         for (int y = -loadingRadius; y < loadingRadius; ++y) {
            glm::ivec2 chunkPos = glm::ivec2{x, y} + cameraChunkPos;

            if (chunks.contains (chunkPos) || pendingGeneration.contains (chunkPos)) {
               continue;
            }

            pendingGeneration.insert (chunkPos);

            threadPool.enqueue ([this, chunkPos] () {
               auto newChunk = std::make_shared<Chunk> (chunkPos);
               this->worldGenerator.generate (*newChunk, 0);

               std::lock_guard<std::mutex> lock (this->resultsMutex);
               this->finishedQueue.push ({TaskResult::Type::Generated, newChunk});
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

   void processFinishedTasks () {
      std::lock_guard<std::mutex> lock (resultsMutex);
      while (!finishedQueue.empty ()) {
         TaskResult result = finishedQueue.front ();
         finishedQueue.pop ();

         if (result.type == TaskResult::Type::Generated) {
            chunks[result.chunk->pos] = result.chunk;
            pendingGeneration.erase (result.chunk->pos);

            tryQueueMeshing (result.chunk->pos);

            for (int dy = -1; dy <= 1; ++dy) {
               for (int dx = -1; dx <= 1; ++dx) {
                  if (dx == 0 && dy == 0) {
                     continue;
                  }
                  tryQueueMeshing (result.chunk->pos + glm::ivec2 (dx, dy));
               }
            }
         } else {
            result.chunk->isMeshed = true;
            result.chunk->needsReloading = true;
            pendingMeshing.erase (result.chunk->pos);
         }
      }
   }

   void tryQueueMeshing (glm::ivec2 pos) {
      if (pendingMeshing.contains (pos)) {
         return;
      }

      auto it = chunks.find (pos);
      if (it == chunks.end ()) {
         return;
      }
      auto chunk = it->second;

      if (chunk->isMeshed) {
         return;
      }

      std::array<std::shared_ptr<Chunk>, 8> neighbors{};
      int nIndex = 0;

      for (int dy = -1; dy <= 1; ++dy) {
         for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
               continue;
            }
            auto nIt = chunks.find (pos + glm::ivec2 (dx, dy));
            if (nIt == chunks.end ()) {
               return;
            }

            neighbors[nIndex++] = nIt->second;
         }
      }

      pendingMeshing.insert (pos);

      threadPool.enqueue ([this, chunk, neighbors] () {
         std::seed_seq seed{chunk->pos.x, chunk->pos.y, 42};
         std::mt19937 localRng (seed);

         chunk->rebuildDisplayMap (this->registry, localRng, neighbors);

         std::lock_guard<std::mutex> lock (this->resultsMutex);
         this->finishedQueue.push ({TaskResult::Type::Meshed, chunk});
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
};

class WorldRenderAdapter {
public:
   WorldRenderAdapter (wgpu::Queue queue, wgpu::Buffer chunkOffsetBuffer, wgpu::Buffer chunkDataBuffer, wgpu::Buffer tilemapBuffer, uint32_t renderRadius, World& world, Camera& camera):
      queue (queue), chunkOffsetBuffer (chunkOffsetBuffer), chunkDataBuffer (chunkDataBuffer), tilemapBuffer (tilemapBuffer), renderRadius (renderRadius), world (world) {}

   void update (Camera& camera, glm::ivec2& globalChunkMove) {
      glm::ivec2 mapCenter{Chunk::SIZE * Chunk::COUNT / 2};
      glm::ivec2 camPosOffset = static_cast<glm::ivec2> (camera.getOffset ()) - mapCenter;
      glm::ivec2 chunkMove = camPosOffset / Chunk::SIZE;

      if (chunkMove != glm::ivec2 (0)) {
         globalChunkMove += chunkMove;
         camera.setOffset (camera.getOffset () - glm::vec2 (chunkMove * Chunk::SIZE));
      }

      for (int y = 0; y < renderRadius * 2; ++y) {
         for (int x = 0; x < renderRadius * 2; ++x) {
            int x1 = (x + globalChunkMove.x) & (Chunk::COUNT - 1);
            int y1 = (y + globalChunkMove.y) & (Chunk::COUNT - 1);

            auto chunk = world.getChunk (x + globalChunkMove.x, y + globalChunkMove.y);

            uint32_t idx1 = y1 * renderRadius * 2 + x1;

            if (!chunk || !chunk->isMeshed) {
               uint32_t zeroOffset = 0;
               queue.writeBuffer (chunkOffsetBuffer, idx1 * sizeof (uint32_t), &zeroOffset, sizeof (uint32_t));
               continue;
            }

            if (chunk->needsReloading) {
               queue.writeBuffer (chunkDataBuffer, chunk->getPackedData ().size () * sizeof (uint16_t) * idx1, chunk->getPackedData ().data (), chunk->getPackedData ().size () * sizeof (uint16_t));

               queue.writeBuffer (tilemapBuffer, chunk->getDisplayData ().size () * sizeof (uint8_t) * idx1, chunk->getDisplayData ().data (), chunk->getDisplayData ().size () * sizeof (uint8_t));

               chunk->needsReloading = false;
            }

            uint32_t offset = idx1 * Chunk::SIZE * Chunk::SIZE;
            queue.writeBuffer (chunkOffsetBuffer, idx1 * sizeof (uint32_t), &offset, sizeof (uint32_t));
         }
      }
   }

   wgpu::Queue queue = nullptr;
   wgpu::Buffer chunkOffsetBuffer = nullptr;
   wgpu::Buffer chunkDataBuffer = nullptr;
   wgpu::Buffer tilemapBuffer = nullptr;
   World& world;
   uint32_t renderRadius = 0;
};
