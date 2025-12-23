#define WEBGPU_CPP_IMPLEMENTATION
#include <chrono>
#include <webgpu/webgpu.hpp>

#include <string>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "render/wgslPreprocessor.hpp"
#include "native/window.hpp"

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

    bool Initialize() {
        Log::SetLevel(Log::Level::Info);

        if(!window.Initialize(640, 480, "Brights: WebGPU", this))
            return false;

        instance = wgpuCreateInstance(nullptr);
        if(!graphics.Initialize(instance, window.handle)) {
            Log::Fatal("Failed to initialize graphics context");
            return false;
        }

        window.SetCallbacks(OnFramebufferResize, OnCursorPos, OnMouseButton,
                            OnScroll);

        InitializeGameContent();

        if(!graphics.InitializeTexture()) {
            Log::Error("Failed to load texture assets/atlas.png");
            return false;
        }
        graphics.InitializePipeline(chunk);
        Update();

        lastFpsTime = std::chrono::steady_clock::now();
        return true;
    }

    void Terminate() {
        graphics.Terminate();
        if(instance) instance.release();
        window.Terminate();
    }

    void MainLoop() {
        window.PollEvents();
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

        Update();
        graphics.Render(window.handle);
    }

    bool IsRunning() { return !window.ShouldClose(); }

private:
    void InitializeGameContent() {
        registry.Register(TileID::Grass, 0, 0, 4, 1.0);
        registry.Register(TileID::Water, 1, 0, 4, 0.6);
        registry.Register(TileID::ColdGrass, 2, 0, 4, 1.0);
        registry.Register(TileID::Stone, 3, 0, 4, 2.0, 0.5);
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

        WorldGenerator::Generate(chunk, rng());
        chunk.RebuildDisplayMap(registry, rng);
    }

    void Update() {
        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window.handle, &windowWidth, &windowHeight);

        static constexpr float baseResolutionX = 640.f;
        static constexpr float baseResolutionY = 480.f;
        static constexpr float basePerspectiveStrength = 0.002f;
        static constexpr float perspectiveStrength = 0.002f;

        float camX = camera.GetOffsetX();
        float camY = camera.GetOffsetY();
        auto macroX = static_cast<int32_t>(std::floor(camX));
        auto macroY = static_cast<int32_t>(std::floor(camY));

        UniformData uData{
            .macroOffsetX = macroX,
            .macroOffsetY = macroY,
            .offsetX = camX - static_cast<float>(macroX),
            .offsetY = camY - static_cast<float>(macroY),
            .resX = static_cast<float>(windowWidth),
            .resY = static_cast<float>(windowHeight),
            .scale = camera.GetScale(),
            .mapSize = static_cast<float>(Chunk::GetSize()),
            .resScaleX = windowWidth / baseResolutionX,
            .resScaleY = windowHeight / baseResolutionY,
            .perspectiveStrength = perspectiveStrength,
            .perspectiveScale = perspectiveStrength / basePerspectiveStrength
        };

        graphics.queue.writeBuffer(graphics.uniformBuffer, 0, &uData,
                                   sizeof(UniformData));
    }

    static void OnFramebufferResize(GLFWwindow* window, int, int) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app && app->continuousResize) {
            app->Update();
            app->graphics.Render(app->window.handle);
        }
    }

    static void OnCursorPos(GLFWwindow* window, double xpos, double ypos) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app) {
            if(app->isDragging) {
                float dx = static_cast<float>(xpos - app->lastMouseX);
                float dy = static_cast<float>(ypos - app->lastMouseY);
                app->camera.Pan(dx, dy);
            }
            app->lastMouseX = xpos;
            app->lastMouseY = ypos;
        }
    }

    static void OnMouseButton(GLFWwindow* window, int button, int action,
                              int mods) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(
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

    static void OnScroll(GLFWwindow* window, double xoffset, double yoffset) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(
            window));
        if(app)
            app->camera.Zoom(static_cast<float>(yoffset),
                             app->lastMouseX, app->lastMouseY);
    }

    Window window;
    GraphicsContext graphics;
    wgpu::Instance instance = nullptr;

    TileRegistry registry;
    Chunk chunk;
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
    if(!app.Initialize()) return 1;
    while(app.IsRunning()) {
        app.MainLoop();
    }
    app.Terminate();
    return 0;
}