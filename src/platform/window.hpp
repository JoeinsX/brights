#pragma once

#include "app/input/event.hpp"
#include "app/settings/settings.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct WindowSettings {
   glm::ivec2 position{100, 100};
   glm::ivec2 size{1280, 720};

   WindowSettings() = default;

   static constexpr const char* key = "window";

   template<typename Self, typename Fn>
   static void forEachField(Self& self, Fn&& fn) {
      fn("position", self.position);
      fn("size", self.size);
   }
};

class Window {
public:
   Window() = default;
   Window(const Window&) = delete;
   Window(Window&&) = delete;
   Window& operator =(const Window&) = delete;
   Window& operator =(Window&&) = delete;

   ~Window() {
      glfwDestroyWindow(handle);
      glfwTerminate();
   }

   void initAppComponent(Settings& settings) { settings.addSection<WindowSettings>(); }

   bool initialize(const WindowSettings& config, const char* title) {
      if (!glfwInit()) {
         return false;
      }
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
      glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
      handle = glfwCreateWindow(config.size.x, config.size.y, title, nullptr, nullptr);
      if (!handle) {
         return false;
      }

      glfwSetWindowPos(handle, config.position.x, config.position.y);
      glfwSetWindowUserPointer(handle, this);
      installCallbacks();
      return true;
   }

   void pollEvents() {
      eventQueue.clear();
      glfwPollEvents();
   }

   [[nodiscard]] const std::vector<Event>& events() const { return eventQueue; }

   [[nodiscard]] glm::ivec2 framebufferSize() const {
      glm::ivec2 size{};
      glfwGetFramebufferSize(handle, &size.x, &size.y);
      return size;
   }

   void setTitle(const std::string& title) const { glfwSetWindowTitle(handle, title.c_str()); }

   [[nodiscard]] GLFWwindow* getHandle() const { return handle; }

   [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(handle); }

private:
   void installCallbacks() {
      glfwSetCursorPosCallback(handle, [](GLFWwindow* w, double x, double y) { accessEventQueue(w).push_back(MouseMoved{{static_cast<float>(x), static_cast<float>(y)}}); });

      glfwSetMouseButtonCallback(handle, [](GLFWwindow* w, int button, int action, int) {
         const MouseButton mapped = mapMouseButton(button);
         if (mapped == MouseButton::Count) {
            return;
         }
         double x{}, y{};
         glfwGetCursorPos(w, &x, &y);
         accessEventQueue(w).push_back(MouseButtonEvent{mapped, action == GLFW_PRESS, {static_cast<float>(x), static_cast<float>(y)}});
      });

      glfwSetScrollCallback(handle, [](GLFWwindow* w, double x, double y) { accessEventQueue(w).push_back(Scrolled{{static_cast<float>(x), static_cast<float>(y)}}); });

      glfwSetKeyCallback(handle, [](GLFWwindow* w, int key, int, int action, int) {
         const Key mapped = mapKey(key);
         if (mapped == Key::Count || action == GLFW_REPEAT) {
            return;
         }
         accessEventQueue(w).push_back(KeyEvent{mapped, action == GLFW_PRESS});
      });
   }

   static std::vector<Event>& accessEventQueue(GLFWwindow* w) { return static_cast<Window*>(glfwGetWindowUserPointer(w))->eventQueue; }

   static MouseButton mapMouseButton(const int button) {
      switch (button) {
      case GLFW_MOUSE_BUTTON_LEFT:   return MouseButton::Left;
      case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
      case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
      default:                       return MouseButton::Count;
      }
   }

   static Key mapKey(const int key) {
      switch (key) {
      case GLFW_KEY_W:   return Key::W;
      case GLFW_KEY_A:   return Key::A;
      case GLFW_KEY_S:   return Key::S;
      case GLFW_KEY_D:   return Key::D;
      case GLFW_KEY_TAB: return Key::Tab;
      case GLFW_KEY_F1:  return Key::F1;
      case GLFW_KEY_F2:  return Key::F2;
      case GLFW_KEY_F3:  return Key::F3;
      default:           return Key::Count;
      }
   }

   GLFWwindow* handle = nullptr;
   std::vector<Event> eventQueue;
};
