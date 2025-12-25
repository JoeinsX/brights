#pragma once

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

class Window {
public:
    GLFWwindow* handle = nullptr;

    bool initialize(int width, int height, const char* title,
                    void* userPointer) {
        if(!glfwInit()) return false;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        handle = glfwCreateWindow(width, height, title, nullptr, nullptr);

        glfwSetWindowPos(handle, 1250, 600);
        if(!handle) return false;

        glfwSetWindowUserPointer(handle, userPointer);
        return true;
    }

    void setCallbacks(GLFWframebuffersizefun resize, GLFWcursorposfun cursor,
                      GLFWmousebuttonfun mouse, GLFWscrollfun scroll) const {
        glfwSetFramebufferSizeCallback(handle, resize);
        glfwSetCursorPosCallback(handle, cursor);
        glfwSetMouseButtonCallback(handle, mouse);
        glfwSetScrollCallback(handle, scroll);
    }

    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(handle); }

    bool pollEvents() {
        glfwPollEvents();
        return true;
    }

    void terminate() const {
        glfwDestroyWindow(handle);
        glfwTerminate();
    }
};