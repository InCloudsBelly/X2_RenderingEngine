#include "Window.h"

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

Window::Window(
    const uint16_t width,
    const uint16_t height,
    const char* title
) {
    glfwInit();
    // Since GLFW automatically creates an OpenGL context,
    // we need to not create it.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // ----------------------------------------------------------------
    // Paramet. #1 -> Width.
    // Paramet. #2 -> Height.
    // Paramet. #3 -> Title.
    // Paramet. #4 -> Specifies in which monitor to open the window.
    // Paramet. #5 -> Irrelevant to Vulkan.
    // ----------------------------------------------------------------
    m_window = glfwCreateWindow(
        width,
        height,
        title,
        nullptr,
        nullptr
    );
}

const VkSurfaceKHR Window::getSurface() const
{
    return m_surface;
}

GLFWwindow* Window::get()
{
    return m_window;
}

void Window::getResolutionInPixels(int& width, int& height) const
{
    glfwGetFramebufferSize(m_window, &width, &height);
}

void Window::createSurface(const VkInstance& instance)
{
    auto status = glfwCreateWindowSurface(instance, m_window, nullptr, &m_surface);

    if (status != VK_SUCCESS)
        throw std::runtime_error("Failed to create a window surface");
}

bool Window::isWindowClosed() const
{
    return glfwWindowShouldClose(m_window);
}

bool Window::isAllowedToModifyTheResolution(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    return (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max());
}

void Window::pollEvents()
{
    glfwPollEvents();
}

void Window::destroySurface(const VkInstance& instance)
{
    vkDestroySurfaceKHR(instance, m_surface, nullptr);
}

void Window::destroy()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

Window::~Window() {}