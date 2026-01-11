#define WEBGPU_CPP_IMPLEMENTATION
#include <chrono>
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEFAULT_PACKED_GENTYPES ;
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/vector_query.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/native/graphicsContext.hpp"
#include "render/wgslPreprocessor.hpp"
#include "util/logger.hpp"
#include "world/chunk.hpp"
#include "world/tile.hpp"
#include "world/worldGenerator.hpp"

#include <map>
#include <random>
#include <string>

class Application {
public:
   Application(): rng(0) {}

   bool initialize() {
      Log::setLevel(Log::Level::Info);

      if (!window.initialize(640, 480, "Brights: WebGPU", this)) {
         return false;
      }

      instance = wgpuCreateInstance(nullptr);
      if (!graphics.initialize(instance, window.handle)) {
         Log::fatal("Failed to initialize graphics context");
         return false;
      }

      window.setCallbacks(onFramebufferResize, onCursorPos, onMouseButton, onScroll, onKey);

      initializeGameContent();

      if (!graphics.initializeTexture()) {
         Log::error("Failed to load texture assets/atlas.png");
         return false;
      }
      graphics.initializePipeline(registry, rng, camera);

      worldGenerator = std::make_unique<WorldGenerator>();
      world = std::make_unique<World>(registry, rng, *worldGenerator, Chunk::COUNT / 2, 0);

      worldRenderAdapter = std::make_unique<WorldRenderAdapter>(graphics.queue, graphics.chunkRefMapBuffer, graphics.packedBuffer, graphics.tilemapBuffer, Chunk::COUNT / 2, *world, camera);

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

      update(elapsedMs);
      graphics.render(window.handle);
   }

   [[nodiscard]] bool isRunning() const { return !window.shouldClose(); }

private:
   void initializeGameContent() {
      registry.registerTile(TileID::Grass, 0, 0, 4, 1.0);
      registry.registerTile(TileID::Water, 1, 0, 4, 0.6);
      registry.registerTile(TileID::ColdGrass, 2, 0, 4, 1.0);
      registry.registerTile(TileID::Stone, 3, 0, 4, 2.0, 0.5);
      registry.registerTile(TileID::HardStone, 4, 0, 4, 2.0);
      registry.registerTile(TileID::Gravel, 5, 0, 1, 1.0, 0.5);
      registry.registerTile(TileID::HardGravel, 6, 0, 1, 1.0);
      registry.registerTile(TileID::Snow, 5, 1, 4, 1.0, 0.5);
      registry.registerTile(TileID::Ice, 6, 1, 4, 0.8);
      registry.registerTile(TileID::Planks, 7, 0, 1, 2.0, 0.0);
      registry.registerTile(TileID::PlankFloor, 8, 0, 1, 1.0);
      registry.registerTile(TileID::RedOre, 9, 0, 1, 1.6);
      registry.registerTile(TileID::BlueOre, 10, 0, 1, 1.6);
      registry.registerTile(TileID::ColdWater, 1, 5, 4, 0.7);
      registry.registerTile(TileID::BurntGround, 11, 0, 1, 0.7);
      registry.registerTile(TileID::Sand, 12, 0, 4, 0.8);
   }

