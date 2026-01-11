#pragma once

#include "../../world/worldGenerator.hpp"
#include "../wgslPreprocessor.hpp"
#include "render/textureImage.hpp"
#include "world/chunk.hpp"
#include "world/world.hpp"

namespace ShaderSlots {
   constexpr uint32_t Uniforms = 0;
   constexpr uint32_t TileMap = 1;
   constexpr uint32_t TextureAtlas = 2;
   constexpr uint32_t Sampler = 3;
   constexpr uint32_t PackedMap = 4;
   constexpr uint32_t ChunkRefMap = 5;
   constexpr uint32_t Num = 6;
}   // namespace ShaderSlots

struct UniformData {
   glm::ivec2 macroOffset;
   glm::vec2 offset;
   glm::vec2 res;
   float scale;
   uint32_t mapSize;
   float sphereMapScale;
   uint32_t chunkSize;
   glm::ivec2 chunkOffset;
   glm::vec2 resScale;
   float perspectiveStrength;
   float perspectiveScale;
};

class GraphicsContext {
public:
   wgpu::Device device = nullptr;
   wgpu::Queue queue = nullptr;
   wgpu::Surface surface = nullptr;
   wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
   wgpu::SurfaceConfiguration surfaceConfig = {};
   wgpu::RenderPipeline pipeline = nullptr;

   wgpu::Buffer tilemapBuffer = nullptr;
   wgpu::Buffer packedBuffer = nullptr;
   wgpu::Buffer chunkRefMapBuffer = nullptr;

   wgpu::Buffer uniformBuffer = nullptr;
   wgpu::Texture texture = nullptr;
   wgpu::TextureView textureView = nullptr;
   wgpu::Sampler sampler = nullptr;
   wgpu::BindGroup bindGroup = nullptr;
   wgpu::BindGroupLayout bindGroupLayout = nullptr;

   WorldRenderAdapter* worldRenderAdapter = nullptr;

   bool initialize (wgpu::Instance instance, GLFWwindow* window) {
      surface = glfwGetWGPUSurface (instance, window);

      wgpu::RequestAdapterOptions adapterOpts = {};
      adapterOpts.compatibleSurface = surface;
      wgpu::Adapter adapter = instance.requestAdapter (adapterOpts);

      wgpu::DeviceDescriptor deviceDesc = {};
      device = adapter.requestDevice (deviceDesc);
      queue = device.getQueue ();

      int w, h;
      glfwGetFramebufferSize (window, &w, &h);
      surfaceConfig.width = w;
      surfaceConfig.height = h;
      surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
      surfaceFormat = surface.getPreferredFormat (adapter);
      surfaceConfig.format = surfaceFormat;
      surfaceConfig.device = device;
      surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
      surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
      surface.configure (surfaceConfig);

      adapter.release ();
      return true;
   }

