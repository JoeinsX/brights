#define WEBGPU_CPP_IMPLEMENTATION
#include <chrono>
#include <webgpu/webgpu.hpp>

#include "wgslPreprocessor.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <FastNoise/FastNoise.h>

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unordered_map>

#include <random>

std::mt19937 random{0};

class Logger {
public:
    enum class Level { None = 0, Error, Warning, Info };

    static void SetLevel(Level level) { s_CurrentLevel = level; }

    static void Error(const std::string& message) {
        if(s_CurrentLevel >= Level::Error)
            std::cerr << "[ERR]  " << message << std::endl;
    }

private:
    static Level s_CurrentLevel;
};

Logger::Level Logger::s_CurrentLevel = Logger::Level::Info;

enum class TileID : uint8_t {
    Air = 0,
    Grass,
    Water,
    ColdGrass,
    Stone,
    HardStone,
    Gravel,
    HardGravel,
    Snow,
    Ice,
    Planks,
    PlankFloor,
    RedOre,
    BlueOre,
    ColdWater
};

struct TileDefinition {
    int atlasBaseX = 0;
    int atlasBaseY = 0;
    int variationCount = 1;
    float height = 0.5f;
    float softness = 0.5f;
};

class TileRegistry {
public:
    void Register(TileID id, int x, int y, int variations,
                  float height = 0.5f,
                  float softness = 0.5f) {
        m_defs[id] = {x, y, variations, height, softness};
    }

    const TileDefinition* Get(TileID id) const {
        auto it = m_defs.find(id);
        if(it != m_defs.end()) return &it->second;
        return nullptr;
    }

private:
    std::unordered_map<TileID, TileDefinition> m_defs;
};

class Chunk {
public:
    static constexpr int SIZE = 128;

    Chunk() {
        m_terrainMap.resize(SIZE * SIZE, TileID::Water);
        m_wallMap.resize(SIZE * SIZE, TileID::Air);
        m_displayMap.resize(SIZE * SIZE * 4, 0.0f);
        m_heightMap.resize(SIZE * SIZE, 0.0f);
        m_softnessMap.resize(SIZE * SIZE, 0.0f);
    }

    void SetTerrain(int x, int y, TileID id) {
        if(x < 0 || x >= SIZE || y < 0 || y >= SIZE) return;
        m_terrainMap[y * SIZE + x] = id;
    }

    void SetWall(int x, int y, TileID id) {
        if(x < 0 || x >= SIZE || y < 0 || y >= SIZE) return;
        m_wallMap[y * SIZE + x] = id;
    }

    std::pair<float, float> GetAtlasCoords(const TileRegistry& registry,
                                           TileID id, int x, int y) {
        if(id == TileID::Air) return {-1.0f, -1.0f};

        const TileDefinition* def = registry.Get(id);
        if(!def) return {-1.0f, -1.0f};

        float atlasX = static_cast<float>(def->atlasBaseX);

        int variationIdx = 0;
        if(def->variationCount > 1) {
            variationIdx = random() % def->variationCount;
        }
        float atlasY = static_cast<float>(def->atlasBaseY + variationIdx);

        return {atlasX, atlasY};
    }

    void RebuildDisplayMap(const TileRegistry& registry) {
        for(int y = 0; y < SIZE; ++y) {
            for(int x = 0; x < SIZE; ++x) {
                int index = y * SIZE + x;
                TileID tID = m_terrainMap[index];
                TileID wID = m_wallMap[index];

                auto terrainCoords = GetAtlasCoords(registry, tID, x, y);
                auto wallCoords = GetAtlasCoords(registry, wID, x, y);

                m_displayMap[index * 4 + 0] = terrainCoords.first;
                m_displayMap[index * 4 + 1] = terrainCoords.second;
                m_displayMap[index * 4 + 2] = wallCoords.first;
                m_displayMap[index * 4 + 3] = wallCoords.second;
                const TileDefinition* def = registry.Get(tID);
                m_heightMap[index] = def ? def->height : 0.0f;
                m_softnessMap[index] = def ? def->softness : 0.0f;
            }
        }
    }

