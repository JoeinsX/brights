#pragma once

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <glm/glm.hpp>

struct WindowConfig {
   glm::ivec2 position{100, 100};
   glm::ivec2 size{1280, 720};
};

class Window {
public:
   GLFWwindow* handle = nullptr;

   bool initialize(const WindowConfig& config, const char* title, void* userPointer) {
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
      glfwSetWindowUserPointer(handle, userPointer);
      return true;
   }

   void setCallbacks(GLFWframebuffersizefun resize, GLFWcursorposfun cursor, GLFWmousebuttonfun mouse, GLFWscrollfun scroll, GLFWkeyfun key) const {
      glfwSetFramebufferSizeCallback(handle, resize);
      glfwSetCursorPosCallback(handle, cursor);
      glfwSetMouseButtonCallback(handle, mouse);
      glfwSetScrollCallback(handle, scroll);
      glfwSetKeyCallback(handle, key);
   }

   [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(handle); }

   static void pollEvents() { glfwPollEvents(); }

   void terminate() const {
      glfwDestroyWindow(handle);
      glfwTerminate();
   }
};
