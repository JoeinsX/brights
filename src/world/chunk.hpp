#pragma once
#include "tile.hpp"

class Chunk {
public:
    static constexpr int SIZE = 64;
    static constexpr int COUNT = 32;
    static constexpr int COUNT_SQUARED = COUNT * COUNT;

    Chunk(glm::ivec2 pos)
        : pos(pos) {
        terrainMap.resize(SIZE * SIZE, TileID::Water);
        wallMap.resize(SIZE * SIZE, TileID::Air);
        displayMap.resize(SIZE * SIZE, 0);
        heightMap.resize(SIZE * SIZE, 0.0f);
        softnessMap.resize(SIZE * SIZE * 2, 0.0f);
        packedMap.resize(SIZE * SIZE, 0);
    }

    void setTerrain(const int x, const int y, TileID id) {
        if(x < 0 || x >= SIZE || y < 0 || y >= SIZE) return;
        terrainMap[y * SIZE + x] = id;
    }

    void setWall(int x, int y, TileID id) {
        if(x < 0 || x >= SIZE || y < 0 || y >= SIZE) return;
        wallMap[y * SIZE + x] = id;
    }

    static glm::vec2 getAtlasCoords(const TileRegistry& registry,
                                    TileID id, std::mt19937& rng) {
        if(id == TileID::Air) return {-1.0f, -1.0f};

        const TileDefinition* def = registry.get(id);
        if(!def) return {-1.0f, -1.0f};

        auto atlasBase = def->atlasBase;

        if(def->variationCount > 1) {
            std::uniform_int_distribution<int> dist(0, def->variationCount - 1);
            atlasBase.y += static_cast<float>(dist(rng));
        }

        return static_cast<glm::vec2>(atlasBase);
    }

    void rebuildDisplayMap(const TileRegistry& registry, std::mt19937& rng) {
        for(int y = 0; y < SIZE; ++y) {
            for(int x = 0; x < SIZE; ++x) {
                int index = y * SIZE + x;
                TileID tID = terrainMap[index];
                TileID wID = wallMap[index];

                auto terrainCoords = getAtlasCoords(registry, tID, rng);
                auto wallCoords = getAtlasCoords(registry, wID, rng);

                uint8_t tx = static_cast<uint8_t>(std::clamp(terrainCoords.x, 0.0f, 15.0f));
                uint8_t ty = static_cast<uint8_t>(std::clamp(terrainCoords.y, 0.0f, 15.0f));

                displayMap[index] = (tx << 4) | (ty & 0x0F);

                const TileDefinition* def = registry.get(tID);
                heightMap[index] = def ? def->height : 0.0f;
            }
        }

        prebuildSoftnessMap(registry);
    }

    void prebuildSoftnessMap(const TileRegistry& registry) {
        const int allNeighbors[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},
            {-1, 0}, {1, 0},
            {-1, 1}, {0, 1}, {1, 1}
        };

        const int edgeOffsets[4][2] = {
            {0, -1}, {-1, 0}, {1, 0}, {0, 1}
        };
        const int cornerOffsets[4][2] = {
            {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
        };

        for(int y = 0; y < SIZE; ++y) {
            for(int x = 0; x < SIZE; ++x) {
                int index = y * SIZE + x;

                TileID tID = terrainMap[index];
                const TileDefinition* def = registry.get(tID);
                float defaultSoftness = def ? def->softness : 0.0f;
                float currentHeight = heightMap[index];

                softnessMap[index * 2] = defaultSoftness;

                if(defaultSoftness > 0.0001f) {
                    bool shouldZeroOut = true;
                    for(const auto& offset : allNeighbors) {
                        int nx = x + offset[0];
                        int ny = y + offset[1];
                        if(nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE) {
                            int nIndex = ny * SIZE + nx;
                            float nHeight = heightMap[nIndex];
                            TileID nID = terrainMap[nIndex];
                            const TileDefinition* nDef = registry.get(nID);
                            float nSoftness = nDef ? nDef->softness : 0.0f;

                            bool isSameHeight = std::abs(
                                                    nHeight - currentHeight) <
                                                0.0001f;
                            bool isHigherAndHard =
                                (nHeight > currentHeight) && (
                                    nSoftness < 0.0001f);

                            if(!isSameHeight && !isHigherAndHard) {
                                shouldZeroOut = false;
                                break;
                            }
                        }
                    }
                    if(shouldZeroOut) {
                        softnessMap[index * 2] = 0.0f;
                    }
                }

                int lowerEdgeCount = 0;
                for(const auto& offset : edgeOffsets) {
                    int nx = x + offset[0];
                    int ny = y + offset[1];
                    if(nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE) {
                        float nHeight = heightMap[ny * SIZE + nx];
                        if(nHeight < currentHeight - 0.0001f) lowerEdgeCount++;
                    }
                }

                int lowerCornerCount = 0;
                if(lowerEdgeCount == 1) {
                    for(const auto& offset : cornerOffsets) {
                        int nx = x + offset[0];
                        int ny = y + offset[1];
                        if(nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE) {
                            float nHeight = heightMap[ny * SIZE + nx];
                            if(nHeight < currentHeight - 0.0001f)
                                lowerCornerCount++;
                        }
                    }
                }

                if(lowerEdgeCount <= 1 && lowerCornerCount == 0) {
                    softnessMap[index * 2 + 1] = 0.0f;
                } else {
                    softnessMap[index * 2 + 1] = 1.0f;
                }

                uint16_t h = static_cast<uint16_t>(std::clamp(currentHeight * 127.5f, 0.0f, 255.f));
                uint16_t s = static_cast<uint16_t>(std::clamp(softnessMap[index * 2] * 127.0f, 0.0f, 127.0f));
                uint16_t t = (softnessMap[index * 2 + 1] > 0.5f) ? 1 : 0;

                packedMap[index] = (h << 8) | (s << 1) | t;
            }
        }
    }

    [[nodiscard]] const std::vector<uint8_t>& getDisplayData() const {
        return displayMap;
    }

    [[nodiscard]] const std::vector<float>& getHeightData() const {
        return heightMap;
    }

    [[nodiscard]] const std::vector<float>& getSoftnessData() const {
        return softnessMap;
    }

    [[nodiscard]] const std::vector<uint16_t>& getPackedData() const {
        return packedMap;
    }

    static int getSize() { return SIZE; }

    //private:
    std::vector<TileID> terrainMap;
    std::vector<TileID> wallMap;
    std::vector<uint8_t> displayMap;
    std::vector<float> heightMap;
    std::vector<float> softnessMap;

    std::vector<uint16_t> packedMap;

    glm::ivec2 pos{};

    bool needsReloading = true;
};
