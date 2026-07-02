#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// decoded in sprite.wgsl and terrain/tilemap.wgsl
inline uint8_t packAtlasCell(const glm::ivec2 cell) {
   return static_cast<uint8_t>((cell.x & 0x0F) << 4 | (cell.y & 0x0F));
}

inline glm::ivec2 unpackAtlasCell(const uint32_t packed) {
   return {(packed >> 4) & 0x0F, packed & 0x0F};
}
