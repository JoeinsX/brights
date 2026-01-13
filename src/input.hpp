#pragma once

#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <map>

class Input {
public:
   void reset() {
      mouseDelta = {0.0f, 0.0f};
      scrollDelta = {0.0f, 0.0f};
   }

   bool isKeyDown(const int key) const {
      const auto it = keyStates.find(key);
      return it != keyStates.end() && it->second;
   }

   bool isDragging() const { return dragging; }

   glm::vec2 getMousePosition() const { return mousePos; }

   glm::vec2 getMouseDelta() const { return mouseDelta; }

   glm::vec2 getScrollDelta() const { return scrollDelta; }

   void onKey(int key, int action) {
      if (action == GLFW_PRESS) {
         keyStates[key] = true;
      } else if (action == GLFW_RELEASE) {
         keyStates[key] = false;
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
      glm::vec2 currentPos = {static_cast<float>(xpos), static_cast<float>(ypos)};
      mousePos = currentPos;

      if (dragging) {
         mouseDelta += (currentPos - lastMousePos);
         lastMousePos = currentPos;
      } else {
         lastMousePos = currentPos;
      }
   }

   void onScroll(double xoffset, double yoffset) { scrollDelta += glm::vec2(static_cast<float>(xoffset), static_cast<float>(yoffset)); }

private:
   std::map<int, bool> keyStates;
   glm::vec2 mousePos{0.0f};
   glm::vec2 lastMousePos{0.0f};
   glm::vec2 mouseDelta{0.0f};
   glm::vec2 scrollDelta{0.0f};
   bool dragging = false;
};
