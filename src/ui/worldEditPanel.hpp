#pragma once

#include "core/world/contents/tile.hpp"
#include "core/worldInteraction/worldEdit.hpp"

#include <algorithm>
#include <imgui.h>

class WorldEditPanel {
public:
   void draw(const TileRegistry& registry, ImTextureID atlas, const EditStatus& status) {
      if (!visible) {
         return;
      }

      ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
      if (!ImGui::Begin("World Edit", &visible)) {
         ImGui::End();
         return;
      }

      drawStatus(status);
      ImGui::Separator();

      drawToolSelector();
      ImGui::SliderInt("Radius", &brush.radius, 0, 16);

      switch (brush.tool) {
      case EditTool::TileBrush:
         ImGui::SliderFloat("Height", &brush.paintHeight, 0.0f, 2.0f, "%.2f");
         drawTilePalette(registry, atlas);
         break;
      case EditTool::HeightBrush:
         ImGui::SliderFloat("Rate /s", &brush.heightRate, -2.0f, 2.0f, "%.2f");
         ImGui::TextDisabled("Hold to raise; negative digs");
         break;
      case EditTool::Trimmer:
         drawTrimModeSelector();
         ImGui::TextWrapped(brush.trimMode == TrimMode::Lift ? "Lifts tiles lower than the cursor tile up to its height; taller tiles are untouched." :
                                                               "Lowers tiles higher than the cursor tile down to its height; shorter tiles are untouched.");
         break;
      }

      ImGui::End();
   }

   void toggle() { visible = !visible; }

   [[nodiscard]] WorldEditBrush settings() const {
      WorldEditBrush active = brush;
      active.active = visible;
      return active;
   }

private:
   void drawStatus(const EditStatus& status) {
      if (!status.locked) {
         ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Focus a planet (Tab) to edit");
         return;
      }
      ImGui::Text("Editing planet %d", status.planet);
      if (status.hit) {
         ImGui::SameLine();
         ImGui::TextDisabled("- tile (%d, %d)", status.tile.x, status.tile.y);
      } else {
         ImGui::SameLine();
         ImGui::TextDisabled("- cursor off planet");
      }
      if (status.lastPainted > 0) {
         ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "painting %d tiles", status.lastPainted);
      } else {
         ImGui::TextDisabled("drag over the surface to paint");
      }
   }

   void drawToolSelector() {
      static constexpr const char* tools[] = {"Tile Brush", "Height Brush", "Trimmer"};
      int current = static_cast<int>(brush.tool);
      if (ImGui::Combo("Tool", &current, tools, IM_ARRAYSIZE(tools))) {
         brush.tool = static_cast<EditTool>(current);
      }
   }

   void drawTrimModeSelector() {
      int mode = static_cast<int>(brush.trimMode);
      ImGui::RadioButton("Lower", &mode, static_cast<int>(TrimMode::Lower));
      ImGui::SameLine();
      ImGui::RadioButton("Lift", &mode, static_cast<int>(TrimMode::Lift));
      brush.trimMode = static_cast<TrimMode>(mode);
   }

   void drawTilePalette(const TileRegistry& registry, ImTextureID atlas) {
      static constexpr float cell = 40.0f;
      static constexpr float uvStep = 1.0f / 16.0f;

      const float advance = cell + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
      const int perRow = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / advance));

      int column = 0;
      for (const TileID id : registry.list()) {
         const TileDefinition& def = registry.get(id);
         const ImVec2 uv0(static_cast<float>(def.atlasBase.x) * uvStep, static_cast<float>(def.atlasBase.y) * uvStep);
         const ImVec2 uv1(uv0.x + uvStep, uv0.y + uvStep);

         if (column != 0) {
            ImGui::SameLine();
         }

         const bool selected = id == brush.tile;
         if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
         }

         ImGui::PushID(static_cast<int>(id));
         if (ImGui::ImageButton("tile", atlas, ImVec2(cell, cell), uv0, uv1)) {
            brush.tile = id;
         }
         ImGui::PopID();

         if (selected) {
            ImGui::PopStyleColor();
         }
         if (ImGui::IsItemHovered() && !def.name.empty()) {
            ImGui::SetTooltip("%.*s", static_cast<int>(def.name.size()), def.name.data());
         }

         if (++column == perRow) {
            column = 0;
         }
      }
   }

   bool visible = false;
   WorldEditBrush brush{};
};
