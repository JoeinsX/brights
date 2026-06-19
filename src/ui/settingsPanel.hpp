#pragma once

#include "core/settings.hpp"

#include <imgui.h>

class SettingsPanel {
public:
   void draw(Settings& settings) {
      if (!visible) return;

      ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
      if (!ImGui::Begin("Settings", &visible)) {
         ImGui::End();
         return;
      }

      if (ImGui::CollapsingHeader("Perspective", ImGuiTreeNodeFlags_DefaultOpen)) {
         ImGui::SliderFloat("Strength", &settings.perspectiveStrength, 0.0f, 0.01f, "%.4f");
      }

      if (ImGui::CollapsingHeader("Raymarching Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
         ImGui::SliderFloat("Simple Threshold", &settings.simpleModeThreshold, 0.0f, 20.0f);
         ImGui::SliderInt("Max Tiles", &settings.raymarchMaxTiles, 1, 10);
         ImGui::SliderInt("Binary Steps", &settings.raymarchBinarySteps, 0, 12);
      }

      if (ImGui::CollapsingHeader("Features", ImGuiTreeNodeFlags_DefaultOpen)) {
         ImGui::TextDisabled("Toggling recompiles the shader");
         settings.featuresDirty |= ImGui::Checkbox("Raymarching", &settings.enableRaymarching);
         settings.featuresDirty |= ImGui::Checkbox("Soft Surface March", &settings.enableSoftMarch);
         settings.featuresDirty |= ImGui::Checkbox("Triplanar Texturing", &settings.enableTriplanar);
         settings.featuresDirty |= ImGui::Checkbox("Tile Blending", &settings.enableBlending);
      }

      if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
         const char* debugViews[] = {"Off", "Normals", "Height"};
         settings.featuresDirty |= ImGui::Combo("View", &settings.debugView, debugViews, IM_ARRAYSIZE(debugViews));
      }

      ImGui::End();
   }

   void toggle() { visible = !visible; }

private:
   bool visible = false;
};
