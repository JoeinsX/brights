#pragma once
#include "FastNoise/FastNoise.h"

class WorldGenerator {
public
:
    static void Generate(Chunk& chunk, int seed) {
        int size = Chunk::GetSize();

        // 1. Elevation Noise
        auto fnElevation = FastNoise::New<FastNoise::FractalFBm>();
        fnElevation->SetSource(FastNoise::New<FastNoise::Simplex>());
        fnElevation->SetOctaveCount(4);
        fnElevation->SetGain(0.5f);
        fnElevation->SetLacunarity(2.0f);

        // 2. Temperature Noise
        auto fnTemperature = FastNoise::New<FastNoise::Simplex>();

        // 3. Moisture/Vegetation Noise
        auto fnMoisture = FastNoise::New<FastNoise::FractalFBm>();
        fnMoisture->SetSource(FastNoise::New<FastNoise::Simplex>());
        fnMoisture->SetOctaveCount(2);

        // 4. Ore Noise
        auto fnOre = FastNoise::New<FastNoise::CellularValue>();
        fnOre->SetJitterModifier(1.0f);

        std::vector<float> elevationMap(size * size);
        std::vector<float> tempMap(size * size);
        std::vector<float> moistureMap(size * size);
        std::vector<float> oreMap(size * size);

        fnElevation->GenUniformGrid2D(elevationMap.data(), 0, 0, size, size,
                                      0.02f, seed);
        fnTemperature->GenUniformGrid2D(tempMap.data(), 0, 0, size, size, 0.01f,
                                        seed + 1923);
        fnMoisture->GenUniformGrid2D(moistureMap.data(), 0, 0, size, size,
                                     0.05f, seed + 4821);
        fnOre->GenUniformGrid2D(oreMap.data(), 0, 0, size, size, 0.2f,
                                seed + 9991);

        for(int y = 0; y < size; ++y) {
            for(int x = 0; x < size; ++x) {
                int idx = y * size + x;
                float h = elevationMap[idx];
                float t = tempMap[idx];
                float o = oreMap[idx];

                TileID terrain = TileID::Grass;
                TileID wall = TileID::Air;

                // Water Level
                if(h < -0.2f) {
                    if(t < -0.4f) terrain = TileID::Ice;
                    else if(t < 0.0f) terrain = TileID::ColdWater;
                    else terrain = TileID::Water;
                }
                // Beach / Lowlands
                else if(h < -0.1f) {
                    terrain = TileID::Gravel;
                }
                // Land
                else if(h < 0.6f) {
                    if(t < -0.3f) terrain = TileID::Snow;
                    else if(t < 0.2f) terrain = TileID::ColdGrass;
                    else terrain = TileID::Grass;
                }
                // Mountains
                else {
                    if(h > 0.8f) terrain = TileID::HardStone;
                    else terrain = TileID::Stone;

                    if(o > 0.85f) wall = TileID::RedOre;
                    else if(o < -0.85f) wall = TileID::BlueOre;
                    else wall = TileID::Planks;
                }

                chunk.SetTerrain(x, y, terrain);
                chunk.SetWall(x, y, wall);
            }
        }
    }
};