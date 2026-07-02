#pragma once

#include "core/graphics/camera.hpp"
#include "core/world/chunk.hpp"
#include "core/world/contents/atlasCell.hpp"
#include "core/world/contents/entity.hpp"
#include "core/world/ecs/entitySimulation.hpp"
#include "core/world/generation/worldGenerator.hpp"
#include "core/world/graphics/chunkMesher.hpp"
#include "core/world/graphics/spriteInstance.hpp"
#include "core/world/graphics/worldRenderAdapter.hpp"
#include "core/world/heightField.hpp"
#include "util/threadpool.hpp"

#include <array>
#include <cstdint>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class WorldArea {
public:
   WorldArea(Threadpool& threadPool, TileRegistry& tileRegistry, const EntityRegistry& entityRegistry, WorldGenerator& worldGenerator, WorldRenderAdapter& renderAdapter,
             uint32_t loadingRadius, uint32_t unloadingThreshold):
      loadingRadius(loadingRadius), unloadingThreshold(unloadingThreshold), threadPool(threadPool), tileRegistry(tileRegistry), worldGenerator(worldGenerator),
      renderAdapter(renderAdapter), entityRegistry(entityRegistry) {}

   [[nodiscard]] uint32_t getStaticSpriteCount() const { return staticSpriteCount; }

   [[nodiscard]] uint32_t getDynamicSpriteCount() const { return dynamicSpriteCount; }

   void update(Camera& camera, const glm::ivec2& globalChunkMove, const float dtSeconds, const glm::vec2 controlAxis, const std::optional<glm::vec2> cursorWorld) {
      processFinishedTasks();

      const HeightField heightField{chunks, tileRegistry};
      simulation.update(camera, heightField, dtSeconds, controlAxis, cursorWorld, globalChunkMove);

      const glm::ivec2 cameraChunkPos = static_cast<glm::ivec2>(camera.getOffset()) / Chunk::SIZE + globalChunkMove;

      const glm::ivec2 bl = cameraChunkPos - static_cast<int32_t>(loadingRadius + unloadingThreshold);
      const glm::ivec2 ur = cameraChunkPos + static_cast<int32_t>(loadingRadius + unloadingThreshold);

      for (auto it = chunks.begin(); it != chunks.end();) {
         if (const glm::ivec2 cp = it->second->getPos(); cp.x < bl.x || cp.x >= ur.x || cp.y < bl.y || cp.y >= ur.y) {
            if (pendingMeshing.contains(cp)) {
               ++it;
               continue;
            }
            staticDirty |= !it->second->getEntities().empty();
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

      if (staticDirty) {
         rebuildStaticSprites();
         staticDirty = false;
      }
      if (simulation.collectSprites(spriteUploadBuffer)) {
         dynamicSpriteCount = renderAdapter.uploadDynamicSprites(spriteUploadBuffer);
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

   [[nodiscard]] const Chunk* chunkAt(const glm::ivec2 chunkPos) const {
      const auto it = chunks.find(chunkPos);
      return it == chunks.end() ? nullptr : it->second.get();
   }

   bool setTerrain(const glm::ivec2 worldTile, const TileID id, const float height) {
      const glm::ivec2 chunkPos = toChunkCoord(worldTile);
      const auto it = chunks.find(chunkPos);
      if (it == chunks.end()) {
         return false;
      }
      const glm::ivec2 local = worldTile - chunkPos * Chunk::SIZE;
      it->second->setTerrain(local.x, local.y, id, height);
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

   [[nodiscard]] const TileRegistry& tiles() const { return tileRegistry; }

   [[nodiscard]] const WorldRenderAdapter& render() const { return renderAdapter; }

   [[nodiscard]] EntitySimulation& entities() { return simulation; }

   void togglePossession(const Camera& camera, const glm::ivec2 chunkMove) { simulation.togglePossession(camera, chunkMove); }

   [[nodiscard]] bool isPossessing() const { return simulation.isPossessing(); }

private:
   void rebuildStaticSprites() {
      staticSprites.clear();
      for (const auto& [pos, chunk] : chunks) {
         for (const EntitySpawn& spawn : chunk->getEntities()) {
            const EntityDefinition& def = entityRegistry.get(spawn.kind);
            staticSprites.push_back({.position = spawn.position, .rotation = 0.0f, .spriteDimensions = def.dimensions, .spriteId = packAtlasCell(def.spriteCell), .pivotY = 0.0f});
         }
      }
      staticSpriteCount = renderAdapter.uploadStaticSprites(staticSprites);
   }

   [[nodiscard]] std::optional<std::array<std::shared_ptr<Chunk>, 8>> collectNeighbors(const glm::ivec2 pos) const {
      std::array<std::shared_ptr<Chunk>, 8> neighbors{};
      int index = 0;
      for (int dy = -1; dy <= 1; ++dy) {
         for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
               continue;
            }
            const auto it = chunks.find(pos + glm::ivec2{dx, dy});
            if (it == chunks.end()) {
               return std::nullopt;
            }
            neighbors[index++] = it->second;
         }
      }
      return neighbors;
   }

   void remeshChunk(const glm::ivec2 pos) {
      const auto it = chunks.find(pos);
      if (it == chunks.end()) {
         return;
      }
      const auto neighbors = collectNeighbors(pos);
      if (!neighbors) {
         return;
      }
      std::seed_seq seed{pos.x, pos.y, chunkSeed};
      std::mt19937 rng(seed);
      ChunkMesher::meshChunk(*it->second, tileRegistry, rng, *neighbors, renderAdapter);
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
      std::queue<TaskResult> finished;
      {
         const std::lock_guard<std::mutex> lock(resultsMutex);
         std::swap(finished, finishedQueue);
      }

      while (!finished.empty()) {
         const TaskResult result = finished.front();
         finished.pop();

         if (result.type == TaskResult::Type::Generated) {
            chunks[result.chunk->getPos()] = result.chunk;
            pendingGeneration.erase(result.chunk->getPos());
            staticDirty |= !result.chunk->getEntities().empty();

            for (int dy = -1; dy <= 1; ++dy) {
               for (int dx = -1; dx <= 1; ++dx) {
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
      if (it == chunks.end() || it->second->isMeshed()) {
         return;
      }

      const auto neighbors = collectNeighbors(pos);
      if (!neighbors) {
         return;
      }

      pendingMeshing.insert(pos);

      threadPool.enqueue([this, chunk = it->second, neighbors = *neighbors]() {
         std::seed_seq seed{chunk->getPos().x, chunk->getPos().y, chunkSeed};
         std::mt19937 rng(seed);

         ChunkMesher::meshChunk(*chunk, tileRegistry, rng, neighbors, renderAdapter);

         const std::scoped_lock lock(resultsMutex);
         finishedQueue.push({TaskResult::Type::Meshed, chunk});
      });
   }

   static constexpr int32_t chunkSeed = 42;

   uint32_t loadingRadius = 0;
   uint32_t unloadingThreshold = 0;
   Threadpool& threadPool;
   TileRegistry& tileRegistry;
   WorldGenerator& worldGenerator;
   WorldRenderAdapter& renderAdapter;
   const EntityRegistry& entityRegistry;
   EntitySimulation simulation;

   std::vector<SpriteInstance> staticSprites;
   std::vector<SpriteInstance> spriteUploadBuffer;
   uint32_t staticSpriteCount = 0;
   uint32_t dynamicSpriteCount = 0;
   bool staticDirty = true;

   std::queue<TaskResult> finishedQueue;
   std::mutex resultsMutex;

   std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>> chunks;
   std::unordered_set<glm::ivec2> pendingGeneration;
   std::unordered_set<glm::ivec2> pendingMeshing;
};
