#pragma once

#include "GLFW/glfw3.h"

#include <array>
#include <glm/glm.hpp>

class Input {
public:
   void reset() {
      mouseDelta = {0.0f, 0.0f};
      scrollDelta = {0.0f, 0.0f};
      keysPressed.fill(false);
   }

   [[nodiscard]] bool isKeyDown(const int key) const { return isValidKey(key) && keysDown[key]; }

   [[nodiscard]] bool isKeyPressed(const int key) const { return isValidKey(key) && keysPressed[key]; }

   [[nodiscard]] bool isDragging() const { return dragging; }
   [[nodiscard]] glm::vec2 getMousePosition() const { return mousePos; }
   [[nodiscard]] glm::vec2 getMouseDelta() const { return mouseDelta; }
   [[nodiscard]] glm::vec2 getScrollDelta() const { return scrollDelta; }

   void onKey(const int key, const int action) {
      if (!isValidKey(key)) {
         return;
      }
      if (action == GLFW_PRESS) {
         keysDown[key] = true;
         keysPressed[key] = true;
      } else if (action == GLFW_RELEASE) {
         keysDown[key] = false;
      }
   }

   void onMouseButton(const int button, const int action, const double xpos, const double ypos) {
      if (button == GLFW_MOUSE_BUTTON_LEFT) {
         if (action == GLFW_PRESS) {
            dragging = true;
            lastMousePos = {static_cast<float>(xpos), static_cast<float>(ypos)};
         } else if (action == GLFW_RELEASE) {
            dragging = false;
         }
      }
   }

   void onCursorPos(double xpos, double ypos) {
      const glm::vec2 currentPos = {static_cast<float>(xpos), static_cast<float>(ypos)};
      mousePos = currentPos;

      if (dragging) {
         mouseDelta += (currentPos - lastMousePos);
      }
      lastMousePos = currentPos;
   }

   void onScroll(double xoffset, double yoffset) { scrollDelta += glm::vec2(static_cast<float>(xoffset), static_cast<float>(yoffset)); }

private:
   static bool isValidKey(const int key) { return key >= 0 && key <= GLFW_KEY_LAST; }

   std::array<bool, GLFW_KEY_LAST + 1> keysDown{};
   std::array<bool, GLFW_KEY_LAST + 1> keysPressed{};

   glm::vec2 mousePos{0.0f};
   glm::vec2 lastMousePos{0.0f};
   glm::vec2 mouseDelta{0.0f};
   glm::vec2 scrollDelta{0.0f};
   bool dragging = false;
};
