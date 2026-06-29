#pragma once

#include "chunk.hpp"
#include "chunkMesher.hpp"
#include "core/graphics/camera.hpp"
#include "entity.hpp"
#include "tileInspection.hpp"
#include "util/threadpool.hpp"
#include "worldEdit.hpp"
#include "worldGenerator.hpp"
#include "worldRenderAdapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

class World {
public:
   World(Threadpool& threadPool, TileRegistry& registry, const EntityRegistry& entityRegistry, WorldGenerator& worldGenerator, WorldRenderAdapter& renderAdapter,
         uint32_t loadingRadius, uint32_t unloadingThreshold):
      loadingRadius(loadingRadius), unloadingThreshold(unloadingThreshold), threadPool(threadPool), registry(registry), entityRegistry(entityRegistry),
      worldGenerator(worldGenerator), renderAdapter(renderAdapter) {
      seedDynamicEntities();
   }

   [[nodiscard]] uint32_t getEntityCount() const { return uploadedEntityCount; }

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
            staticDirty = true;
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

      repositionPlayer(camera, globalChunkMove);
      updateDynamicHeights();

      if (staticDirty) {
         rebuildStaticEntities();
         staticDirty = false;
         entitiesDirty = true;
      }
      if (entitiesDirty) {
         uploadEntities();
         entitiesDirty = false;
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
   void seedDynamicEntities() {
      const EntityDefinition& def = entityRegistry.get(EntityKind::Player);
      dynamicEntities.push_back({.spriteDimensions = def.dimensions, .spriteId = encodeSpriteCell(def.spriteCell)});
   }

   void repositionPlayer(const Camera& camera, const glm::ivec2 globalChunkMove) {
      const glm::vec2 center = camera.getOffset() + glm::vec2(globalChunkMove * Chunk::SIZE);
      Entity& player = dynamicEntities.front();
      if (glm::vec2(player.position) != center) {
         player.position = glm::vec3(center, player.position.z);
         entitiesDirty = true;
      }
   }

   void updateDynamicHeights() {
      for (Entity& entity : dynamicEntities) {
         if (const auto sampled = sampleSmoothedHeight(glm::vec2(entity.position)); sampled) {
            if (std::abs(*sampled - entity.position.z) > 1e-5f) {
               entity.position.z = *sampled;
               entitiesDirty = true;
            }
         }
      }
   }

   void rebuildStaticEntities() {
      staticEntities.clear();
      for (const auto& [pos, chunk] : chunks) {
         const std::vector<Entity>& chunkEntities = chunk->getEntities();
         staticEntities.insert(staticEntities.end(), chunkEntities.begin(), chunkEntities.end());
      }
   }

   void uploadEntities() {
      visibleEntities.clear();
      visibleEntities.reserve(dynamicEntities.size() + staticEntities.size());
      visibleEntities.insert(visibleEntities.end(), dynamicEntities.begin(), dynamicEntities.end());
      visibleEntities.insert(visibleEntities.end(), staticEntities.begin(), staticEntities.end());
      renderAdapter.uploadEntities(visibleEntities);
      uploadedEntityCount = static_cast<uint32_t>(std::min<size_t>(visibleEntities.size(), spriteCapacity));
   }

   [[nodiscard]] std::optional<float> sampleSmoothedHeight(const glm::vec2 worldPos) const {
      const glm::ivec2 worldTile = static_cast<glm::ivec2>(glm::floor(worldPos));

      float heights[9];
      float centerSoftness = 0.0f;

      const glm::ivec2 offsets[9] = {{0, 0}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}};

      for (int i = 0; i < 9; ++i) {
         const glm::ivec2 nTile = worldTile + offsets[i];
         const glm::ivec2 chunkPos = toChunkCoord(nTile);
         const auto it = chunks.find(chunkPos);
         if (it == chunks.end()) {
            return std::nullopt;
         }
         const glm::ivec2 local = nTile - chunkPos * Chunk::SIZE;
         heights[i] = it->second->heightAt(local.x, local.y);

         if (i == 0) {
            const TileID id = it->second->terrainAt(local.x, local.y);
            centerSoftness = registry.get(id).softness;
         }
      }

      const float centerH = heights[0];
      if (centerH <= 0.01f) {
         return 0.0f;
      }

      float softness = centerSoftness;
      if (softness < 0.001f) {
         softness = 0.02f;
      }
      softness = std::min(softness, 0.5f);

      const glm::vec2 uv = worldPos - glm::vec2(worldTile);
      if (uv.x >= softness && (1.0f - uv.x) >= softness && uv.y >= softness && (1.0f - uv.y) >= softness) {
         return centerH;
      }

      const float hL = heights[6];
      const float hR = heights[2];
      const float hU = heights[4];
      const float hD = heights[8];

      const float eL = std::min(centerH, hL);
      const float eR = std::min(centerH, hR);
      const float eU = std::min(centerH, hU);
      const float eD = std::min(centerH, hD);

      const float cUL = std::min(std::min(centerH, hL), std::min(hU, heights[5]));
      const float cUR = std::min(std::min(centerH, hR), std::min(hU, heights[3]));
      const float cDL = std::min(std::min(centerH, hL), std::min(hD, heights[7]));
      const float cDR = std::min(std::min(centerH, hR), std::min(hD, heights[1]));

      auto smoothstep = [](const float edge0, const float edge1, const float x) {
         const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
         return t * t * (3.0f - 2.0f * t);
      };

      const float aL = smoothstep(1.0f, 0.0f, std::clamp(uv.x / softness, 0.0f, 1.0f));
      const float aR = smoothstep(1.0f, 0.0f, std::clamp((1.0f - uv.x) / softness, 0.0f, 1.0f));
      const float aU = smoothstep(1.0f, 0.0f, std::clamp(uv.y / softness, 0.0f, 1.0f));
      const float aD = smoothstep(1.0f, 0.0f, std::clamp((1.0f - uv.y) / softness, 0.0f, 1.0f));

      const float mX = 1.0f - aL - aR;
      const float mY = 1.0f - aU - aD;

      return cUL * aL * aU + cUR * aR * aU + cDL * aL * aD + cDR * aR * aD + eL * aL * mY + eR * aR * mY + eU * aU * mX + eD * aD * mX + centerH * mX * mY;
   }

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
      case EditTool::TileBrush:   chunk.setTerrain(local.x, local.y, brush.tile, brush.paintHeight); break;
      case EditTool::HeightBrush: chunk.setTerrain(local.x, local.y, id, std::clamp(chunk.heightAt(local.x, local.y) + heightStep, 0.0f, 2.0f)); break;
      case EditTool::Trimmer:     {
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
            staticDirty = true;

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

   std::vector<Entity> staticEntities;
   std::vector<Entity> dynamicEntities;
   std::vector<Entity> visibleEntities;
   uint32_t uploadedEntityCount = 0;
   bool staticDirty = false;
   bool entitiesDirty = true;

   uint32_t loadingRadius = 0;
   uint32_t unloadingThreshold = 0;
   Threadpool& threadPool;
   TileRegistry& registry;
   const EntityRegistry& entityRegistry;
   WorldGenerator& worldGenerator;
   WorldRenderAdapter& renderAdapter;
};