    [[nodiscard]] const void* GetDisplayData() const {
        return m_displayMap.data();
    }

    [[nodiscard]] size_t GetDisplayDataSize() const {
        return m_displayMap.size() * sizeof(float);
    }

    const void* GetHeightData() const { return m_heightMap.data(); }

    [[nodiscard]] size_t GetHeightDataSize() const {
        return m_heightMap.size() * sizeof(float);
    }

    [[nodiscard]] const void* GetSoftnessData() const {
        return m_softnessMap.data();
    }

    [[nodiscard]] size_t GetSoftnessDataSize() const {
        return m_softnessMap.size() * sizeof(float);
    }

    static int GetSize() { return SIZE; }

private:
    std::vector<TileID> m_terrainMap;
    std::vector<TileID> m_wallMap;
    std::vector<float> m_displayMap;
    std::vector<float> m_heightMap;
    std::vector<float> m_softnessMap;
};

class Application {
public:
    bool Initialize();
    void Terminate();
    void MainLoop();
    void Update();
    void Render();
    bool IsRunning();

private:
    wgpu::TextureView GetNextSurfaceTextureView();
    void InitializePipeline();
    void InitializeGameContent();
    void GenerateWorld();
    bool InitializeTexture();

    static void OnFramebufferResize(GLFWwindow* window, int width,
                                    int height);
    static void OnCursorPos(GLFWwindow* window, double xpos, double ypos);
    static void OnMouseButton(GLFWwindow* window, int button, int action,
                              int mods);
    static void OnScroll(GLFWwindow* window, double xoffset,
                         double yoffset);

    void HandleMouseMove(double x, double y);
    void HandleMouseClick(int button, int action);
    void HandleScroll(double yoffset);

    GLFWwindow* window = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;
    wgpu::Surface surface = nullptr;
    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
    wgpu::SurfaceConfiguration surfaceConfig = {};
    wgpu::RenderPipeline pipeline = nullptr;

    wgpu::Buffer m_tilemapBuffer = nullptr;
    wgpu::Buffer m_heightBuffer = nullptr;
    wgpu::Buffer m_softnessBuffer = nullptr;

    wgpu::Texture m_heightTexture = nullptr;
    wgpu::TextureView m_heightTextureView = nullptr;

    wgpu::Buffer m_uniformBuffer = nullptr;

    wgpu::Texture m_texture = nullptr;
    wgpu::TextureView m_textureView = nullptr;
    wgpu::Sampler m_sampler = nullptr;

    wgpu::BindGroup m_bindGroup = nullptr;
    wgpu::BindGroupLayout m_bindGroupLayout = nullptr;

    TileRegistry m_registry;
    Chunk m_chunk;

    bool m_continuousResize = false;

    std::chrono::steady_clock::time_point m_lastFpsTime;
    int m_frameCount = 0;

    struct UniformData {
        float offsetX, offsetY;
        float resX, resY;
        float scale;
        float mapSize;
    };

    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;
    float m_scale = 4.0f;

    bool m_isDragging = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
};

int main() {
    Application app;
    if(!app.Initialize()) return 1;

#ifdef __EMSCRIPTEN__
    auto callback = [](void* arg) {
        reinterpret_cast<Application*>(arg)->MainLoop();
    };
    emscripten_set_main_loop_arg(callback, &app, 0, true);
#else
    while(app.IsRunning()) {
        app.MainLoop();
    }
#endif
    app.Terminate();
    return 0;
}

