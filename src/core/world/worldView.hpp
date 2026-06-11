#pragma once

#include "planet.hpp"
#include "platform/input.hpp"
class WorldView {
private:
   enum class Mode : uint8_t {
      Free,
      Transitioning,
      Locked
   };

   struct CameraState {
      glm::vec2 offset{0.0f};
      float scale{1.0f};
   };

public:
   struct Config {
      float minScale = 0.1f;
      float maxScale = 86.0f;
      float zoomStep = 1.1f;
      float baseLerpSpeed = 8.0f;
      float focusDurationMs = 900.0f;
   };

   WorldView() {
      targetState.scale = savedGlobalScale;
      targetState.offset = galaxyCamera.getOffset();
      syncCameraToCurrent();
   }

   void handleInput(const Input& input, const std::vector<std::unique_ptr<Planet>>& planets, glm::ivec2 windowSize) {
      if (input.isKeyDown(GLFW_KEY_TAB)) {
         if (!tabWasDown) {
            toggleFocusMode(planets);
         }
         tabWasDown = true;
      } else {
         tabWasDown = false;
      }

      if (input.isDragging()) {
         applyDrag(input.getMouseDelta(), planets);
      }

      const auto keyInput = glm::vec2(static_cast<float>(input.isKeyDown(GLFW_KEY_D)) - static_cast<float>(input.isKeyDown(GLFW_KEY_A)),
                                      static_cast<float>(input.isKeyDown(GLFW_KEY_S)) - static_cast<float>(input.isKeyDown(GLFW_KEY_W)));

      if (glm::length(keyInput) > 0.1f) {
         applyKeyboardPan(glm::normalize(keyInput) * 10.0f, planets);
      }

      const float scrollY = input.getScrollDelta().y;
      if (scrollY != 0.0f) {
         const float zoomDir = (scrollY > 0) ? 1.0f : -1.0f;
         applyZoom(zoomDir, input.getMousePosition(), windowSize);
      }
   }

   void update(float dt, const std::vector<std::unique_ptr<Planet>>& planets) {
      if (mode != Mode::Free && focusedPlanetIndex < planets.size()) {
         targetState.offset = planets[focusedPlanetIndex]->getConfig().position;
      }

      if (mode == Mode::Locked) {
         currentState = targetState;
      } else if (mode == Mode::Transitioning) {
         transitionT = std::min(transitionT + dt / config.focusDurationMs, 1.0f);
         const float ease = glm::smoothstep(0.0f, 1.0f, transitionT);

         currentState.offset = glm::mix(transitionStart.offset, targetState.offset, ease);
         currentState.scale = std::exp(glm::mix(std::log(transitionStart.scale), std::log(targetState.scale), ease));

         if (transitionT >= 1.0f) {
            mode = Mode::Locked;
            currentState = targetState;
         }
      } else {
         const float lerpFactor = 1.0f - std::exp(-config.baseLerpSpeed * dt / 1000.0f);
         currentState.offset = glm::mix(currentState.offset, targetState.offset, lerpFactor);
         currentState.scale = glm::mix(currentState.scale, targetState.scale, lerpFactor);
      }

      galaxyCamera.setOffset(currentState.offset);
      galaxyCamera.setScale(currentState.scale);
   }

   [[nodiscard]] Camera& getCamera() { return galaxyCamera; }
   [[nodiscard]] const Camera& getCamera() const { return galaxyCamera; }
   [[nodiscard]] int getFocusedPlanetIndex() const { return static_cast<int>(focusedPlanetIndex); }

private:
   Camera galaxyCamera;
   Config config;
   Mode mode = Mode::Free;

   CameraState currentState;
   CameraState targetState;
   CameraState transitionStart;

   size_t focusedPlanetIndex = -1;
   bool tabWasDown = false;

   float savedGlobalScale = 0.5f;
   float savedPlanetScale = 1.0f;

   float transitionT = 0.0f;

   void syncCameraToCurrent() {
      galaxyCamera.setOffset(currentState.offset);
      galaxyCamera.setScale(currentState.scale);
   }

   void toggleFocusMode(const std::vector<std::unique_ptr<Planet>>& planets) {
      if (mode != Mode::Free) {
         savedPlanetScale = targetState.scale;
         mode = Mode::Free;
         focusedPlanetIndex = -1;

         targetState.scale = std::min(savedGlobalScale, targetState.scale);
      } else {
         float minDist = std::numeric_limits<float>::max();
         int closest = -1;

         for (size_t i = 0; i < planets.size(); ++i) {
            const float dist = glm::distance(currentState.offset, planets[i]->getConfig().position);
            if (dist < minDist) {
               minDist = dist;
               closest = static_cast<int>(i);
            }
         }

         if (closest != -1) {
            savedGlobalScale = targetState.scale;
            mode = Mode::Transitioning;
            focusedPlanetIndex = closest;

            transitionStart = currentState;
            transitionT = 0.0f;

            targetState.scale = std::max(savedPlanetScale, targetState.scale);
         }
      }
   }

   void applyDrag(glm::vec2 deltaPixels, const std::vector<std::unique_ptr<Planet>>& planets) {
      if (mode == Mode::Locked && focusedPlanetIndex < planets.size()) {
         auto& planet = planets[focusedPlanetIndex];
         planet->localCamera.setScale(galaxyCamera.getScale());
         planet->localCamera.pan(deltaPixels);
      } else {
         targetState.offset -= deltaPixels / currentState.scale;
      }
   }

   void applyKeyboardPan(glm::vec2 direction, const std::vector<std::unique_ptr<Planet>>& planets) {
      if (mode == Mode::Locked && focusedPlanetIndex < planets.size()) {
         auto& planet = planets[focusedPlanetIndex];
         planet->localCamera.setScale(galaxyCamera.getScale());
         planet->localCamera.pan(-direction);
      } else {
         targetState.offset += direction / currentState.scale;
      }
   }

   void applyZoom(float direction, glm::vec2 mousePos, glm::ivec2 windowSize) {
      const float zoomFactor = (direction > 0) ? config.zoomStep : (1.0f / config.zoomStep);
      const float newScale = std::clamp(targetState.scale * zoomFactor, config.minScale, config.maxScale);

      if (std::abs(newScale - targetState.scale) < 0.0001f) {
         return;
      }

      if (mode == Mode::Locked) {
         targetState.scale = newScale;
      } else {
         const glm::vec2 halfScreen = glm::vec2(windowSize) * 0.5f;
         const glm::vec2 mouseFromCenter = mousePos - halfScreen;

         targetState.offset += mouseFromCenter * (1.0f / targetState.scale - 1.0f / newScale);
         targetState.scale = newScale;
      }
   }
};
