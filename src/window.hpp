#pragma once

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

class Window {
public:
    Window(int width, int height, const char* title) {
        if(!glfwInit()) {
            throw std::runtime_error("Could not initialize GLFW!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if(!m_window) {
            glfwTerminate();
            throw std::runtime_error("Could not create GLFW window!");
        }
    }

    ~Window() {
        if(m_window) {
            glfwDestroyWindow(m_window);
        }
        glfwTerminate();
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const {
        return glfwWindowShouldClose(m_window);
    }

    void pollEvents() const {
        glfwPollEvents();
    }

    GLFWwindow* getNativeHandle() const {
        return m_window;
    }

private:
    GLFWwindow* m_window = nullptr;
};