bool Application::Initialize() {
    if(!glfwInit()) return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(640, 480, "Brights: WebGPU", nullptr,
                              nullptr);
    if(!window) return false;

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, OnFramebufferResize);
    glfwSetCursorPosCallback(window, OnCursorPos);
    glfwSetMouseButtonCallback(window, OnMouseButton);
    glfwSetScrollCallback(window, OnScroll);

    wgpu::Instance instance = wgpuCreateInstance(nullptr);
    surface = glfwGetWGPUSurface(instance, window);

    wgpu::RequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = surface;
    wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
    instance.release();

    wgpu::DeviceDescriptor deviceDesc = {};
    device = adapter.requestDevice(deviceDesc);
    queue = device.getQueue();

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    surfaceConfig.width = w;
    surfaceConfig.height = h;
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
    surfaceFormat = surface.getPreferredFormat(adapter);
    surfaceConfig.format = surfaceFormat;
    surfaceConfig.device = device;
    surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
    surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
    surface.configure(surfaceConfig);

    adapter.release();

    InitializeGameContent();

    if(!InitializeTexture()) {
        Logger::Error("Failed to load texture assets/atlas.png");
        return false;
    }

    InitializePipeline();
    Update();

    m_lastFpsTime = std::chrono::steady_clock::now();

    return true;
}

void Application::InitializeGameContent() {
    m_registry.Register(TileID::Grass, 0, 0, 4, 0.5);
    m_registry.Register(TileID::Water, 1, 0, 4, 0.3);
    m_registry.Register(TileID::ColdGrass, 2, 0, 4, 0.5);
    m_registry.Register(TileID::Stone, 3, 0, 4, 0.8, 0.9);
    m_registry.Register(TileID::HardStone, 4, 0, 4, 0.8);
    m_registry.Register(TileID::Gravel, 5, 0, 1, 0.5);
    m_registry.Register(TileID::HardGravel, 6, 0, 1, 0.5);
    m_registry.Register(TileID::Snow, 5, 1, 4, 0.5, 1.0);
    m_registry.Register(TileID::Ice, 6, 1, 4, 0.4);
    m_registry.Register(TileID::Planks, 7, 0, 1, 0.8, 0.0);
    m_registry.Register(TileID::PlankFloor, 8, 0, 1, 0.5);
    m_registry.Register(TileID::RedOre, 9, 0, 1, 0.8);
    m_registry.Register(TileID::BlueOre, 10, 0, 1, 0.8);
    m_registry.Register(TileID::ColdWater, 1, 5, 4, 0.35);

    GenerateWorld();

    m_chunk.RebuildDisplayMap(m_registry);
}

