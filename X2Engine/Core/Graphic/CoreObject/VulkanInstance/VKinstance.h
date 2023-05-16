#pragma once

#include <string>

#include <vulkan/vulkan.h>

class VKinstance
{

public:

	VKinstance(const std::string& appName);
	~VKinstance();

	void destroy();
	VkInstance& get();

private:

	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;

};