#pragma once

#include "chunk.hpp"
#include "chunkMesher.hpp"
#include "core/graphics/camera.hpp"
#include "tileInspection.hpp"
#include "util/threadpool.hpp"
#include "worldEdit.hpp"
#include "worldGenerator.hpp"
#include "worldRenderAdapter.hpp"

#include <algorithm>
#include <array>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class World {
public:
   explicit World(Threadpool& threadPool, TileRegistry& registry, WorldGenerator& worldGenerator, WorldRenderAdapter& renderAdapter, uint32_t loadingRadius,
                  uint32_t unloadingThreshold):
      loadingRadius(loadingRadius), unloadingThreshold(unloadingThreshold), threadPool(threadPool), registry(registry), worldGenerator(worldGenerator),
      renderAdapter(renderAdapter) {}

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
               worldGenerator.generate(*newChunk);

               const std::lock_guard<std::mutex> lock(resultsMutex);
               finishedQueue.push({TaskResult::Type::Generated, newChunk});
            });
         }
      }
   }

   [[nodiscard]] std::optional<float> sampleHeight(const glm::ivec2 worldTile) const {
      const glm::ivec2 chunkPos = toChunkCoord(worldTile);
      const auto it = chunks.find(chunkPos);
      if (it == chunks.end()) {
         return std::nullopt;
      }
      const glm::ivec2 local = worldTile - chunkPos * Chunk::SIZE;
      return it->second->heightAt(local.x, local.y);
   }

   [[nodiscard]] TileInspection inspect(const glm::ivec2 worldTile) const {
      TileInspection info;
      info.worldTile = worldTile;
      info.chunkPos = toChunkCoord(worldTile);
      info.localTile = worldTile - info.chunkPos * Chunk::SIZE;

      const auto it = chunks.find(info.chunkPos);
      if (it == chunks.end()) {
         return info;
      }
      const Chunk& chunk = *it->second;
      const int idx = info.localTile.y * Chunk::SIZE + info.localTile.x;

      info.loaded = true;
      info.id = chunk.terrainAt(info.localTile.x, info.localTile.y);
      info.height = chunk.heightAt(info.localTile.x, info.localTile.y);
      info.softness = registry.get(info.id).softness;
      info.meshed = chunk.isMeshed();

      if (info.meshed) {
         const uint8_t display = renderAdapter.getDisplayAt(info.chunkPos, idx);
         info.atlasCoords = {display >> 4, display & 0x0F};
         info.flags = static_cast<RenderFlag>(renderAdapter.getPackedAt(info.chunkPos, idx) & 0x0F);
      } else {
         info.atlasCoords = registry.get(info.id).atlasBase;
      }
      return info;
   }

   int applyBrush(const glm::ivec2 centerTile, const BrushSettings& brush, const float dtMs) {
      float trimHeight = 0.0f;
      if (brush.tool == EditTool::Trimmer) {
         const auto sampled = sampleHeight(centerTile);
         if (!sampled) {
            return 0;
         }
         trimHeight = *sampled;
      }
      const float heightStep = brush.heightRate * (dtMs / 1000.0f);

      std::unordered_set<glm::ivec2> dirty;
      int painted = 0;
      const int radius = brush.radius;
      for (int dy = -radius; dy <= radius; ++dy) {
         for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
               continue;
            }
            const glm::ivec2 tile = centerTile + glm::ivec2{dx, dy};
            if (editTile(tile, brush, trimHeight, heightStep)) {
               dirty.insert(toChunkCoord(tile));
               ++painted;
            }
         }
      }

      remeshDirty(dirty);
      return painted;
   }

private:
   static glm::ivec2 toChunkCoord(const glm::ivec2 worldTile) {
      const auto floorDiv = [](const int v) { return v >= 0 ? v / Chunk::SIZE : -((-v + Chunk::SIZE - 1) / Chunk::SIZE); };
      return {floorDiv(worldTile.x), floorDiv(worldTile.y)};
   }

   bool editTile(const glm::ivec2 worldTile, const BrushSettings& brush, const float trimHeight, const float heightStep) {
      const glm::ivec2 chunkPos = toChunkCoord(worldTile);
      const auto it = chunks.find(chunkPos);
      if (it == chunks.end()) {
         return false;
      }
      Chunk& chunk = *it->second;
      const glm::ivec2 local = worldTile - chunkPos * Chunk::SIZE;
      const TileID id = chunk.terrainAt(local.x, local.y);

      switch (brush.tool) {
      case EditTool::TileBrush:
         chunk.setTerrain(local.x, local.y, brush.tile, brush.paintHeight);
         break;
      case EditTool::HeightBrush:
         chunk.setTerrain(local.x, local.y, id, std::clamp(chunk.heightAt(local.x, local.y) + heightStep, 0.0f, 2.0f));
         break;
      case EditTool::Trimmer: {
         const float current = chunk.heightAt(local.x, local.y);
         const float trimmed = brush.trimMode == TrimMode::Lift ? std::max(current, trimHeight) : std::min(current, trimHeight);
         chunk.setTerrain(local.x, local.y, id, trimmed);
         break;
      }
      }
      return true;
   }

   void remeshDirty(const std::unordered_set<glm::ivec2>& dirty) {
      std::unordered_set<glm::ivec2> toMesh;
      for (const glm::ivec2 chunkPos : dirty) {
         for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
               toMesh.insert(chunkPos + glm::ivec2{dx, dy});
            }
         }
      }
      for (const glm::ivec2 chunkPos : toMesh) {
         remeshChunk(chunkPos);
      }
   }

   void remeshChunk(const glm::ivec2 pos) {
      const auto it = chunks.find(pos);
      if (it == chunks.end()) {
         return;
      }
      const auto chunk = it->second;

      std::array<std::shared_ptr<Chunk>, 8> neighbors{};
      int nIndex = 0;
      for (int dy = -1; dy <= 1; ++dy) {
         for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
               continue;
            }
            const auto nIt = chunks.find(pos + glm::ivec2{dx, dy});
            if (nIt == chunks.end()) {
               return;
            }
            neighbors[nIndex++] = nIt->second;
         }
      }

      std::seed_seq seed{pos.x, pos.y, chunkSeed};
      std::mt19937 rng(seed);
      ChunkMesher::meshChunk(*chunk, registry, rng, neighbors, renderAdapter);
      renderAdapter.onChunkDataUpdated(pos);
   }

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
            result.chunk->markMeshed();
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

      if (chunk->isMeshed()) {
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

      threadPool.enqueue([this, chunk, neighbors]() -> void {
         std::seed_seq seed{chunk->getPos().x, chunk->getPos().y, chunkSeed};
         std::mt19937 localRng(seed);

         ChunkMesher::meshChunk(*chunk, this->registry, localRng, neighbors, renderAdapter);

         const std::scoped_lock lock(this->resultsMutex);
         this->finishedQueue.push({TaskResult::Type::Meshed, chunk});
      });
   }

   static constexpr int32_t chunkSeed = 42;

   std::queue<TaskResult> finishedQueue;
   std::mutex resultsMutex;

   std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>> chunks;
   std::unordered_set<glm::ivec2> pendingGeneration;
   std::unordered_set<glm::ivec2> pendingMeshing;

   uint32_t loadingRadius = 0;
   uint32_t unloadingThreshold = 0;
   Threadpool& threadPool;
   TileRegistry& registry;
   WorldGenerator& worldGenerator;
   WorldRenderAdapter& renderAdapter;
};