void Application::GenerateWorld() {
    int size = m_chunk.GetSize();
    int seed = random();

    // 1. Elevation Noise (Simplex Fractal)
    auto fnElevation = FastNoise::New<FastNoise::FractalFBm>();
    fnElevation->SetSource(FastNoise::New<FastNoise::Simplex>());
    fnElevation->SetOctaveCount(4);
    fnElevation->SetGain(0.5f);
    fnElevation->SetLacunarity(2.0f);

    // 2. Temperature Noise (Perlin, larger scale)
    auto fnTemperature = FastNoise::New<FastNoise::Simplex>();

    // 3. Moisture/Vegetation Noise
    auto fnMoisture = FastNoise::New<FastNoise::FractalFBm>();
    fnMoisture->SetSource(FastNoise::New<FastNoise::Simplex>());
    fnMoisture->SetOctaveCount(2);

    // 4. Ore Noise (White noise / Cellular for scattering)
    auto fnOre = FastNoise::New<FastNoise::CellularValue>();
    fnOre->SetJitterModifier(1.0f);

    std::vector<float> elevationMap(size * size);
    std::vector<float> tempMap(size * size);
    std::vector<float> moistureMap(size * size);
    std::vector<float> oreMap(size * size);

    fnElevation->GenUniformGrid2D(elevationMap.data(), 0, 0, size, size, 0.02f,
                                  seed);
    fnTemperature->GenUniformGrid2D(tempMap.data(), 0, 0, size, size, 0.01f,
                                    seed + 1);
    fnMoisture->GenUniformGrid2D(moistureMap.data(), 0, 0, size, size, 0.05f,
                                 seed + 2);
    fnOre->GenUniformGrid2D(oreMap.data(), 0, 0, size, size, 0.2f, seed + 3);

    for(int y = 0; y < size; ++y) {
        for(int x = 0; x < size; ++x) {
            int idx = y * size + x;
            float h = elevationMap[idx]; // Range approx -1 to 1
            float t = tempMap[idx];      // Range approx -1 to 1
            float m = moistureMap[idx];  // Range approx -1 to 1
            float o = oreMap[idx];       // Range -1 to 1

            TileID terrain = TileID::Grass;
            TileID wall = TileID::Air;

            // Water Level
            if(h < -0.2f) {
                // It's water
                if(t < -0.4f) {
                    terrain = TileID::Ice; // Frozen lake
                } else if(t < 0.0f) {
                    terrain = TileID::ColdWater;
                } else {
                    terrain = TileID::Water;
                }
            }
            // Beach / Lowlands
            else if(h < -0.1f) {
                terrain = TileID::Gravel;
            }
            // Land
            else if(h < 0.6f) {
                if(t < -0.3f) {
                    terrain = TileID::Snow;
                } else if(t < 0.2f) {
                    terrain = TileID::ColdGrass;
                } else {
                    terrain = TileID::Grass;
                }
            }
            // Mountains
            else {
                if(h > 0.8f) terrain = TileID::HardStone;
                else terrain = TileID::Stone;

                // Ores (Walls)
                if(o > 0.85f) wall = TileID::RedOre;
                else if(o < -0.85f) wall = TileID::BlueOre;
                else wall = TileID::Stone; // Mountain wall
            }

            m_chunk.SetTerrain(x, y, terrain);
            m_chunk.SetWall(x, y, wall);
        }
    }
}