   void update(float dt) {
      int windowWidth, windowHeight;
      glfwGetFramebufferSize(window.handle, &windowWidth, &windowHeight);

      static constexpr float baseResolutionX = 640.f;
      static constexpr float baseResolutionY = 480.f;
      static constexpr float basePerspectiveStrength = 0.002f;
      static constexpr float perspectiveStrength = 0.002f;

      static glm::ivec2 globalChunkMove{};

      glm::vec2 movementVector = glm::vec2(keyStates[GLFW_KEY_A] - keyStates[GLFW_KEY_D], keyStates[GLFW_KEY_W] - keyStates[GLFW_KEY_S]);
      movementVector = glm::normalize(movementVector);

      if (glm::isNormalized(movementVector, 0.1f)) {
         std::cout << movementVector.x << " " << movementVector.y << std::endl;
         camera.pan(movementVector * 10.f);
      }

      world->update(camera, globalChunkMove);

      worldRenderAdapter->update(camera, globalChunkMove);

      float camCenterX = camera.getOffsetX();
      float camCenterY = camera.getOffsetY();

      float halfScreenWorldW = (static_cast<float>(windowWidth) * 0.5f) / camera.getScale();
      float halfScreenWorldH = (static_cast<float>(windowHeight) * 0.5f) / camera.getScale();

      float shaderOffsetX = camCenterX - halfScreenWorldW;
      float shaderOffsetY = camCenterY - halfScreenWorldH;

      auto macroX = static_cast<int32_t>(std::floor(shaderOffsetX));
      auto macroY = static_cast<int32_t>(std::floor(shaderOffsetY));

      UniformData uData{
         .macroOffset = {macroX,                                     macroY                                    },
         .offset = {shaderOffsetX - static_cast<float>(macroX), shaderOffsetY - static_cast<float>(macroY)},
         .res = {static_cast<float>(windowWidth),            static_cast<float>(windowHeight)          },
         .scale = camera.getScale(),
         .mapSize = static_cast<uint32_t>(Chunk::SIZE) * Chunk::COUNT,
         .sphereMapScale = static_cast<float>(Chunk::COUNT - 2) / static_cast<float>(Chunk::COUNT),
         .chunkSize = Chunk::SIZE,
         .chunkOffset = globalChunkMove,
         .resScale = {windowWidth / baseResolutionX,              windowHeight / baseResolutionY            },
         .perspectiveStrength = perspectiveStrength,
         .perspectiveScale = perspectiveStrength / basePerspectiveStrength
      };

      graphics.queue.writeBuffer(graphics.uniformBuffer, 0, &uData, sizeof(UniformData));
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
         if (app->isDragging) {
            auto dx = static_cast<float>(xpos - app->lastMouseX);
            auto dy = static_cast<float>(ypos - app->lastMouseY);
            app->camera.pan({dx, dy});
         }
         app->lastMouseX = xpos;
         app->lastMouseY = ypos;
      }
   }

   static void onMouseButton(GLFWwindow* window, int button, int action, int mods) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));

      if (app && button == GLFW_MOUSE_BUTTON_LEFT) {
         if (action == GLFW_PRESS) {
            app->isDragging = true;
            glfwGetCursorPos(window, &app->lastMouseX, &app->lastMouseY);
         } else if (action == GLFW_RELEASE) {
            app->isDragging = false;
         }
      }
   }

   static void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
      auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));

      if (action == GLFW_PRESS || action == GLFW_RELEASE) {
         const bool isPressed = (action == GLFW_PRESS);
         app->keyStates[key] = isPressed;
      }
   }

   static void onScroll(GLFWwindow* window, double /*xoffset*/, double yoffset) {
      const auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      if (app) {
         int width, height;
         glfwGetFramebufferSize(window, &width, &height);
         app->camera.zoom(static_cast<float>(yoffset), app->lastMouseX, app->lastMouseY, width, height);
      }
   }

   Window window;
   GraphicsContext graphics;
   std::unique_ptr<WorldGenerator> worldGenerator;
   std::unique_ptr<World> world;
   std::unique_ptr<WorldRenderAdapter> worldRenderAdapter;
   wgpu::Instance instance = nullptr;

   TileRegistry registry;
   Camera camera;
   std::mt19937 rng;

   bool continuousResize = false;
   std::chrono::steady_clock::time_point lastFpsTime;
   int frameCount = 0;

   bool isDragging = false;
   double lastMouseX = 0.0;
   double lastMouseY = 0.0;
   std::map<int, bool> keyStates{};
};

int main() {
   Application app;
   if (!app.initialize()) {
      return 1;
   }
   while (app.isRunning()) {
      app.mainLoop();
   }
   app.terminate();
   return 0;
}
