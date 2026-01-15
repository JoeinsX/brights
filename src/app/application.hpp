#pragma once

#include "core/graphics/camera.hpp"
#include "game.hpp"
#include "glm/gtx/vector_query.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "render/graphicsContext.hpp"
#include "render/wgslPreprocessor.hpp"
#include "util/logger.hpp"

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
      gpuContext.initialize(instance, window.handle);
      ctx.initialize(&gpuContext);
      gameGraphics.initialize(ctx);

      window.setCallbacks(onFramebufferResize, onCursorPos, onMouseButton, onScroll, onKey);

      game.initialize(&gameGraphics, gpuContext, ctx.getQueue());

      update(0.0f);

      auto now = std::chrono::steady_clock::now();
      lastFrameTime = now;
      lastFpsTime = now;

      return true;
   }

   void terminate() {
      gameGraphics.terminate();
      if (instance) {
         instance.release();
      }
      window.terminate();
   }

   void mainLoop() {
      input.reset();

      Window::pollEvents();

      frameCount++;
      auto currentTime = std::chrono::steady_clock::now();

      const std::chrono::duration<float, std::milli> frameDuration = currentTime - lastFrameTime;
      const float dtMs = frameDuration.count();
      lastFrameTime = currentTime;

      auto elapsedFpsMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFpsTime).count();

      if (elapsedFpsMs >= 1000) {
         const std::string title = "Brights: WebGPU - FPS: " + std::to_string(frameCount);
         glfwSetWindowTitle(window.handle, title.c_str());
         frameCount = 0;
         lastFpsTime = currentTime;
      }

      update(dtMs);

      gameGraphics.render(ctx, window, game.getPlanets());
   }

   [[nodiscard]] bool isRunning() const { return !window.shouldClose(); }

private:
   void update(float dt) {
      glm::ivec2 windowSize{};
      glfwGetFramebufferSize(window.handle, &windowSize.x, &windowSize.y);

      game.update(dt, input, windowSize);
   }

   static void onFramebufferResize(GLFWwindow* window, int /*newWidth*/, int /*newHeight*/) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app && app->continuousResize) {
         app->update(0.0f);
         app->gameGraphics.render(app->ctx, app->window, app->game.getPlanets());
      }
   }

   static void onCursorPos(GLFWwindow* window, double xpos, double ypos) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app) {
         app->input.onCursorPos(xpos, ypos);
      }
   }

   static void onMouseButton(GLFWwindow* window, int button, int action, int /*mods*/) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      double x{}, y{};
      glfwGetCursorPos(window, &x, &y);
      if (app) {
         app->input.onMouseButton(button, action, x, y);
      }
   }

   static void onKey(GLFWwindow* window, int key, int /*scancode*/, const int action, int /*mods*/) {
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
   GpuContext gpuContext;
   GraphicsContext ctx;
   GameGraphics gameGraphics;
   wgpu::Instance instance = nullptr;

   Input input;
   Game game;

   bool continuousResize = false;

   std::chrono::steady_clock::time_point lastFrameTime;
   std::chrono::steady_clock::time_point lastFpsTime;
   int frameCount = 0;
};
