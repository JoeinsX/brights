#pragma once
#include "chunk.hpp"

#include <webgpu/webgpu.hpp>

class World {
public:
    explicit World(TileRegistry& registry, std::mt19937 rng, WorldGenerator& worldGenerator, uint32_t loadingRadius,
                   uint32_t unloadingThreshold)
        : loadingRadius(loadingRadius), unloadingThreshold(unloadingThreshold), registry(registry),
          worldGenerator(worldGenerator),
          rng(rng) {
    }

    Chunk* getChunk(int x, int y) const {
        glm::ivec2 chunkPos{x, y};
        if(!chunks.contains(chunkPos)) {
            return nullptr;
        }
        return chunks.at(chunkPos);
    }

    void update(const Camera& camera, const glm::ivec2& globalChunkMove) {
        static constexpr uint32_t maxGeneratedChunksPerUpdate = 128;
        glm::ivec2 cameraChunkPos = static_cast<glm::ivec2>(camera.getOffset()) / Chunk::SIZE + globalChunkMove;
        for(auto it = chunks.begin(); it != chunks.end();) {
            glm::ivec2 bl = globalChunkMove - loadingRadius - static_cast<int32_t>(unloadingThreshold);
            glm::ivec2 ur = globalChunkMove + loadingRadius + static_cast<int32_t>(unloadingThreshold);
            glm::ivec2 cp = it->second->pos;

            if(cp.x < bl.x || cp.x >= ur.x || cp.y < bl.y || cp.y >= ur.y) {
                delete it->second;
                it = chunks.erase(it);
            } else {
                ++it;
            }
        }

        for(int x = -loadingRadius; x < loadingRadius; ++x) {
            for(int y = -loadingRadius; y < loadingRadius; ++y) {
                glm::ivec2 chunkPos = glm::ivec2{x, y} + cameraChunkPos;
                if(!chunks.contains(chunkPos)) {
                    Chunk* chunk = chunks.emplace(chunkPos, new Chunk{chunkPos}).first->second;
                    worldGenerator.generate(*chunk, 0);
                    chunk->rebuildDisplayMap(registry, rng);
                }
            }
        }
    }

private:
    std::unordered_map<glm::ivec2, Chunk*> chunks;
    int32_t loadingRadius = 0;
    uint32_t unloadingThreshold = 0;
    TileRegistry& registry;
    WorldGenerator& worldGenerator;
    std::mt19937 rng;
};

class WorldRenderAdapter {
public:
    WorldRenderAdapter(wgpu::Queue queue, wgpu::Buffer chunkOffsetBuffer, wgpu::Buffer chunkDataBuffer,
                       wgpu::Buffer tilemapBuffer, uint32_t renderRadius, World& world,
                       Camera& camera)
        : queue(queue), chunkOffsetBuffer(chunkOffsetBuffer), chunkDataBuffer(chunkDataBuffer),
          tilemapBuffer(tilemapBuffer), renderRadius(renderRadius), world(world) {
    }

    void update(Camera& camera, glm::ivec2& globalChunkMove) {
        static constexpr uint32_t maxUploadedChunksPerUpdate = 128;
        glm::ivec2 mapCenter{Chunk::SIZE * Chunk::COUNT / 2};

        glm::ivec2 camPosOffset = static_cast<glm::ivec2>(camera.getOffset()) - mapCenter;
        glm::ivec2 chunkMove = camPosOffset / Chunk::SIZE;

        if(chunkMove == glm::ivec2(0)) return;

        globalChunkMove += chunkMove;

        camera.setOffset(camera.getOffset() - glm::vec2(chunkMove * Chunk::SIZE));
        for(int y = 0; y < renderRadius * 2; ++y) {
            for(int x = 0; x < renderRadius * 2; ++x) {
                int x1 = (x + globalChunkMove.x) & (Chunk::COUNT - 1);

                int y1 = (y + globalChunkMove.y) & (Chunk::COUNT - 1);

                Chunk* chunk = world.getChunk(x + globalChunkMove.x, y + globalChunkMove.y);

                uint32_t idx = y * renderRadius * 2 + x;
                uint32_t idx1 = y1 * renderRadius * 2 + x1;
                uint32_t offset = idx1 * Chunk::SIZE * Chunk::SIZE;

                if(!chunk) {
                    uint32_t newOffset = 0;
                    queue.writeBuffer(chunkOffsetBuffer, idx1 * sizeof(uint32_t), &newOffset,
                                      sizeof(uint32_t));
                    continue;
                }

                if(chunk->needsReloading) {
                    queue.writeBuffer(chunkDataBuffer, chunk->getPackedData().size() * sizeof(uint16_t) * idx1,
                                      chunk->getPackedData().data(),
                                      chunk->getPackedData().size() * sizeof(uint16_t));

                    queue.writeBuffer(tilemapBuffer, chunk->getDisplayData().size() * sizeof(uint8_t) * idx1,
                                      chunk->getDisplayData().data(),
                                      chunk->getDisplayData().size() * sizeof(uint8_t));
                    //chunk->needsReloading = false;
                }

                queue.writeBuffer(chunkOffsetBuffer, idx1 * sizeof(uint32_t), &offset,
                                  sizeof(uint32_t));
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
