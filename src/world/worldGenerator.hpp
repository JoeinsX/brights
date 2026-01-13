#pragma once

#include "FastNoise/FastNoise.h"
#include "chunk.hpp"

#include <cmath>
#include <glm/gtx/hash.hpp>
#include <mutex>
#include <vector>

class WorldGenerator {
private:
   struct NoiseContext {
      FastNoise::SmartNode<> elevation;
      FastNoise::SmartNode<> river;
      FastNoise::SmartNode<> temperature;
      FastNoise::SmartNode<> moisture;
      FastNoise::SmartNode<> ore;
      FastNoise::SmartNode<> trees;

      NoiseContext() {
         auto fnBaseElev = FastNoise::New<FastNoise::FractalRidged>();
         fnBaseElev->SetSource(FastNoise::New<FastNoise::Simplex>());
         fnBaseElev->SetOctaveCount(5);
         fnBaseElev->SetGain(0.5f);
         fnBaseElev->SetLacunarity(2.0f);

         auto warpElev = FastNoise::New<FastNoise::DomainWarpGradient>();
         warpElev->SetSource(fnBaseElev);
         warpElev->SetWarpAmplitude(40.0f);
         warpElev->SetWarpFrequency(0.005f);
         elevation = warpElev;

         auto fnBaseRiver = FastNoise::New<FastNoise::FractalRidged>();
         fnBaseRiver->SetSource(FastNoise::New<FastNoise::Simplex>());
         fnBaseRiver->SetOctaveCount(3);

         auto warpRiver = FastNoise::New<FastNoise::DomainWarpGradient>();
         warpRiver->SetSource(fnBaseRiver);
         warpRiver->SetWarpAmplitude(20.0f);
         warpRiver->SetWarpFrequency(0.005f);
         river = warpRiver;

         auto fnBaseTemp = FastNoise::New<FastNoise::Simplex>();

         auto warpTemp = FastNoise::New<FastNoise::DomainWarpGradient>();
         warpTemp->SetSource(fnBaseTemp);
         warpTemp->SetWarpAmplitude(10.0f);
         warpTemp->SetWarpFrequency(0.01f);
         temperature = warpTemp;

         auto fnBaseMoist = FastNoise::New<FastNoise::FractalFBm>();
         fnBaseMoist->SetSource(FastNoise::New<FastNoise::Simplex>());

         auto warpMoist = FastNoise::New<FastNoise::DomainWarpGradient>();
         warpMoist->SetSource(fnBaseMoist);
         warpMoist->SetWarpAmplitude(30.0f);
         warpMoist->SetWarpFrequency(0.005f);
         moisture = warpMoist;

         auto fnBaseOre = FastNoise::New<FastNoise::CellularValue>();
         fnBaseOre->SetJitterModifier(1.2f);
         ore = fnBaseOre;

         trees = FastNoise::New<FastNoise::White>();
      }
   };

   static const NoiseContext& getContext() {
      static NoiseContext ctx;
      return ctx;
   }

public:
   static void generateDefaultChunk(Chunk& chunk) {
      for (int y = 0; y < Chunk::SIZE; ++y) {
         for (int x = 0; x < Chunk::SIZE; ++x) {
            chunk.setTerrain(x, y, TileID::Gravel);
         }
      }
   }

