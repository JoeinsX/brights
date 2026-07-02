#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <variant>

enum class Key : uint8_t {
   W,
   A,
   S,
   D,
   Tab,
   Space,
   F1,
   F2,
   F3,
   Count
};

enum class MouseButton : uint8_t {
   Left,
   Right,
   Middle,
   Count
};

struct MouseMoved {
   glm::vec2 position;
};

struct MouseButtonEvent {
   MouseButton button;
   bool pressed;
   glm::vec2 position;
};

struct Scrolled {
   glm::vec2 delta;
};

struct KeyEvent {
   Key key;
   bool pressed;
};

using Event = std::variant<MouseMoved, MouseButtonEvent, Scrolled, KeyEvent>;