   bool initializeTexture () {
      TextureImage image ("assets/atlas.png");
      if (!image.isValid ()) {
         return false;
      }

      wgpu::TextureDescriptor textureDesc;
      textureDesc.dimension = wgpu::TextureDimension::_2D;
      textureDesc.format = wgpu::TextureFormat::RGBA8UnormSrgb;
      textureDesc.size = {static_cast<uint32_t> (image.getWidth ()), static_cast<uint32_t> (image.getHeight ()), 1};
      textureDesc.mipLevelCount = 4;
      textureDesc.sampleCount = 1;
      textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;

      texture = device.createTexture (textureDesc);

      wgpu::ImageCopyTexture destination;
      destination.texture = texture;
      destination.origin = {0, 0, 0};
      destination.aspect = wgpu::TextureAspect::All;

      wgpu::TextureDataLayout source;
      source.offset = 0;
      source.bytesPerRow = 4 * image.getWidth ();
      source.rowsPerImage = image.getHeight ();

      wgpu::Extent3D mipLevelSize = textureDesc.size;
      wgpu::Extent3D prevMipLevelSize = textureDesc.size;

      std::vector<uint8_t> previousLevelPixels;

      auto srgbToLinear = [] (uint8_t val) -> float {
         float v = val / 255.0f;
         return std::pow (v, 2.2f);
      };

      auto linearToSrgb = [] (float val) -> uint8_t {
         float v = std::pow (val, 1.0f / 2.2f);
         if (v > 1.0f) {
            v = 1.0f;
         }
         if (v < 0.0f) {
            v = 0.0f;
         }
         return static_cast<uint8_t> (v * 255.0f + 0.5f);
      };

      for (uint32_t level = 0; level < textureDesc.mipLevelCount; ++level) {
         std::vector<uint8_t> pixels (4 * mipLevelSize.width * mipLevelSize.height);

         for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
            for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
               uint8_t* p = &pixels[4 * (j * mipLevelSize.width + i)];

               if (level == 0) {
                  const uint8_t* srcData = image.getData ();
                  p[0] = srcData[4 * (j * mipLevelSize.width + i) + 0];
                  p[1] = srcData[4 * (j * mipLevelSize.width + i) + 1];
                  p[2] = srcData[4 * (j * mipLevelSize.width + i) + 2];
                  p[3] = srcData[4 * (j * mipLevelSize.width + i) + 3];
               } else {
                  uint32_t prevStride = 4 * prevMipLevelSize.width;

                  uint8_t* p00 = &previousLevelPixels[prevStride * (2 * j + 0) + 4 * (2 * i + 0)];
                  uint8_t* p01 = &previousLevelPixels[prevStride * (2 * j + 0) + 4 * (2 * i + 1)];
                  uint8_t* p10 = &previousLevelPixels[prevStride * (2 * j + 1) + 4 * (2 * i + 0)];
                  uint8_t* p11 = &previousLevelPixels[prevStride * (2 * j + 1) + 4 * (2 * i + 1)];

                  float r = (srgbToLinear (p00[0]) + srgbToLinear (p01[0]) + srgbToLinear (p10[0]) + srgbToLinear (p11[0])) / 4.0f;
                  float g = (srgbToLinear (p00[1]) + srgbToLinear (p01[1]) + srgbToLinear (p10[1]) + srgbToLinear (p11[1])) / 4.0f;
                  float b = (srgbToLinear (p00[2]) + srgbToLinear (p01[2]) + srgbToLinear (p10[2]) + srgbToLinear (p11[2])) / 4.0f;
                  float a = (p00[3] + p01[3] + p10[3] + p11[3]) / 4.0f;

                  p[0] = linearToSrgb (r);
                  p[1] = linearToSrgb (g);
                  p[2] = linearToSrgb (b);
                  p[3] = static_cast<uint8_t> (a);
               }
            }
         }

         destination.mipLevel = level;
         source.bytesPerRow = 4 * mipLevelSize.width;
         source.rowsPerImage = mipLevelSize.height;

         queue.writeTexture (destination, pixels.data (), pixels.size (), source, mipLevelSize);

         prevMipLevelSize = mipLevelSize;
         previousLevelPixels = std::move (pixels);