   static void generate(Chunk& chunk, int seed) {
      const auto& ctx = getContext();

      glm::ivec2 offset = chunk.getPos() * Chunk::SIZE;
      int size = Chunk::SIZE;
      int area = size * size;

      static thread_local std::vector<float> elevationMap(area);
      static thread_local std::vector<float> riverMap(area);
      static thread_local std::vector<float> tempMap(area);
      static thread_local std::vector<float> moistureMap(area);
      static thread_local std::vector<float> oreMap(area);
      static thread_local std::vector<float> treeMap(area);

      ctx.elevation->GenUniformGrid2D(elevationMap.data(), offset.x, offset.y, size, size, 0.004f, seed);
      ctx.river->GenUniformGrid2D(riverMap.data(), offset.x, offset.y, size, size, 0.005f, seed + 111);
      ctx.temperature->GenUniformGrid2D(tempMap.data(), offset.x, offset.y, size, size, 0.002f, seed + 1923);
      ctx.moisture->GenUniformGrid2D(moistureMap.data(), offset.x, offset.y, size, size, 0.003f, seed + 4821);
      ctx.ore->GenUniformGrid2D(oreMap.data(), offset.x, offset.y, size, size, 0.05f, seed + 9991);
      ctx.trees->GenUniformGrid2D(treeMap.data(), offset.x, offset.y, size, size, 1.0f, seed + 555);

      for (int y = 0; y < size; ++y) {
         for (int x = 0; x < size; ++x) {
            int idx = y * size + x;

            float h = elevationMap[idx];
            float t = tempMap[idx];
            float m = moistureMap[idx];
            float r = riverMap[idx];
            float o = oreMap[idx];
            float treeRng = treeMap[idx];

            float dither = treeRng * 0.05f;
            auto terrain = TileID::Water;

            bool isRiver = (h > -0.1f && h < 0.5f) && (r > 0.85f);

            if (h < -0.1f) {
               // Ocean
               if (t < -0.5f) {
                  terrain = TileID::Ice;
               } else if (t < 0.0f) {
                  terrain = TileID::ColdWater;
               } else {
                  terrain = TileID::Water;
               }
            } else if (isRiver) {
               // River cutting through land
               if (t < -0.5f) {
                  terrain = TileID::Ice;
               } else {
                  terrain = TileID::Water;
               }
            } else {
               // Beach
               if (h < 0.0f) {
                  terrain = TileID::Sand;
               } else if (h > 0.7f) {
                  // High Altitudes
                  if (h > 0.85f) {
                     terrain = TileID::HardStone;   // Peaks
                  } else {
                     terrain = TileID::Stone;
                  }

                  // Mountain Snow (with dither for ragged snow line)
                  if ((t + dither) < -0.2f || h > 0.9f) {
                     terrain = TileID::Snow;
                  }
               } else {
                  // Standard Biomes
                  if ((t + dither) < -0.3f) {
                     // Cold
                     terrain = TileID::Snow;
                  } else if ((t + dither) > 0.4f && (m + dither) < -0.2f) {
                     // Hot & Dry (Desert)
                     terrain = TileID::Sand;
                  } else if ((t + dither) > 0.4f && (m + dither) < 0.1f) {
                     // Hot & Semi-dry (Savanna/Burnt)
                     terrain = TileID::BurntGround;
                  } else {
                     // Temperate
                     if ((m + dither) < -0.3f) {
                        terrain = TileID::Gravel;   // Wasteland
                     } else if ((t + dither) < 0.1f) {
                        terrain = TileID::ColdGrass;
                     } else {
                        terrain = TileID::Grass;
                     }
                  }
               }

               bool isSoil = (terrain == TileID::Grass || terrain == TileID::ColdGrass);

               if (isSoil) {
                  // Trees
                  if (m > 0.2f && treeRng > 0.6f) {
                     terrain = TileID::Planks;   // Placeholder for Tree
                  }
                  // Dense Forest / Swamp
                  else if (m > 0.6f && treeRng > 0.3f) {
                     terrain = TileID::Planks;
                  }
                  // Cold bushes
                  else if (terrain == TileID::ColdGrass && treeRng > 0.8f) {
                     terrain = TileID::HardGravel;
                  }
               }
            }

            // Ores
            if (terrain == TileID::Stone || terrain == TileID::HardStone) {
               if (o > 0.8f) {
                  terrain = TileID::RedOre;
               } else if (o < -0.8f) {
                  terrain = TileID::BlueOre;
               }
            }

            chunk.setTerrain(x, y, terrain);
         }
      }
   }
};
