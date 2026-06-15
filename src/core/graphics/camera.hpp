#pragma once

#include <glm/glm.hpp>

class Camera {
public:
   void pan(const glm::vec2 deltaOffset) { offset -= deltaOffset / scale; }

   [[nodiscard]] glm::vec2 getOffset() const { return offset; }

   void setOffset(const glm::vec2 newOffset) { offset = newOffset; }

   [[nodiscard]] float getScale() const { return scale; }

   void setScale(const float newScale) { scale = newScale; }

private:
   glm::vec2 offset{};
   float scale = 4.0f;
};
