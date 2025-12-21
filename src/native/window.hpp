#pragma once

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

class Window {
public:
    GLFWwindow* handle = nullptr;

    bool Initialize(int width, int height, const char* title,
                    void* userPointer) {
        if(!glfwInit()) return false;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if(!handle) return false;

        glfwSetWindowUserPointer(handle, userPointer);
        return true;
    }

    void SetCallbacks(GLFWframebuffersizefun resize, GLFWcursorposfun cursor,
                      GLFWmousebuttonfun mouse, GLFWscrollfun scroll) {
        glfwSetFramebufferSizeCallback(handle, resize);
        glfwSetCursorPosCallback(handle, cursor);
        glfwSetMouseButtonCallback(handle, mouse);
        glfwSetScrollCallback(handle, scroll);
    }

    bool ShouldClose() { return glfwWindowShouldClose(handle); }
    void PollEvents() { glfwPollEvents(); }

    void Terminate() {
        glfwDestroyWindow(handle);
        glfwTerminate();
    }
};