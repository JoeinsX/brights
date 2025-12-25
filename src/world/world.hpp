#pragma once
#include "chunk.hpp"

#include <webgpu/webgpu.hpp>

class World {
public:
    explicit World(uint32_t size, TileRegistry& registry, std::mt19937 rng)
        : size(size), registry(registry), rng(rng) {
        for(int y = 0; y < size; ++y) {
            for(int x = 0; x < size; ++x) {
                glm::ivec2 chunkPos{x, y};
                Chunk* chunk = chunks.emplace(chunkPos, new Chunk{chunkPos}).first->second;
                WorldGenerator worldGenerator;
                worldGenerator.generate(*chunk, 0);
                chunk->rebuildDisplayMap(registry, rng);
            }
        }
    }

    Chunk* getChunk(int x, int y) const {
        glm::ivec2 chunkPos{x, y};
        if(!chunks.contains(chunkPos)) {
            return nullptr;
        }
        return chunks.at(chunkPos);
    }

    void update(const Camera& camera, const glm::ivec2& globalChunkMove) {
        glm::ivec2 cameraChunkPos = static_cast<glm::ivec2>(camera.getOffset()) / Chunk::SIZE + globalChunkMove;
        for(auto it = chunks.begin(); it != chunks.end(); ++it) {
            if(glm::distance(static_cast<glm::vec2>(cameraChunkPos), static_cast<glm::vec2>(it->second->pos)) >
               Chunk::COUNT / 2 + 3) {
                delete it->second;
                chunks.erase(it);
            }
        }
        for(int x = -Chunk::COUNT / 2; x < Chunk::COUNT / 2; ++x) {
            for(int y = -Chunk::COUNT / 2; y < Chunk::COUNT / 2; ++y) {
                glm::ivec2 chunkPos = glm::ivec2{x, y} + cameraChunkPos;
                if(glm::distance(static_cast<glm::vec2>(cameraChunkPos), static_cast<glm::vec2>(chunkPos)) <=
                   Chunk::COUNT / 2 + 2) {
                    if(!chunks.contains(chunkPos)) {
                        Chunk* chunk = chunks.emplace(chunkPos, new Chunk{chunkPos}).first->second;
                        WorldGenerator worldGenerator;
                        worldGenerator.generate(*chunk, 0);
                        chunk->rebuildDisplayMap(registry, rng);
                    }
                }
            }
        }
    }

    //private:
    std::unordered_map<glm::ivec2, Chunk*> chunks;
    uint32_t size = 0;
    TileRegistry& registry;
    std::mt19937 rng;
};

class WorldRenderAdapter {
public:
    WorldRenderAdapter(wgpu::Queue queue, wgpu::Buffer chunkOffsetBuffer, wgpu::Buffer chunkDataBuffer,
                       wgpu::Buffer tilemapBuffer, uint32_t size, World& world,
                       Camera& camera)
        : queue(queue), chunkOffsetBuffer(chunkOffsetBuffer), chunkDataBuffer(chunkDataBuffer),
          tilemapBuffer(tilemapBuffer), size(size), world(world) {
        size_t bufferOffset = 0;
        size_t bufferOffset1 = 0;
    }

    void update(Camera& camera, glm::ivec2& globalChunkMove) {
        glm::ivec2 mapCenter{Chunk::SIZE * Chunk::COUNT / 2};

        glm::ivec2 camPosOffset = static_cast<glm::ivec2>(camera.getOffset()) - mapCenter;
        glm::ivec2 chunkMove = camPosOffset / Chunk::SIZE;

        if(chunkMove == glm::ivec2(0)) return;

        globalChunkMove += chunkMove;

        camera.setOffset(camera.getOffset() - glm::vec2(chunkMove * Chunk::SIZE));
        for(int y = 0; y < size; ++y) {
            for(int x = 0; x < size; ++x) {
                int x1 = ((x + globalChunkMove.x) % Chunk::COUNT + Chunk::COUNT) % Chunk::COUNT;

                int y1 = ((y + globalChunkMove.y) % Chunk::COUNT + Chunk::COUNT) % Chunk::COUNT;

                Chunk* chunk = world.getChunk(x + globalChunkMove.x, y + globalChunkMove.y);

                uint32_t idx = y * size + x;
                uint32_t idx1 = y1 * size + x1;
                uint32_t offset = idx1 * Chunk::SIZE * Chunk::SIZE;

                if(!chunk) {
                    uint32_t newOffset = 0;
                    queue.writeBuffer(chunkOffsetBuffer, idx * sizeof(uint32_t), &newOffset,
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
                    chunk->needsReloading = false;
                }

                queue.writeBuffer(chunkOffsetBuffer, idx * sizeof(uint32_t), &offset,
                                  sizeof(uint32_t));
            }
        }
    }

    wgpu::Queue queue = nullptr;
    wgpu::Buffer chunkOffsetBuffer = nullptr;
    wgpu::Buffer chunkDataBuffer = nullptr;
    wgpu::Buffer tilemapBuffer = nullptr;

    World& world;

    uint32_t size = 0;
};
