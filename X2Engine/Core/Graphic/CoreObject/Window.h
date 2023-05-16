#pragma once 

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// Make it Singleton!
class Window
{
public:
	Window(const uint16_t width, const uint16_t height, const char* title);
	~Window();

	void createSurface(const VkInstance& instance);

	const VkSurfaceKHR getSurface() const;
	void getResolutionInPixels(int& width, int& height) const;

	void destroy();
	void destroySurface(const VkInstance& instance);

	bool isWindowClosed() const;
	bool isAllowedToModifyTheResolution(const VkSurfaceCapabilitiesKHR& capabilities) const;

	void pollEvents();

	GLFWwindow* get();

private:

	// ImGui needs to access to non-const GLFWwindow.
	friend class GUI;
	GLFWwindow* m_window;
	VkSurfaceKHR m_surface;
};