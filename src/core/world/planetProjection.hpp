#pragma once

#include "core/graphics/camera.hpp"
#include "core/world/chunk.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <optional>

struct PlanetProjection {
   static constexpr float mapTileSpan = static_cast<float>(Chunk::SIZE * Chunk::COUNT);
   static constexpr float sphereTileCoverage = static_cast<float>(Chunk::COUNT - 2) / static_cast<float>(Chunk::COUNT);
   static constexpr float centerTileStretch = 0.25f * mapTileSpan * sphereTileCoverage;

   float planetRadius = 0.0f;

   [[nodiscard]] float pixelsPerTile(const float cameraScale) const { return cameraScale * planetRadius / centerTileStretch; }

   [[nodiscard]] float focusScaleForPixelsPerTile(const float targetPixelsPerTile) const { return targetPixelsPerTile * centerTileStretch / planetRadius; }

   // copies shader projection from lib/sphere.wgsl
   [[nodiscard]] std::optional<glm::vec2> pickWorld(const glm::vec2 screenPos, const glm::ivec2 windowSize, const Camera& globalCamera, const glm::vec2 planetPos,
                                                    const glm::vec2 localOffset, const glm::ivec2 chunkMove) const {
      const glm::vec2 res = static_cast<glm::vec2>(windowSize);
      const glm::vec2 globalDiff = globalCamera.getOffset() - planetPos;
      const float scale = globalCamera.getScale();

      const glm::vec2 disk = (screenPos - 0.5f * res + globalDiff * scale) / (scale * planetRadius);
      const float distSq = glm::dot(disk, disk);
      if (distSq > 1.0f) {
         return std::nullopt;
      }

      const float z = std::sqrt(1.0f - distSq);
      const glm::vec2 diskUv = disk / (1.0f + z) * 0.5f;
      const glm::vec2 worldPos = localOffset + diskUv * mapTileSpan * sphereTileCoverage;
      return worldPos + glm::vec2(chunkMove * Chunk::SIZE);
   }

   [[nodiscard]] std::optional<glm::ivec2> pickTile(const glm::vec2 screenPos, const glm::ivec2 windowSize, const Camera& globalCamera, const glm::vec2 planetPos,
                                                    const glm::vec2 localOffset, const glm::ivec2 chunkMove) const {
      const std::optional<glm::vec2> world = pickWorld(screenPos, windowSize, globalCamera, planetPos, localOffset, chunkMove);
      return world ? std::optional<glm::ivec2>(static_cast<glm::ivec2>(glm::floor(*world))) : std::nullopt;
   }
};
