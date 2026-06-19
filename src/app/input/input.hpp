#pragma once

#include "event.hpp"

#include <array>
#include <cstddef>
#include <glm/glm.hpp>

class Input {
public:
   void reset() {
      mouseDelta = {0.0f, 0.0f};
      scrollDelta = {0.0f, 0.0f};
      keysPressed.fill(false);
   }

   [[nodiscard]] bool isKeyDown(const Key key) const { return key != Key::Count && keysDown[index(key)]; }
   [[nodiscard]] bool isKeyPressed(const Key key) const { return key != Key::Count && keysPressed[index(key)]; }

   [[nodiscard]] bool isDragging() const { return dragging; }
   [[nodiscard]] glm::vec2 getMousePosition() const { return mousePos; }
   [[nodiscard]] glm::vec2 getMouseDelta() const { return mouseDelta; }
   [[nodiscard]] glm::vec2 getScrollDelta() const { return scrollDelta; }

   void onKey(const Key key, const bool pressed) {
      if (key == Key::Count) {
         return;
      }
      keysDown[index(key)] = pressed;
      if (pressed) {
         keysPressed[index(key)] = true;
      }
   }

   void onMouseButton(const MouseButton button, const bool pressed, const glm::vec2 position) {
      if (button == MouseButton::Left) {
         dragging = pressed;
         if (pressed) {
            lastMousePos = position;
         }
      }
   }

   void onMouseMove(const glm::vec2 position) {
      mousePos = position;
      if (dragging) {
         mouseDelta += (position - lastMousePos);
      }
      lastMousePos = position;
   }

   void onScroll(const glm::vec2 delta) { scrollDelta += delta; }

private:
   static constexpr size_t index(const Key key) { return static_cast<size_t>(key); }

   std::array<bool, static_cast<size_t>(Key::Count)> keysDown{};
   std::array<bool, static_cast<size_t>(Key::Count)> keysPressed{};

   glm::vec2 mousePos{0.0f};
   glm::vec2 lastMousePos{0.0f};
   glm::vec2 mouseDelta{0.0f};
   glm::vec2 scrollDelta{0.0f};
   bool dragging = false;
};
