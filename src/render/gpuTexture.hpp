#pragma once

#include "textureImage.hpp"

#include <algorithm>
#include <webgpu/webgpu.hpp>

class GpuTexture {
public:
   GpuTexture() = default;
   GpuTexture(const GpuTexture& other) = delete;
   GpuTexture(GpuTexture&& other) noexcept = delete;
   GpuTexture& operator =(const GpuTexture& other) = delete;
   GpuTexture& operator =(GpuTexture&& other) noexcept = delete;

   ~GpuTexture() { destroy(); }

   bool load(wgpu::Device& device, wgpu::Queue& queue, const std::string& path) {
      const TextureImage image(path);
      if (!image.isValid()) {
         return false;
      }

      static constexpr uint8_t mipLevelCount = 4;

      wgpu::TextureDescriptor textureDesc;
      textureDesc.dimension = wgpu::TextureDimension::_2D;
      textureDesc.format = wgpu::TextureFormat::RGBA8UnormSrgb;
      textureDesc.size = {static_cast<uint32_t>(image.getWidth()), static_cast<uint32_t>(image.getHeight()), 1};
      textureDesc.mipLevelCount = mipLevelCount;
      textureDesc.sampleCount = 1;
      textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
      texture = device.createTexture(textureDesc);

      uploadWithMipmaps(queue, image, textureDesc);

      wgpu::TextureViewDescriptor viewDesc;
      viewDesc.format = textureDesc.format;
      viewDesc.dimension = wgpu::TextureViewDimension::_2D;
      viewDesc.mipLevelCount = textureDesc.mipLevelCount;
      viewDesc.arrayLayerCount = 1;
      view = texture.createView(viewDesc);

      wgpu::SamplerDescriptor samplerDesc = {};
      samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
      samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
      samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
      samplerDesc.magFilter = wgpu::FilterMode::Nearest;
      samplerDesc.minFilter = wgpu::FilterMode::Nearest;
      samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
      samplerDesc.lodMinClamp = 0.0f;

      samplerDesc.lodMaxClamp = mipLevelCount;
      samplerDesc.maxAnisotropy = 1;
      sampler = device.createSampler(samplerDesc);

      return true;
   }

   void destroy() {
      if (sampler) {
         sampler.release();
         sampler = nullptr;
      }
      if (view) {
         view.release();
         view = nullptr;
      }
      if (texture) {
         texture.destroy();
         texture.release();
         texture = nullptr;
      }
   }

   [[nodiscard]] const wgpu::Texture& getTexture() const { return texture; }

   [[nodiscard]] const wgpu::TextureView& getView() const { return view; }

   [[nodiscard]] const wgpu::Sampler& getSampler() const { return sampler; }

private:
   void uploadWithMipmaps(wgpu::Queue& queue, const TextureImage& image, const wgpu::TextureDescriptor& textureDesc) {
      wgpu::ImageCopyTexture destination;
      destination.texture = texture;
      destination.origin = {0, 0, 0};
      destination.aspect = wgpu::TextureAspect::All;

      wgpu::TextureDataLayout source;
      source.offset = 0;

      wgpu::Extent3D mipLevelSize = textureDesc.size;
      wgpu::Extent3D prevMipLevelSize = textureDesc.size;
      std::vector<uint8_t> prevLevelPixels;

      auto srgbToLinear = [](const uint8_t val) -> float { return std::pow(static_cast<float>(val) / 255.0f, 2.2f); };
      auto linearToSrgb = [](const float val) -> uint8_t {
         const float v = std::pow(val, 1.0f / 2.2f);
         return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
      };

      for (uint32_t level = 0; level < textureDesc.mipLevelCount; ++level) {
         std::vector<uint8_t> pixels(4 * mipLevelSize.width * mipLevelSize.height);

         for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
            for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
               uint8_t* p = &pixels[4 * (j * mipLevelSize.width + i)];

               if (level == 0) {
                  const uint8_t* srcData = image.getData();
                  const size_t srcIdx = 4 * (j * mipLevelSize.width + i);
                  p[0] = srcData[srcIdx + 0];
                  p[1] = srcData[srcIdx + 1];
                  p[2] = srcData[srcIdx + 2];
                  p[3] = srcData[srcIdx + 3];
               } else {
                  const uint32_t prevStride = 4 * prevMipLevelSize.width;
                  // Sampling 2x2 block from previous level
                  const uint8_t* p00 = &prevLevelPixels[prevStride * (2 * j + 0) + 4 * (2 * i + 0)];
                  const uint8_t* p01 = &prevLevelPixels[prevStride * (2 * j + 0) + 4 * (2 * i + 1)];
                  const uint8_t* p10 = &prevLevelPixels[prevStride * (2 * j + 1) + 4 * (2 * i + 0)];
                  const uint8_t* p11 = &prevLevelPixels[prevStride * (2 * j + 1) + 4 * (2 * i + 1)];

                  const float r = (srgbToLinear(p00[0]) + srgbToLinear(p01[0]) + srgbToLinear(p10[0]) + srgbToLinear(p11[0])) / 4.0f;
                  const float g = (srgbToLinear(p00[1]) + srgbToLinear(p01[1]) + srgbToLinear(p10[1]) + srgbToLinear(p11[1])) / 4.0f;
                  const float b = (srgbToLinear(p00[2]) + srgbToLinear(p01[2]) + srgbToLinear(p10[2]) + srgbToLinear(p11[2])) / 4.0f;
                  const float a = (static_cast<float>(p00[3] + p01[3] + p10[3] + p11[3])) / 4.0f;

                  p[0] = linearToSrgb(r);
                  p[1] = linearToSrgb(g);
                  p[2] = linearToSrgb(b);
                  p[3] = static_cast<uint8_t>(a);
               }
            }
         }

         destination.mipLevel = level;
         source.bytesPerRow = 4 * mipLevelSize.width;
         source.rowsPerImage = mipLevelSize.height;

         queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

         prevMipLevelSize = mipLevelSize;
         prevLevelPixels = std::move(pixels);

         mipLevelSize.width = std::max(1u, mipLevelSize.width / 2);
         mipLevelSize.height = std::max(1u, mipLevelSize.height / 2);
      }
   }

   wgpu::Texture texture = nullptr;
   wgpu::TextureView view = nullptr;
   wgpu::Sampler sampler = nullptr;
};
