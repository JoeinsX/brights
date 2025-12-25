#define WEBGPU_CPP_IMPLEMENTATION
#include <chrono>
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEFAULT_PACKED_GENTYPES;
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


#include <string>
#include <random>

#include "render/wgslPreprocessor.hpp"
#include "platform/window.hpp"

#include "render/camera.hpp"
#include "render/native/graphicsContext.hpp"
#include "util/logger.hpp"
#include "world/tile.hpp"
#include "world/chunk.hpp"
#include "world/worldGenerator.hpp"

class Application {
public:
    Application()
        : rng(0) {
    }

    bool initialize() {
        Log::setLevel(Log::Level::Info);

        if(!window.initialize(640, 480, "Brights: WebGPU", this))
            return false;

        instance = wgpuCreateInstance(nullptr);
        if(!graphics.initialize(instance, window.handle)) {
            Log::fatal("Failed to initialize graphics context");
            return false;
        }

        window.setCallbacks(onFramebufferResize, onCursorPos, onMouseButton,
                            onScroll);

        initializeGameContent();

        if(!graphics.initializeTexture()) {
            Log::error("Failed to load texture assets/atlas.png");
            return false;
        }
        graphics.initializePipeline(registry, rng, camera);

        worldRenderAdapter = std::make_unique<WorldRenderAdapter>(graphics.queue, graphics.chunkRefMapBuffer,
                                                                  graphics.packedBuffer, graphics.tilemapBuffer,
                                                                  Chunk::COUNT,
                                                                  *world, camera);

        update();

        lastFpsTime = std::chrono::steady_clock::now();
        return true;
    }

    void terminate() {
        graphics.terminate();
        if(instance) instance.release();
        window.terminate();
    }

    void mainLoop() {
        window.pollEvents();
        frameCount++;
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastFpsTime).count();

        if(elapsedMs >= 1000) {
            std::string title = "Brights: WebGPU - FPS: " +
                                std::to_string(frameCount);
            glfwSetWindowTitle(window.handle, title.c_str());
            frameCount = 0;
            lastFpsTime = currentTime;
        }

        update();
        graphics.render(window.handle);
    }

    [[nodiscard]] bool isRunning() const { return !window.shouldClose(); }

private:
    void initializeGameContent() {
        registry.Register(TileID::Grass, 0, 0, 4, 1.0);
        registry.Register(TileID::Water, 1, 0, 4, 0.6);
        registry.Register(TileID::ColdGrass, 2, 0, 4, 1.0);
        registry.Register(TileID::Stone, 7, 0, 1, 2.0, 0.1);
        registry.Register(TileID::HardStone, 4, 0, 4, 2.0);
        registry.Register(TileID::Gravel, 5, 0, 1, 1.0, 0.5);
        registry.Register(TileID::HardGravel, 6, 0, 1, 1.0);
        registry.Register(TileID::Snow, 5, 1, 4, 1.0, 0.5);
        registry.Register(TileID::Ice, 6, 1, 4, 0.8);
        registry.Register(TileID::Planks, 7, 0, 1, 1.6, 0.0);
        registry.Register(TileID::PlankFloor, 8, 0, 1, 1.0);
        registry.Register(TileID::RedOre, 9, 0, 1, 1.6);
        registry.Register(TileID::BlueOre, 10, 0, 1, 1.6);
        registry.Register(TileID::ColdWater, 1, 5, 4, 0.7);
        registry.Register(TileID::BurntGround, 11, 0, 1, 0.7);
        registry.Register(TileID::Sand, 12, 0, 4, 0.8);

        world = std::make_unique<World>(Chunk::COUNT, registry, rng);
    }

    void update() {
        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window.handle, &windowWidth, &windowHeight);

        static constexpr float baseResolutionX = 640.f;
        static constexpr float baseResolutionY = 480.f;
        static constexpr float basePerspectiveStrength = 0.002f;
        static constexpr float perspectiveStrength = 0.002f;

        static glm::ivec2 globalChunkMove{};

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
            .macroOffset = {macroX, macroY},
            .offset = {shaderOffsetX - static_cast<float>(macroX),
                       shaderOffsetY - static_cast<float>(macroY)},
            .res = {static_cast<float>(windowWidth), static_cast<float>(windowHeight)},
            .scale = camera.getScale(),
            .mapSize = static_cast<float>(Chunk::getSize() * Chunk::COUNT),
            .mapSizeChunks = Chunk::COUNT,
            .chunkSize = Chunk::SIZE,
            .resScale = {windowWidth / baseResolutionX, windowHeight / baseResolutionY},
            .perspectiveStrength = perspectiveStrength,
            .perspectiveScale = perspectiveStrength / basePerspectiveStrength
        };

        graphics.queue.writeBuffer(graphics.uniformBuffer, 0, &uData,
                                   sizeof(UniformData));
    }

    static void onFramebufferResize(GLFWwindow* window, int, int) {
        const auto app = static_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app && app->continuousResize) {
            app->update();
            app->graphics.render(app->window.handle);
        }
    }

    static void onCursorPos(GLFWwindow* window, double xpos, double ypos) {
        const auto app = static_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app) {
            if(app->isDragging) {
                auto dx = static_cast<float>(xpos - app->lastMouseX);
                auto dy = static_cast<float>(ypos - app->lastMouseY);
                app->camera.pan(dx, dy);
            }
            app->lastMouseX = xpos;
            app->lastMouseY = ypos;
        }
    }

    static void onMouseButton(GLFWwindow* window, int button, int action,
                              int mods) {
        const auto app = static_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app && button == GLFW_MOUSE_BUTTON_LEFT) {
            if(action == GLFW_PRESS) {
                app->isDragging = true;
                glfwGetCursorPos(window, &app->lastMouseX,
                                 &app->lastMouseY);
            } else if(action == GLFW_RELEASE) {
                app->isDragging = false;
            }
        }
    }

    static void onScroll(GLFWwindow* window, double /*xoffset*/, double yoffset) {
        const auto app = static_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            app->camera.zoom(static_cast<float>(yoffset),
                             app->lastMouseX, app->lastMouseY, width, height);
        }
    }

    Window window;
    GraphicsContext graphics;
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
};

int main() {
    Application app;
    if(!app.initialize()) return 1;
    while(app.isRunning()) {
        app.mainLoop();
    }
    app.terminate();
    return 0;
}