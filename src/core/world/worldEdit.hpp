#pragma once

#include "tile.hpp"

#include <cstdint>

enum class EditTool : uint8_t {
   TileBrush,
   HeightBrush,
   Trimmer
};

enum class TrimMode : uint8_t {
   Lower,
   Lift
};

struct BrushSettings {
   EditTool tool = EditTool::TileBrush;
   int radius = 3;
   float paintHeight = 1.0f;
   float heightRate = 1.0f;
   TileID tile = TileID::Grass;
   TrimMode trimMode = TrimMode::Lower;
   bool active = false;
};

struct EditStatus {
   bool locked = false;
   bool hit = false;
   glm::ivec2 tile{};
   int planet = -1;
   int lastPainted = 0;
};
