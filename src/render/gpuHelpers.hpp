#pragma once
#include <webgpu/webgpu.hpp>

namespace WGPUHelpers {
   inline wgpu::BindGroupLayoutEntry bufferEntry(const uint32_t binding, const wgpu::ShaderStageFlags visibility, const wgpu::BufferBindingType type, const uint64_t minSize = 0) {
      wgpu::BindGroupLayoutEntry entry = {};
      entry.binding = binding;
      entry.visibility = visibility;
      entry.buffer.type = type;
      entry.buffer.minBindingSize = minSize;
      return entry;
   }

   inline wgpu::BindGroupLayoutEntry textureEntry(const uint32_t binding, const wgpu::ShaderStageFlags visibility,
                                                  const wgpu::TextureSampleType sampleType = wgpu::TextureSampleType::Float,
                                                  const wgpu::TextureViewDimension viewDimension = wgpu::TextureViewDimension::_2D) {
      wgpu::BindGroupLayoutEntry entry = {};
      entry.binding = binding;
      entry.visibility = visibility;
      entry.texture.sampleType = sampleType;
      entry.texture.viewDimension = viewDimension;
      return entry;
   }

   inline wgpu::BindGroupLayoutEntry samplerEntry(const uint32_t binding, const wgpu::ShaderStageFlags visibility,
                                                  const wgpu::SamplerBindingType type = wgpu::SamplerBindingType::Filtering) {
      wgpu::BindGroupLayoutEntry entry = {};
      entry.binding = binding;
      entry.visibility = visibility;
      entry.sampler.type = type;
      return entry;
   }

   inline wgpu::BindGroupEntry bindBuffer(const uint32_t binding, const wgpu::Buffer buffer, const uint64_t size, const uint64_t offset = 0) {
      wgpu::BindGroupEntry entry = {};
      entry.binding = binding;
      entry.buffer = buffer;
      entry.size = size;
      entry.offset = offset;
      return entry;
   }

   inline wgpu::BindGroupEntry bindTexture(const uint32_t binding, const wgpu::TextureView view) {
      wgpu::BindGroupEntry entry = {};
      entry.binding = binding;
      entry.textureView = view;
      return entry;
   }

   inline wgpu::BindGroupEntry bindSampler(const uint32_t binding, const wgpu::Sampler sampler) {
      wgpu::BindGroupEntry entry = {};
      entry.binding = binding;
      entry.sampler = sampler;
      return entry;
   }
}   // namespace WGPUHelpers