void Application::InitializePipeline() {
    wgpu::BufferDescriptor storageBufDesc;
    storageBufDesc.usage = wgpu::BufferUsage::Storage |
                           wgpu::BufferUsage::CopyDst;
    storageBufDesc.size = m_chunk.GetDisplayDataSize();
    m_tilemapBuffer = device.createBuffer(storageBufDesc);
    queue.writeBuffer(m_tilemapBuffer, 0, m_chunk.GetDisplayData(),
                      m_chunk.GetDisplayDataSize());

    wgpu::BufferDescriptor heightBufDesc;
    heightBufDesc.usage = wgpu::BufferUsage::Storage |
                          wgpu::BufferUsage::CopyDst;
    heightBufDesc.size = m_chunk.GetHeightDataSize();
    m_heightBuffer = device.createBuffer(heightBufDesc);
    queue.writeBuffer(m_heightBuffer, 0, m_chunk.GetHeightData(),
                      m_chunk.GetHeightDataSize());

    wgpu::TextureDescriptor hTexDesc;
    hTexDesc.label = "Heightmap Texture";
    hTexDesc.dimension = wgpu::TextureDimension::_2D;
    hTexDesc.size = {(uint32_t)m_chunk.GetSize(), (uint32_t)m_chunk.GetSize(),
                     1};
    hTexDesc.format = wgpu::TextureFormat::R32Float;
    hTexDesc.usage = wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::CopyDst;
    hTexDesc.mipLevelCount = 1;
    hTexDesc.sampleCount = 1;
    m_heightTexture = device.createTexture(hTexDesc);

    wgpu::ImageCopyTexture hDest;
    hDest.texture = m_heightTexture;
    wgpu::TextureDataLayout hSrc;
    hSrc.offset = 0;
    hSrc.bytesPerRow = Chunk::GetSize() * sizeof(float);
    hSrc.rowsPerImage = Chunk::GetSize();
    queue.writeTexture(hDest, m_chunk.GetHeightData(),
                       m_chunk.GetHeightDataSize(), hSrc, hTexDesc.size);

    m_heightTextureView = m_heightTexture.createView();

    wgpu::BufferDescriptor softnessBufDesc;
    softnessBufDesc.usage = wgpu::BufferUsage::Storage |
                            wgpu::BufferUsage::CopyDst;
    softnessBufDesc.size = m_chunk.GetSoftnessDataSize();
    m_softnessBuffer = device.createBuffer(softnessBufDesc);
    queue.writeBuffer(m_softnessBuffer, 0, m_chunk.GetSoftnessData(),
                      m_chunk.GetSoftnessDataSize());

    wgpu::BufferDescriptor uniformBufDesc;
    uniformBufDesc.usage = wgpu::BufferUsage::Uniform |
                           wgpu::BufferUsage::CopyDst;
    uniformBufDesc.size = sizeof(UniformData);
    m_uniformBuffer = device.createBuffer(uniformBufDesc);

    wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;

    WGSLPreprocessor preprocessor;

    preprocessor.addDefine("DERIVATIVE_NORMALS");

    std::string code = preprocessor.load("assets/shaders/terrain.wgsl");

    shaderCodeDesc.code = code.c_str();
    wgpu::ShaderModuleDescriptor shaderDesc;
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    std::vector<wgpu::BindGroupLayoutEntry> bgEntries(7);

    bgEntries[0].binding = 0;
    bgEntries[0].visibility = wgpu::ShaderStage::Fragment |
                              wgpu::ShaderStage::Vertex;
    bgEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bgEntries[0].buffer.minBindingSize = sizeof(UniformData);

    bgEntries[1].binding = 1;
    bgEntries[1].visibility = wgpu::ShaderStage::Fragment;
    bgEntries[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    bgEntries[1].buffer.minBindingSize = m_chunk.GetDisplayDataSize();

    bgEntries[2].binding = 2;
    bgEntries[2].visibility = wgpu::ShaderStage::Fragment;
    bgEntries[2].texture.sampleType = wgpu::TextureSampleType::Float;
    bgEntries[2].texture.viewDimension = wgpu::TextureViewDimension::_2D;

    bgEntries[3].binding = 3;
    bgEntries[3].visibility = wgpu::ShaderStage::Fragment;
    bgEntries[3].sampler.type = wgpu::SamplerBindingType::Filtering;

    bgEntries[4].binding = 4;
    bgEntries[4].visibility = wgpu::ShaderStage::Fragment;
    bgEntries[4].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    bgEntries[4].buffer.minBindingSize = m_chunk.GetHeightDataSize();

    bgEntries[5].binding = 5;
    bgEntries[5].visibility = wgpu::ShaderStage::Fragment;
    bgEntries[5].texture.sampleType =
        wgpu::TextureSampleType::UnfilterableFloat;
    bgEntries[5].texture.viewDimension = wgpu::TextureViewDimension::_2D;

    bgEntries[6].binding = 6;
    bgEntries[6].visibility = wgpu::ShaderStage::Fragment;
    bgEntries[6].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    bgEntries[6].buffer.minBindingSize = m_chunk.GetSoftnessDataSize();

    wgpu::BindGroupLayoutDescriptor bglDesc;
    bglDesc.entryCount = 7;
    bglDesc.entries = bgEntries.data();
    m_bindGroupLayout = device.createBindGroupLayout(bglDesc);

    wgpu::PipelineLayoutDescriptor layoutDesc;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(
        layoutDesc);

    std::vector<wgpu::BindGroupEntry> bgGroupEntries(7);
    bgGroupEntries[0].binding = 0;
    bgGroupEntries[0].buffer = m_uniformBuffer;
    bgGroupEntries[0].size = sizeof(UniformData);

    bgGroupEntries[1].binding = 1;
    bgGroupEntries[1].buffer = m_tilemapBuffer;
    bgGroupEntries[1].size = m_chunk.GetDisplayDataSize();

    bgGroupEntries[2].binding = 2;
    bgGroupEntries[2].textureView = m_textureView;

    bgGroupEntries[3].binding = 3;
    bgGroupEntries[3].sampler = m_sampler;

    bgGroupEntries[4].binding = 4;
    bgGroupEntries[4].buffer = m_heightBuffer;
    bgGroupEntries[4].size = m_chunk.GetHeightDataSize();

    bgGroupEntries[5].binding = 5;
    bgGroupEntries[5].textureView = m_heightTextureView;

    bgGroupEntries[6].binding = 6;
    bgGroupEntries[6].buffer = m_softnessBuffer;
    bgGroupEntries[6].size = m_chunk.GetSoftnessDataSize();

    wgpu::BindGroupDescriptor bgDesc;
    bgDesc.layout = m_bindGroupLayout;
    bgDesc.entryCount = 7;
    bgDesc.entries = bgGroupEntries.data();
    m_bindGroup = device.createBindGroup(bgDesc);

    wgpu::RenderPipelineDescriptor pipelineDesc;
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

    wgpu::FragmentState fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    wgpu::ColorTargetState colorTarget;
    colorTarget.format = surfaceFormat;

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::One;
    blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;

    pipeline = device.createRenderPipeline(pipelineDesc);
    shaderModule.release();
    pipelineLayout.release();
}

void Application::Terminate() {
    if(m_bindGroup) m_bindGroup.release();
    if(m_bindGroupLayout) m_bindGroupLayout.release();
    if(m_tilemapBuffer) m_tilemapBuffer.release();
    if(m_uniformBuffer) m_uniformBuffer.release();
    if(m_sampler) m_sampler.release();
    if(m_textureView) m_textureView.release();
    if(m_texture) m_texture.destroy();
    if(m_texture) m_texture.release();
    pipeline.release();
    surface.unconfigure();
    queue.release();
    surface.release();
    device.release();
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool Application::InitializeTexture() {
    int width, height, channels;
    unsigned char* pixelData = stbi_load("assets/atlas.png", &width, &height,
                                         &channels, 4);

    if(!pixelData) return false;

    wgpu::TextureDescriptor textureDesc;
    textureDesc.dimension = wgpu::TextureDimension::_2D;
    textureDesc.format = wgpu::TextureFormat::RGBA8UnormSrgb;
    textureDesc.size = {(uint32_t)width, (uint32_t)height, 1};
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.usage = wgpu::TextureUsage::TextureBinding |
                        wgpu::TextureUsage::CopyDst;

    m_texture = device.createTexture(textureDesc);

    wgpu::ImageCopyTexture destination;
    destination.texture = m_texture;
    destination.origin = {0, 0, 0};
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TextureDataLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * width;
    source.rowsPerImage = height;

    queue.writeTexture(destination, pixelData, width * height * 4, source,
                       textureDesc.size);

    stbi_image_free(pixelData);

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.format = textureDesc.format;
    viewDesc.dimension = wgpu::TextureViewDimension::_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    m_textureView = m_texture.createView(viewDesc);

    wgpu::SamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.magFilter = wgpu::FilterMode::Nearest;
    // Nearest for pixel art look
    samplerDesc.minFilter = wgpu::FilterMode::Nearest;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.maxAnisotropy = 1;

    m_sampler = device.createSampler(samplerDesc);

    return true;
}

void Application::OnCursorPos(GLFWwindow* window, double xpos, double ypos) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if(app) app->HandleMouseMove(xpos, ypos);
}

void Application::OnMouseButton(GLFWwindow* window, int button, int action,
                                int mods) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if(app) app->HandleMouseClick(button, action);
}

