#pragma once

#include "core/graphics/renderSettings.hpp"

#include <imgui.h>

class SettingsPanel {
public:
   bool draw(RenderSettings& settings) {
      if (!visible) return false;

      ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
      if (!ImGui::Begin("Settings", &visible)) {
         ImGui::End();
         return false;
      }

      bool changed = false;

      if (ImGui::CollapsingHeader("Perspective", ImGuiTreeNodeFlags_DefaultOpen)) {
         changed |= ImGui::SliderFloat("Strength", &settings.perspectiveStrength, 0.0f, 0.01f, "%.4f");
      }

      if (ImGui::CollapsingHeader("Raymarching Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
         changed |= ImGui::SliderFloat("Simple Threshold", &settings.simpleModeThreshold, 0.0f, 20.0f);
         changed |= ImGui::SliderInt("Max Tiles", &settings.raymarchMaxTiles, 1, 10);
         changed |= ImGui::SliderInt("Binary Steps", &settings.raymarchBinarySteps, 0, 12);
      }

      if (ImGui::CollapsingHeader("Features", ImGuiTreeNodeFlags_DefaultOpen)) {
         ImGui::TextDisabled("Toggling recompiles the shader");
         changed |= ImGui::Checkbox("Raymarching", &settings.enableRaymarching);
         changed |= ImGui::Checkbox("Soft Surface March", &settings.enableSoftMarch);
         changed |= ImGui::Checkbox("Triplanar Texturing", &settings.enableTriplanar);
         changed |= ImGui::Checkbox("Tile Blending", &settings.enableBlending);
      }

      if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
         const char* debugViews[] = {"Off", "Normals", "Height"};
         changed |= ImGui::Combo("View", &settings.debugView, debugViews, IM_ARRAYSIZE(debugViews));
      }

      ImGui::End();
      return changed;
   }

   void toggle() { visible = !visible; }

private:
   bool visible = false;
};
