#pragma once

#include "platform/window.hpp"
#include "render/gpuContext.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.hpp>

class Gui {
public:
   Gui() = default;
   Gui(const Gui&) = delete;
   Gui(Gui&&) = delete;
   Gui& operator =(const Gui&) = delete;
   Gui& operator =(Gui&&) = delete;

   ~Gui() {
      if (!ImGui::GetCurrentContext()) {
         return;
      }
      ImGui_ImplWGPU_Shutdown();
      ImGui_ImplGlfw_Shutdown();
      ImGui::DestroyContext();
   }

   void initialize(const Window& window, wgpu::Device device, wgpu::TextureFormat surfaceFormat) {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGui::StyleColorsDark();

      ImGui_ImplGlfw_InitForOther(window.getHandle(), true);

      ImGui_ImplWGPU_InitInfo info;
      info.Device = device;
      info.RenderTargetFormat = surfaceFormat;
      info.DepthStencilFormat = GpuContext::depthTextureFormat;
      ImGui_ImplWGPU_Init(&info);
   }

   void beginFrame() {
      ImGui_ImplWGPU_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
   }

   void render(wgpu::RenderPassEncoder pass) {
      ImGui::Render();
      ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
   }

   [[nodiscard]] bool consumeMouse() const { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse; }
   [[nodiscard]] bool consumeKeyboard() const { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard; }
};
