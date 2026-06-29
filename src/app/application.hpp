#pragma once

#include "applicationMeta.hpp"
#include "game.hpp"
#include "input/event.hpp"
#include "input/input.hpp"
#include "platform/window.hpp"
#include "render/graphicsContext.hpp"
#include "settings/settings.hpp"
#include "ui/gui.hpp"
#include "ui/settingsPanel.hpp"
#include "ui/tileInspectorPanel.hpp"
#include "ui/worldEditPanel.hpp"
#include "util/logger.hpp"

#include <chrono>
#include <string>
#include <variant>
#include <webgpu/webgpu.hpp>

using AppComponentPack = AppComponentsMixin<Settings, Logger, Window, GameGraphics>;

class Application: public AppComponentPack {
public:
   Application(): AppComponentPack(static_cast<Settings&>(*this)) {}

   Application(Application& other) = delete;
   Application(Application&& other) = delete;
   Application& operator =(Application& other) = delete;
   Application& operator =(Application&& other) = delete;

   ~Application() { getComponent<Settings>().save(); }

   bool initialize() {
      getComponent<Settings>().load();

      Logger::setLevels(getComponent<Settings>().accessSection<LoggerSettings>());

      if (!getComponent<Window>().initialize(getComponent<Settings>().getSection<WindowSettings>(), "Brights: WebGPU")) {
         return false;
      }

      gpuContext.initialize(getComponent<Window>());
      ctx.initialize(&gpuContext);
      getComponent<GameGraphics>().initialize(ctx);

      ui.initialize(getComponent<Window>(), gpuContext.getDevice(), ctx.getSurfaceFormat());

      if (!game.initialize(&getComponent<GameGraphics>(), gpuContext, ctx.getQueue())) {
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
      getComponent<Window>().pollEvents();
      for (const Event& event : getComponent<Window>().events()) {
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
         getComponent<Window>().setTitle("Brights: WebGPU - FPS: " + std::to_string(lastFps));
         frameCount = 0;
         lastFpsTime = currentTime;
      }

      update(dtMs);

      renderFrame();
   }

   [[nodiscard]] bool isRunning() const { return !getComponent<Window>().shouldClose(); }

private:
   void update(float dt) { game.update(dt, input, getComponent<Window>().framebufferSize(), getComponent<Settings>().accessSection<RenderSettings>(), editPanel.settings()); }

   void renderFrame() {
      if (!ctx.beginFrame(getComponent<Window>())) {
         return;
      }

      ui.beginFrame();
      if (settingsPanel.draw(getComponent<Settings>().accessSection<RenderSettings>())) {
         getComponent<GameGraphics>().refreshDefines();
      }
      editPanel.draw(game.getRegistry(), reinterpret_cast<ImTextureID>(game.getAtlasView()), game.getEditStatus());
      tileInspectorPanel.draw(game.getRegistry(), reinterpret_cast<ImTextureID>(game.getAtlasView()), game.getTileInspection());

      const wgpu::RenderPassEncoder pass = ctx.beginRenderPass({0.0, 0.0, 0.0, 1.0});
      getComponent<GameGraphics>().draw(pass, game.getPlanets());
      getComponent<GameGraphics>().drawSprites(pass, game.getPlanets());
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
      if (e.pressed && e.key == Key::F1) {
         settingsPanel.toggle();
         return;
      }
      if (e.pressed && e.key == Key::F2) {
         editPanel.toggle();
         return;
      }
      if (e.pressed && e.key == Key::F3) {
         tileInspectorPanel.toggle();
         return;
      }
      if (ui.consumeKeyboard()) {
         return;
      }
      input.onKey(e.key, e.pressed);
   }

   GpuContext gpuContext;
   GraphicsContext ctx;
   Gui ui;
   SettingsPanel settingsPanel;
   WorldEditPanel editPanel;
   TileInspectorPanel tileInspectorPanel;

   Input input;
   Game game;

   std::chrono::steady_clock::time_point lastFrameTime;
   std::chrono::steady_clock::time_point lastFpsTime;
   int frameCount = 0;
   int lastFps = 0;
};