void Application::OnScroll(GLFWwindow* window, double xoffset, double yoffset) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if(app) app->HandleScroll(yoffset);
}

void Application::HandleMouseMove(double x, double y) {
    if(m_isDragging) {
        double dx = x - m_lastMouseX;
        double dy = y - m_lastMouseY;
        m_offsetX -= (float)dx / m_scale;
        m_offsetY -= (float)dy / m_scale;
    }
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void Application::HandleMouseClick(int button, int action) {
    if(button == GLFW_MOUSE_BUTTON_LEFT) {
        if(action == GLFW_PRESS) {
            m_isDragging = true;
            glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
        } else if(action == GLFW_RELEASE) {
            m_isDragging = false;
        }
    }
}

void Application::HandleScroll(double yoffset) {
    float mouseWorldX = ((float)m_lastMouseX / m_scale) + m_offsetX;
    float mouseWorldY = ((float)m_lastMouseY / m_scale) + m_offsetY;

    float zoomFactor = 1.1f;
    if(yoffset > 0) m_scale *= zoomFactor;
    else m_scale /= zoomFactor;

    if(m_scale < 0.01f) m_scale = 0.01f;
    if(m_scale > 64.0f) m_scale = 64.0f;

    m_offsetX = mouseWorldX - ((float)m_lastMouseX / m_scale);
    m_offsetY = mouseWorldY - ((float)m_lastMouseY / m_scale);
}

void Application::Update() {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    UniformData uData;
    uData.offsetX = m_offsetX;
    uData.offsetY = m_offsetY;
    uData.resX = (float)w;
    uData.resY = (float)h;
    uData.scale = m_scale;
    uData.mapSize = (float)m_chunk.GetSize();

    queue.writeBuffer(m_uniformBuffer, 0, &uData, sizeof(UniformData));
}

void Application::MainLoop() {
    glfwPollEvents();
    m_frameCount++;
    auto currentTime = std::chrono::steady_clock::now();

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - m_lastFpsTime).count();

    if(elapsedMs >= 1000) {
        std::string title = "WebGPU FastNoise2 Tilemap - FPS: " +
                            std::to_string(m_frameCount);
        glfwSetWindowTitle(window, title.c_str());

        m_frameCount = 0;
        m_lastFpsTime = currentTime;
    }

    Update();
    Render();
}

