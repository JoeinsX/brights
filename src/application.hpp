#pragma once

#include "game.hpp"
#include "glm/gtx/vector_query.hpp"
#include "input.hpp"
#include "platform/window.hpp"
#include "render/core/camera.hpp"
#include "render/core/wgslPreprocessor.hpp"
#include "render/native/graphicsContext.hpp"
#include "util/logger.hpp"
#include "world/chunk.hpp"
#include "world/tile.hpp"
#include "world/worldGenerator.hpp"

#include <chrono>
#include <glm/glm.hpp>
#include <map>
#include <random>
#include <string>
#include <webgpu/webgpu.hpp>

class Application {
public:
   bool initialize() {
      Log::setLevel(Log::Level::Info);

      if (!window.initialize(640, 480, "Brights: WebGPU", this)) {
         return false;
      }

      instance = wgpuCreateInstance(nullptr);
      context.initialize(instance, window.handle);
      if (!graphics.initialize(&context)) {
         Log::fatal("Failed to initialize graphics context");
         return false;
      }

      window.setCallbacks(onFramebufferResize, onCursorPos, onMouseButton, onScroll, onKey);

      if (!graphics.initializeTexture()) {
         Log::error("Failed to load texture assets/atlas.png");
         return false;
      }
      graphics.initializePipeline();

      game.initialize(&graphics, context.queue);

      update(0.0f);

      lastFpsTime = std::chrono::steady_clock::now();
      return true;
   }

   void terminate() {
      graphics.terminate();
      if (instance) {
         instance.release();
      }
      window.terminate();
   }

   void mainLoop() {
      input.reset();

      window.pollEvents();

      frameCount++;
      auto currentTime = std::chrono::steady_clock::now();
      auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFpsTime).count();

      if (elapsedMs >= 1000) {
         std::string title = "Brights: WebGPU - FPS: " + std::to_string(frameCount);
         glfwSetWindowTitle(window.handle, title.c_str());
         frameCount = 0;
         lastFpsTime = currentTime;
      }

      update(static_cast<float>(elapsedMs));

      graphics.render(window.handle);
   }

   [[nodiscard]] bool isRunning() const { return !window.shouldClose(); }

private:
   void update(float dt) {
      glm::ivec2 windowSize{};
      glfwGetFramebufferSize(window.handle, &windowSize.x, &windowSize.y);

      game.update(dt, input, windowSize);
   }

   static void onFramebufferResize(GLFWwindow* window, int, int) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app && app->continuousResize) {
         app->update(0.0f);
         app->graphics.render(app->window.handle);
      }
   }

   static void onCursorPos(GLFWwindow* window, double xpos, double ypos) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app) {
         app->input.onCursorPos(xpos, ypos);
      }
   }

   static void onMouseButton(GLFWwindow* window, int button, int action, int mods) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      double x, y;
      glfwGetCursorPos(window, &x, &y);
      if (app) {
         app->input.onMouseButton(button, action, x, y);
      }
   }

   static void onKey(GLFWwindow* window, int key, int scancode, const int action, int mods) {
      auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app) {
         app->input.onKey(key, action);
      }
   }

   static void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app) {
         app->input.onScroll(xoffset, yoffset);
      }
   }

   Window window;
   GpuContext context;
   GraphicsContext graphics;
   wgpu::Instance instance = nullptr;

   Input input;
   Game game;

   bool continuousResize = false;
   std::chrono::steady_clock::time_point lastFpsTime;
   int frameCount = 0;
};
