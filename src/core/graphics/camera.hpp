#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <iostream>

class Camera {
public:
   void pan(const glm::vec2 deltaOffset) { offset -= deltaOffset / scale; }

   void zoom(const float scrollOffset, const glm::dvec2 mousePosScreen, const glm::ivec2 screenSize) {
      const glm::vec2 halfScreenSize = static_cast<glm::vec2>(screenSize) * 0.5f;

      const glm::vec2 mousePosWorld = (static_cast<glm::vec2>(mousePosScreen) - halfScreenSize) / scale + offset;

      static constexpr float zoomFactor = 1.1f;
      if (scrollOffset > 0) {
         scale *= zoomFactor;
      } else {
         scale /= zoomFactor;
      }

      scale = std::clamp(scale, 0.1f, 86.0f);
      offset = mousePosWorld - (static_cast<glm::vec2>(mousePosScreen) - halfScreenSize) / scale;
   }

   void zoomCentered(const float scrollOffset) {
      static constexpr float zoomFactor = 1.1f;
      if (scrollOffset > 0) {
         scale *= zoomFactor;
      } else {
         scale /= zoomFactor;
      }
      scale = std::clamp(scale, 0.1f, 86.0f);
   }

   [[nodiscard]] glm::vec2 getOffset() const { return offset; }

   void setOffset(const glm::vec2 newOffset) { offset = newOffset; }

   [[nodiscard]] float getScale() const { return scale; }

   void setScale(const float newScale) { scale = std::clamp(newScale, 0.1f, 86.0f); }

private:
   glm::vec2 offset{};
   float scale = 4.0f;
};