void Application::Render() {
    wgpu::TextureView targetView = GetNextSurfaceTextureView();
    if(!targetView) return;

    wgpu::CommandEncoder encoder = device.createCommandEncoder({});

    wgpu::RenderPassColorAttachment attachment = {};
    attachment.view = targetView;
    attachment.loadOp = wgpu::LoadOp::Clear;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = wgpu::Color{0.0, 0.0, 0.0, 1.0};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &attachment;

    wgpu::RenderPassEncoder renderPass = encoder.
        beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);
    renderPass.draw(6, 1, 0, 0);
    renderPass.end();
    renderPass.release();

    wgpu::CommandBuffer command = encoder.finish({});
    encoder.release();

    queue.submit(1, &command);
    command.release();
    targetView.release();

#ifndef __EMSCRIPTEN__
    surface.present();
#endif
}

void Application::OnFramebufferResize(GLFWwindow* window, int, int) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if(app && app->m_continuousResize) {
        app->Update();
        app->Render();
    }
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}

wgpu::TextureView Application::GetNextSurfaceTextureView() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if(width == 0 || height == 0) return nullptr;

    if(width != (int)surfaceConfig.width || height != (int)surfaceConfig.
       height) {
        surfaceConfig.width = width;
        surfaceConfig.height = height;
        surface.configure(surfaceConfig);
    }

    wgpu::SurfaceTexture surfaceTexture;
    surface.getCurrentTexture(&surfaceTexture);
    if(surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success)
        return nullptr;

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDesc.dimension = wgpu::TextureViewDimension::_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;

    WGPUTextureView targetView = wgpuTextureCreateView(
        surfaceTexture.texture, &viewDesc);

#ifndef WEBGPU_BACKEND_WGPU
    wgpuTextureRelease(surfaceTexture.texture);
#endif

    return targetView;
}