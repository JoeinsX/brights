#pragma once

#include "core/world/tile.hpp"
#include "core/world/tileInspection.hpp"

#include <imgui.h>
#include <optional>

class TileInspectorPanel {
public:
   void draw(const TileRegistry& registry, ImTextureID atlas, const std::optional<TileInspection>& info) {
      if (!visible) {
         return;
      }

      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      constexpr float pad = 10.0f;
      constexpr float width = 250.0f;
      ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - pad, viewport->WorkPos.y + pad}, ImGuiCond_Always, {1.0f, 0.0f});
      ImGui::SetNextWindowSizeConstraints({width, 0.0f}, {width, viewport->WorkSize.y - 2.0f * pad});

      if (!ImGui::Begin("Tile Inspector", &visible, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
         ImGui::End();
         return;
      }

      if (!info) {
         ImGui::TextDisabled("Hover a planet surface");
         ImGui::End();
         return;
      }

      const TileInspection& tile = *info;
      const TileDefinition& def = registry.get(tile.id);

      drawSprite(atlas, tile.atlasCoords);
      ImGui::SameLine();
      ImGui::BeginGroup();
      if (def.name.empty()) {
         ImGui::Text("Tile %d", static_cast<int>(tile.id));
      } else {
         ImGui::Text("%.*s", static_cast<int>(def.name.size()), def.name.data());
      }
      ImGui::TextDisabled("planet %d", tile.planet);
      ImGui::TextDisabled("tile %d, %d", tile.worldTile.x, tile.worldTile.y);
      ImGui::EndGroup();

      ImGui::Separator();

      if (!tile.loaded) {
         ImGui::TextColored({1.0f, 0.7f, 0.2f, 1.0f}, "chunk not loaded");
         ImGui::End();
         return;
      }

      ImGui::Text("Height");
      ImGui::SameLine(90.0f);
      ImGui::Text("%.3f", tile.height);
      ImGui::ProgressBar(tile.height / 2.0f, {-1.0f, 0.0f}, "");

      ImGui::Text("Softness");
      ImGui::SameLine(90.0f);
      ImGui::Text("%.2f", tile.softness);
      ImGui::ProgressBar(tile.softness, {-1.0f, 0.0f}, "");

      ImGui::Separator();
      ImGui::TextDisabled("chunk %d, %d   local %d, %d", tile.chunkPos.x, tile.chunkPos.y, tile.localTile.x, tile.localTile.y);
      ImGui::TextDisabled("atlas %d, %d", tile.atlasCoords.x, tile.atlasCoords.y);

      ImGui::Separator();
      ImGui::TextUnformatted("Render flags");
      if (tile.meshed) {
         drawFlag("Triplanar", tile.flags, RenderFlag::Triplanar);
         drawFlag("Blending", tile.flags, RenderFlag::Blending);
         drawFlag("Advanced raymarch", tile.flags, RenderFlag::AdvancedRaymarching);
         drawFlag("Skip raymarch", tile.flags, RenderFlag::SkipRaymarching);
      } else {
         ImGui::TextDisabled("  meshing...");
      }

      ImGui::End();
   }

   void toggle() { visible = !visible; }

private:
   static void drawSprite(ImTextureID atlas, const glm::ivec2 atlasCoords) {
      constexpr float size = 64.0f;
      constexpr float uvStep = 1.0f / 16.0f;
      const ImVec2 uv0(static_cast<float>(atlasCoords.x) * uvStep, static_cast<float>(atlasCoords.y) * uvStep);
      ImGui::Image(atlas, {size, size}, uv0, {uv0.x + uvStep, uv0.y + uvStep});
   }

   static void drawFlag(const char* label, const RenderFlag set, const RenderFlag bit) {
      const bool on = static_cast<bool>(set & bit);
      ImGui::TextColored(on ? ImVec4{0.45f, 1.0f, 0.45f, 1.0f} : ImVec4{0.5f, 0.5f, 0.5f, 1.0f}, "%s %s", on ? "[x]" : "[ ]", label);
   }

   bool visible = false;
};
