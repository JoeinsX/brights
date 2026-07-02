#pragma once

#include "core/world/chunk.hpp"
#include "core/world/contents/tile.hpp"

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <optional>
#include <unordered_map>

// copies reconstructHeight from assets/shaders/terrain/heightfield.wgsl
struct HeightField {
   const std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>>& chunks;
   const TileRegistry& registry;

   [[nodiscard]] std::optional<float> sampleAt(const glm::vec2 worldPos) const {
      const glm::ivec2 worldTile = static_cast<glm::ivec2>(glm::floor(worldPos));

      static constexpr std::array<glm::ivec2, 9> offsets{{{0, 0}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}}};

      std::array<float, 9> heights{};
      float centerSoftness = 0.0f;

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

      const float aL = glm::smoothstep(1.0f, 0.0f, std::clamp(uv.x / softness, 0.0f, 1.0f));
      const float aR = glm::smoothstep(1.0f, 0.0f, std::clamp((1.0f - uv.x) / softness, 0.0f, 1.0f));
      const float aU = glm::smoothstep(1.0f, 0.0f, std::clamp(uv.y / softness, 0.0f, 1.0f));
      const float aD = glm::smoothstep(1.0f, 0.0f, std::clamp((1.0f - uv.y) / softness, 0.0f, 1.0f));

      const float mX = 1.0f - aL - aR;
      const float mY = 1.0f - aU - aD;

      return cUL * aL * aU + cUR * aR * aU + cDL * aL * aD + cDR * aR * aD + eL * aL * mY + eR * aR * mY + eU * aU * mX + eD * aD * mX + centerH * mX * mY;
   }
};
