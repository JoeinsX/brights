#pragma once

enum class TileID : uint8_t {
    Air = 0,
    Grass,
    Water,
    ColdGrass,
    Stone,
    HardStone,
    Gravel,
    HardGravel,
    Snow,
    Ice,
    Planks,
    PlankFloor,
    RedOre,
    BlueOre,
    ColdWater,
    BurntGround,
    Sand
};

struct TileDefinition {
    int atlasBaseX = 0;
    int atlasBaseY = 0;
    int variationCount = 1;
    float height = 0.5f;
    float softness = 0.5f;
};

class TileRegistry {
public:
    void Register(TileID id, int x, int y, int variations,
                  float height = 0.5f,
                  float softness = 0.5f) {
        defs[id] = {x, y, variations, height, softness};
    }

    const TileDefinition* Get(TileID id) const {
        auto it = defs.find(id);
        if(it != defs.end()) return &it->second;
        return nullptr;
    }

private:
    std::unordered_map<TileID, TileDefinition> defs{};
};