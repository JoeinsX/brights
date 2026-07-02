#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct UniformData {
   glm::ivec2 macroOffset;
   glm::vec2 offset;
   glm::vec2 centerOffset;
   glm::vec2 res;
   float scale;
   float sphereMapScale;
   glm::ivec2 chunkOffset;
   glm::vec2 resScale;
   float perspectiveStrength;
   float perspectiveScale;
   float planetRadius;
   float planetDepth;
   float simpleModeThreshold;
   int32_t raymarchMaxTiles;
   int32_t raymarchBinarySteps;
   float _pad;
};

// must match uniforms in assets/shaders/common/uniforms.wgsl
static_assert(sizeof(UniformData) == 88);

namespace ShaderSlots {
   constexpr uint32_t Uniforms = 0;
   constexpr uint32_t TileMap = 1;
   constexpr uint32_t TextureAtlas = 2;
   constexpr uint32_t Sampler = 3;
   constexpr uint32_t PackedMap = 4;
   constexpr uint32_t Num = 5;
}   // namespace ShaderSlots

namespace SpriteSlots {
   constexpr uint32_t Uniforms = 0;
   constexpr uint32_t Instances = 1;
   constexpr uint32_t TextureAtlas = 2;
   constexpr uint32_t Sampler = 3;
   constexpr uint32_t Num = 4;
}   // namespace SpriteSlots
