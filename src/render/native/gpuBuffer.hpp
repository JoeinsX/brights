#pragma once
#include <webgpu/webgpu.hpp>

class GpuBuffer {
public:
   ~GpuBuffer() { destroy(); }

   void init(wgpu::Device& device, const uint64_t byteSize, const wgpu::BufferUsageFlags usage, const char* label = nullptr) {
      destroy();
      size = byteSize;
      wgpu::BufferDescriptor desc;
      desc.usage = usage;
      desc.size = size;
      desc.label = label;
      buffer = device.createBuffer(desc);
   }

   void destroy() {
      if (buffer) {
         buffer.destroy();
         buffer.release();
         buffer = nullptr;
      }
      size = 0;
   }

   [[nodiscard]] const wgpu::Buffer& getBuffer() const { return buffer; }

   [[nodiscard]] uint64_t getSize() const { return size; }

private:
   wgpu::Buffer buffer = nullptr;
   uint64_t size = 0;
};
