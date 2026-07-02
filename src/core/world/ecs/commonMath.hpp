#pragma once

#include <cmath>
#include <glm/glm.hpp>

inline float facingAngle(const glm::vec2 direction) {
   return std::atan2(-direction.x, -direction.y);
}
