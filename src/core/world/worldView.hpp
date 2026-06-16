#pragma once

#include "planet.hpp"
#include "platform/input.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

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
      float minScale = 0.1f;   // galaxy-view zoom range
      float maxScale = 86.0f;
      float zoomStep = 1.1f;
      float baseLerpSpeed = 8.0f;
      float focusDurationMs = 900.0f;
      float focusMinPixelsPerTile = 1.5f;     // focused zoom-out limit: planet roughly fills the view
      float focusMaxPixelsPerTile = 128.0f;   // focused zoom-in limit: one tile spans this many pixels
   };

   WorldView() {
      targetState.scale = savedGlobalScale;
      targetState.offset = galaxyCamera.getOffset();
      syncCameraToCurrent();
   }

   void handleInput(const Input& input, const std::vector<std::unique_ptr<Planet>>& planets, glm::ivec2 windowSize) {
      if (input.isKeyPressed(GLFW_KEY_TAB)) {
         toggleFocusMode(planets);
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
         applyZoom(zoomDir, input.getMousePosition(), windowSize, planets);
      }
   }

   void update(float dt, const std::vector<std::unique_ptr<Planet>>& planets) {
      if (mode != Mode::Free) {
         if (const Planet* focused = focusedPlanet(planets)) {
            targetState.offset = focused->getConfig().position;
         }
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
   [[nodiscard]] int getFocusedPlanetIndex() const { return focusedPlanetIndex; }

private:
   Camera galaxyCamera;
   Config config;
   Mode mode = Mode::Free;

   CameraState currentState;
   CameraState targetState;
   CameraState transitionStart;

   int focusedPlanetIndex = -1;

   float savedGlobalScale = 0.5f;
   float savedFocusPixelsPerTile = 2.0f;

   float transitionT = 0.0f;

   [[nodiscard]] Planet* focusedPlanet(const std::vector<std::unique_ptr<Planet>>& planets) const {
      const bool valid = focusedPlanetIndex >= 0 && std::cmp_less(focusedPlanetIndex, planets.size());
      return valid ? planets[focusedPlanetIndex].get() : nullptr;
   }

   void syncCameraToCurrent() {
      galaxyCamera.setOffset(currentState.offset);
      galaxyCamera.setScale(currentState.scale);
   }

   void toggleFocusMode(const std::vector<std::unique_ptr<Planet>>& planets) {
      if (mode != Mode::Free) {
         if (const Planet* focused = focusedPlanet(planets)) {
            savedFocusPixelsPerTile = focused->getPixelsPerTile(targetState.scale);
         }
         mode = Mode::Free;
         focusedPlanetIndex = -1;

         targetState.scale = savedGlobalScale;
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

            const float ppt = std::clamp(savedFocusPixelsPerTile, config.focusMinPixelsPerTile, config.focusMaxPixelsPerTile);
            targetState.scale = planets[closest]->getFocusScaleForPixelsPerTile(ppt);
         }
      }
   }

   void applyDrag(glm::vec2 deltaPixels, const std::vector<std::unique_ptr<Planet>>& planets) {
      Planet* focused = mode == Mode::Locked ? focusedPlanet(planets) : nullptr;
      if (focused) {
         focused->localCamera.setScale(focused->getPixelsPerTile(galaxyCamera.getScale()));
         focused->localCamera.pan(deltaPixels);
      } else {
         targetState.offset -= deltaPixels / currentState.scale;
      }
   }

   void applyKeyboardPan(glm::vec2 direction, const std::vector<std::unique_ptr<Planet>>& planets) {
      Planet* focused = mode == Mode::Locked ? focusedPlanet(planets) : nullptr;
      if (focused) {
         focused->localCamera.setScale(focused->getPixelsPerTile(galaxyCamera.getScale()));
         focused->localCamera.pan(-direction);
      } else {
         targetState.offset += direction / currentState.scale;
      }
   }

   void applyZoom(float direction, glm::vec2 mousePos, glm::ivec2 windowSize, const std::vector<std::unique_ptr<Planet>>& planets) {
      const float zoomFactor = (direction > 0) ? config.zoomStep : (1.0f / config.zoomStep);

      float minScale = config.minScale;
      float maxScale = config.maxScale;
      if (const Planet* focused = mode != Mode::Free ? focusedPlanet(planets) : nullptr) {
         minScale = focused->getFocusScaleForPixelsPerTile(config.focusMinPixelsPerTile);
         maxScale = focused->getFocusScaleForPixelsPerTile(config.focusMaxPixelsPerTile);
      }

      const float newScale = std::clamp(targetState.scale * zoomFactor, minScale, maxScale);

      if (std::abs(newScale - targetState.scale) < 0.0001f) {
         return;
      }

      if (mode != Mode::Free) {
         targetState.scale = newScale;
      } else {
         const glm::vec2 halfScreen = glm::vec2(windowSize) * 0.5f;
         const glm::vec2 mouseFromCenter = mousePos - halfScreen;

         targetState.offset += mouseFromCenter * (1.0f / targetState.scale - 1.0f / newScale);
         targetState.scale = newScale;
      }
   }
};