         mipLevelSize.width = std::max (1u, mipLevelSize.width / 2);
         mipLevelSize.height = std::max (1u, mipLevelSize.height / 2);
      }

      wgpu::TextureViewDescriptor viewDesc;
      viewDesc.format = textureDesc.format;
      viewDesc.dimension = wgpu::TextureViewDimension::_2D;
      viewDesc.mipLevelCount = textureDesc.mipLevelCount;
      viewDesc.arrayLayerCount = 1;
      textureView = texture.createView (viewDesc);

      wgpu::SamplerDescriptor samplerDesc = {};
      samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
      samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
      samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
      samplerDesc.magFilter = wgpu::FilterMode::Nearest;
      samplerDesc.minFilter = wgpu::FilterMode::Nearest;
      samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
      samplerDesc.lodMinClamp = 0.0f;
      samplerDesc.lodMaxClamp = 8.0f;
      samplerDesc.maxAnisotropy = 1;

      sampler = device.createSampler (samplerDesc);
      return true;
   }

   void initializePipeline (TileRegistry& registry, std::mt19937& rng, Camera& camera) {
      Chunk chunk{
         {0, 0}
      };
      const auto& displayData = chunk.getDisplayData ();

      const auto& packedData = chunk.getPackedData ();
      wgpu::BufferDescriptor storageBufDesc;
      storageBufDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
      storageBufDesc.size = displayData.size () * Chunk::COUNT_SQUARED_EX * sizeof (uint8_t);
      tilemapBuffer = device.createBuffer (storageBufDesc);

      wgpu::BufferDescriptor packedBufDesc;
      packedBufDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
      packedBufDesc.size = packedData.size () * Chunk::COUNT_SQUARED_EX * sizeof (uint16_t);
      packedBuffer = device.createBuffer (packedBufDesc);

      wgpu::BufferDescriptor chunkRefMapBufDesc;
      chunkRefMapBufDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
      chunkRefMapBufDesc.size = Chunk::COUNT_SQUARED * sizeof (uint32_t);
      chunkRefMapBuffer = device.createBuffer (chunkRefMapBufDesc);

      wgpu::BufferDescriptor uniformBufDesc;
      uniformBufDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
      uniformBufDesc.size = sizeof (UniformData);
      uniformBuffer = device.createBuffer (uniformBufDesc);

      WGSLPreprocessor preprocessor;
      std::string code = preprocessor.load ("assets/shaders/terrain/terrain.wgsl");
      wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
      shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
      shaderCodeDesc.code = code.c_str ();
      wgpu::ShaderModuleDescriptor shaderDesc;
      shaderDesc.nextInChain = &shaderCodeDesc.chain;
      wgpu::ShaderModule shaderModule = device.createShaderModule (shaderDesc);

      std::vector<wgpu::BindGroupLayoutEntry> bgEntries (ShaderSlots::Num);
      bgEntries[ShaderSlots::Uniforms].binding = ShaderSlots::Uniforms;
      bgEntries[ShaderSlots::Uniforms].visibility = wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex;
      bgEntries[ShaderSlots::Uniforms].buffer.type = wgpu::BufferBindingType::Uniform;
      bgEntries[ShaderSlots::Uniforms].buffer.minBindingSize = sizeof (UniformData);

      bgEntries[ShaderSlots::TileMap].binding = ShaderSlots::TileMap;
      bgEntries[ShaderSlots::TileMap].visibility = wgpu::ShaderStage::Fragment;
      bgEntries[ShaderSlots::TileMap].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
      bgEntries[ShaderSlots::TileMap].buffer.minBindingSize = displayData.size () * Chunk::COUNT_SQUARED_EX * sizeof (uint8_t);

      bgEntries[ShaderSlots::TextureAtlas].binding = ShaderSlots::TextureAtlas;
      bgEntries[ShaderSlots::TextureAtlas].visibility = wgpu::ShaderStage::Fragment;
      bgEntries[ShaderSlots::TextureAtlas].texture.sampleType = wgpu::TextureSampleType::Float;
      bgEntries[ShaderSlots::TextureAtlas].texture.viewDimension = wgpu::TextureViewDimension::_2D;

      bgEntries[ShaderSlots::Sampler].binding = ShaderSlots::Sampler;
      bgEntries[ShaderSlots::Sampler].visibility = wgpu::ShaderStage::Fragment;
      bgEntries[ShaderSlots::Sampler].sampler.type = wgpu::SamplerBindingType::Filtering;

      bgEntries[ShaderSlots::PackedMap].binding = ShaderSlots::PackedMap;
      bgEntries[ShaderSlots::PackedMap].visibility = wgpu::ShaderStage::Fragment;
      bgEntries[ShaderSlots::PackedMap].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
      bgEntries[ShaderSlots::PackedMap].buffer.minBindingSize = packedData.size () * Chunk::COUNT_SQUARED_EX * sizeof (uint16_t);

      bgEntries[ShaderSlots::ChunkRefMap].binding = ShaderSlots::ChunkRefMap;
      bgEntries[ShaderSlots::ChunkRefMap].visibility = wgpu::ShaderStage::Fragment;
      bgEntries[ShaderSlots::ChunkRefMap].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
      bgEntries[ShaderSlots::ChunkRefMap].buffer.minBindingSize = Chunk::COUNT_SQUARED * sizeof (uint32_t);

      wgpu::BindGroupLayoutDescriptor bglDesc;
      bglDesc.entryCount = ShaderSlots::Num;
      bglDesc.entries = bgEntries.data ();
      bindGroupLayout = device.createBindGroupLayout (bglDesc);

      wgpu::PipelineLayoutDescriptor layoutDesc;
      layoutDesc.bindGroupLayoutCount = 1;
      layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
      wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout (layoutDesc);

      std::vector<wgpu::BindGroupEntry> bgGroupEntries (ShaderSlots::Num);
      bgGroupEntries[ShaderSlots::Uniforms].binding = ShaderSlots::Uniforms;
      bgGroupEntries[ShaderSlots::Uniforms].buffer = uniformBuffer;
      bgGroupEntries[ShaderSlots::Uniforms].size = sizeof (UniformData);

      bgGroupEntries[ShaderSlots::TileMap].binding = ShaderSlots::TileMap;
      bgGroupEntries[ShaderSlots::TileMap].buffer = tilemapBuffer;
      bgGroupEntries[ShaderSlots::TileMap].size = displayData.size () * Chunk::COUNT_SQUARED_EX * sizeof (uint8_t);

      bgGroupEntries[ShaderSlots::TextureAtlas].binding = ShaderSlots::TextureAtlas;
      bgGroupEntries[ShaderSlots::TextureAtlas].textureView = textureView;

      bgGroupEntries[ShaderSlots::Sampler].binding = ShaderSlots::Sampler;
      bgGroupEntries[ShaderSlots::Sampler].sampler = sampler;

      bgGroupEntries[ShaderSlots::PackedMap].binding = ShaderSlots::PackedMap;
      bgGroupEntries[ShaderSlots::PackedMap].buffer = packedBuffer;
      bgGroupEntries[ShaderSlots::PackedMap].size = packedData.size () * Chunk::COUNT_SQUARED_EX * sizeof (uint16_t);

      bgGroupEntries[ShaderSlots::ChunkRefMap].binding = ShaderSlots::ChunkRefMap;
      bgGroupEntries[ShaderSlots::ChunkRefMap].buffer = chunkRefMapBuffer;
      bgGroupEntries[ShaderSlots::ChunkRefMap].size = Chunk::COUNT_SQUARED * sizeof (uint32_t);

      wgpu::BindGroupDescriptor bgDesc;
      bgDesc.layout = bindGroupLayout;
      bgDesc.entryCount = ShaderSlots::Num;
      bgDesc.entries = bgGroupEntries.data ();
      bindGroup = device.createBindGroup (bgDesc);

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

      pipeline = device.createRenderPipeline (pipelineDesc);
      shaderModule.release ();
      pipelineLayout.release ();
   }

   wgpu::TextureView getNextSurfaceTextureView (GLFWwindow* window) {
      int width, height;
      glfwGetFramebufferSize (window, &width, &height);
      if (width == 0 || height == 0) {
         return nullptr;
      }

      if (width != (int)surfaceConfig.width || height != (int)surfaceConfig.height) {
         surfaceConfig.width = width;
         surfaceConfig.height = height;
         surface.configure (surfaceConfig);
      }

      wgpu::SurfaceTexture surfaceTexture;
      surface.getCurrentTexture (&surfaceTexture);
      if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
         return nullptr;
      }

      wgpu::TextureViewDescriptor viewDesc;
      viewDesc.format = wgpuTextureGetFormat (surfaceTexture.texture);
      viewDesc.dimension = wgpu::TextureViewDimension::_2D;
      viewDesc.mipLevelCount = 1;
      viewDesc.arrayLayerCount = 1;

      return wgpuTextureCreateView (surfaceTexture.texture, &viewDesc);
   }

   void render (GLFWwindow* window) {
      wgpu::TextureView targetView = getNextSurfaceTextureView (window);
      if (!targetView) {
         return;
      }

      wgpu::CommandEncoder encoder = device.createCommandEncoder ({});
      wgpu::RenderPassColorAttachment attachment = {};
      attachment.view = targetView;
      attachment.loadOp = wgpu::LoadOp::Clear;
      attachment.storeOp = wgpu::StoreOp::Store;
      attachment.clearValue = wgpu::Color{0.0, 0.0, 0.0, 1.0};

      wgpu::RenderPassDescriptor renderPassDesc = {};
      renderPassDesc.colorAttachmentCount = 1;
      renderPassDesc.colorAttachments = &attachment;

      wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass (renderPassDesc);
      renderPass.setPipeline (pipeline);
      renderPass.setBindGroup (0, bindGroup, 0, nullptr);
      renderPass.draw (6, 1, 0, 0);
      renderPass.end ();
      renderPass.release ();

      wgpu::CommandBuffer command = encoder.finish ({});
      encoder.release ();
      queue.submit (1, &command);
      command.release ();
      targetView.release ();
      surface.present ();
   }

   void terminate () {
      if (bindGroup) {
         bindGroup.release ();
      }
      if (bindGroupLayout) {
         bindGroupLayout.release ();
      }
      if (tilemapBuffer) {
         tilemapBuffer.release ();
      }
      if (uniformBuffer) {
         uniformBuffer.release ();
      }
      if (sampler) {
         sampler.release ();
      }
      if (textureView) {
         textureView.release ();
      }
      if (texture) {
         texture.destroy ();
         texture.release ();
      }
      if (pipeline) {
         pipeline.release ();
      }
      if (surface) {
         surface.unconfigure ();
         surface.release ();
      }
      if (queue) {
         queue.release ();
      }
      if (device) {
         device.release ();
      }
   }
};
