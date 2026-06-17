#pragma once

#include "config.hpp"
#include "game.hpp"
#include "platform/event.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "render/graphicsContext.hpp"
#include "ui/gui.hpp"
#include "util/logger.hpp"

#include <chrono>
#include <string>
#include <variant>
#include <webgpu/webgpu.hpp>

class Application {
public:
   bool initialize() {
      const Config config = Config::load();
      Log::setLevels(config.loggerSection);

      if (!window.initialize(config.windowSection, "Brights: WebGPU")) {
         return false;
      }

      gpuContext.initialize(window);
      ctx.initialize(&gpuContext);
      gameGraphics.initialize(ctx);

      ui.initialize(window, gpuContext.getDevice(), ctx.getSurfaceFormat());

      if (!game.initialize(&gameGraphics, gpuContext, ctx.getQueue())) {
         return false;
      }

      update(0.0f);

      auto now = std::chrono::steady_clock::now();
      lastFrameTime = now;
      lastFpsTime = now;

      return true;
   }

   void mainLoop() {
      input.reset();
      window.pollEvents();
      for (const Event& event : window.events()) {
         manageEvent(event);
      }

      frameCount++;
      auto currentTime = std::chrono::steady_clock::now();

      const std::chrono::duration<float, std::milli> frameDuration = currentTime - lastFrameTime;
      const float dtMs = frameDuration.count();
      lastFrameTime = currentTime;

      auto elapsedFpsMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFpsTime).count();

      if (elapsedFpsMs >= 1000) {
         lastFps = frameCount;
         window.setTitle("Brights: WebGPU - FPS: " + std::to_string(lastFps));
         frameCount = 0;
         lastFpsTime = currentTime;
      }

      update(dtMs);

      renderFrame();
   }

   [[nodiscard]] bool isRunning() const { return !window.shouldClose(); }

private:
   void update(float dt) { game.update(dt, input, window.framebufferSize()); }

   void renderFrame() {
      if (!ctx.beginFrame(window)) {
         return;
      }

      ui.beginFrame();

      const wgpu::RenderPassEncoder pass = ctx.beginRenderPass({0.0, 0.0, 0.0, 1.0});
      gameGraphics.draw(pass, game.getPlanets());
      ui.render(pass);
      pass.end();
      pass.release();

      ctx.endFrame();
   }

   void manageEvent(const Event& event) {
      std::visit([this](const auto& e) { handleEvent(e); }, event);
   }

   void handleEvent(const MouseMoved& e) { input.onMouseMove(e.position); }

   void handleEvent(const MouseButtonEvent& e) {
      if (e.pressed && ui.consumeMouse()) {
         return;
      }
      input.onMouseButton(e.button, e.pressed, e.position);
   }

   void handleEvent(const Scrolled& e) {
      if (ui.consumeMouse()) {
         return;
      }
      input.onScroll(e.delta);
   }

   void handleEvent(const KeyEvent& e) {
      if (ui.consumeKeyboard()) {
         return;
      }
      input.onKey(e.key, e.pressed);
   }

   Window window;
   GpuContext gpuContext;
   GraphicsContext ctx;
   GameGraphics gameGraphics;
   Gui ui;

   Input input;
   Game game;

   std::chrono::steady_clock::time_point lastFrameTime;
   std::chrono::steady_clock::time_point lastFpsTime;
   int frameCount = 0;
   int lastFps = 0;
};
