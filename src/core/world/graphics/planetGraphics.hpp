#pragma once

#include "core/world/chunk.hpp"
#include "core/world/graphics/shaderBindings.hpp"
#include "core/world/graphics/spriteInstance.hpp"
#include "render/gpuBuffer.hpp"
#include "render/gpuHelpers.hpp"
#include "render/gpuTexture.hpp"

#include <cstdint>
#include <vector>
#include <webgpu/webgpu.hpp>

class PlanetGraphics {
public:
   PlanetGraphics(wgpu::Device device, const wgpu::Queue queue, const wgpu::BindGroupLayout terrainLayout, const wgpu::BindGroupLayout spriteLayout, const GpuTexture& sharedAtlas,
                  const GpuTexture& sharedEntity): queue(queue) {
      const uint64_t tileMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED) * sizeof(uint8_t);
      const uint64_t packedMapSize = static_cast<uint64_t>(Chunk::SIZE_SQUARED * Chunk::COUNT_SQUARED) * sizeof(uint16_t);
      const uint64_t uniformSize = sizeof(UniformData);
      const uint64_t spriteBufferSize = static_cast<uint64_t>(totalSpriteCapacity) * sizeof(SpriteInstance);

      tilemapData.init(device, tileMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_TileMap");
      packedData.init(device, packedMapSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_PackedMap");
      uniformData.init(device, uniformSize, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "Planet_Uniforms");
      spriteData.init(device, spriteBufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "Planet_Sprites");

      std::vector<wgpu::BindGroupEntry> entries(ShaderSlots::Num);
      entries[ShaderSlots::Uniforms] = WGPUHelpers::bindBuffer(ShaderSlots::Uniforms, uniformData.getBuffer(), uniformSize);
      entries[ShaderSlots::TileMap] = WGPUHelpers::bindBuffer(ShaderSlots::TileMap, tilemapData.getBuffer(), tileMapSize);
      entries[ShaderSlots::TextureAtlas] = WGPUHelpers::bindTexture(ShaderSlots::TextureAtlas, sharedAtlas.getView());
      entries[ShaderSlots::Sampler] = WGPUHelpers::bindSampler(ShaderSlots::Sampler, sharedAtlas.getSampler());
      entries[ShaderSlots::PackedMap] = WGPUHelpers::bindBuffer(ShaderSlots::PackedMap, packedData.getBuffer(), packedMapSize);

      wgpu::BindGroupDescriptor bgDesc;
      bgDesc.layout = terrainLayout;
      bgDesc.entryCount = static_cast<uint32_t>(entries.size());
      bgDesc.entries = entries.data();
      terrainGroup = device.createBindGroup(bgDesc);

      std::vector<wgpu::BindGroupEntry> spriteEntries(SpriteSlots::Num);
      spriteEntries[SpriteSlots::Uniforms] = WGPUHelpers::bindBuffer(SpriteSlots::Uniforms, uniformData.getBuffer(), uniformSize);
      spriteEntries[SpriteSlots::Instances] = WGPUHelpers::bindBuffer(SpriteSlots::Instances, spriteData.getBuffer(), spriteBufferSize);
      spriteEntries[SpriteSlots::TextureAtlas] = WGPUHelpers::bindTexture(SpriteSlots::TextureAtlas, sharedEntity.getView());
      spriteEntries[SpriteSlots::Sampler] = WGPUHelpers::bindSampler(SpriteSlots::Sampler, sharedEntity.getSampler());

      wgpu::BindGroupDescriptor spriteBgDesc;
      spriteBgDesc.layout = spriteLayout;
      spriteBgDesc.entryCount = static_cast<uint32_t>(spriteEntries.size());
      spriteBgDesc.entries = spriteEntries.data();
      spriteGroup = device.createBindGroup(spriteBgDesc);
   }

   PlanetGraphics(const PlanetGraphics&) = delete;
   PlanetGraphics& operator =(const PlanetGraphics&) = delete;

   ~PlanetGraphics() {
      terrainGroup.release();
      spriteGroup.release();
   }

   void writeUniforms(const UniformData& uniforms) const { queue.writeBuffer(uniformData.getBuffer(), 0, &uniforms, sizeof(UniformData)); }

   [[nodiscard]] wgpu::Buffer packedBuffer() const { return packedData.getBuffer(); }
   [[nodiscard]] wgpu::Buffer tilemapBuffer() const { return tilemapData.getBuffer(); }
   [[nodiscard]] wgpu::Buffer spriteBuffer() const { return spriteData.getBuffer(); }
   [[nodiscard]] wgpu::BindGroup bindGroup() const { return terrainGroup; }
   [[nodiscard]] wgpu::BindGroup spriteBindGroup() const { return spriteGroup; }

private:
   GpuBuffer tilemapData;
   GpuBuffer packedData;
   GpuBuffer uniformData;
   GpuBuffer spriteData;

   wgpu::BindGroup terrainGroup = nullptr;
   wgpu::BindGroup spriteGroup = nullptr;

   wgpu::Queue queue = nullptr;
};
