#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"


#include "X2/Core/Ref.h"

struct GLFWwindow;

namespace X2
{
	class VulkanContext : public RefCounted
	{
	public:
		VulkanContext();
		virtual ~VulkanContext();

		virtual void Init() ;

		Ref<VulkanDevice> GetDevice() { return m_Device; }

		static VkInstance GetInstance() { return s_VulkanInstance; }

		static Ref<VulkanContext> Get(); 
		static Ref<VulkanDevice> GetCurrentDevice() { return Get()->GetDevice(); }
	private:
		// Devices
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		Ref<VulkanDevice> m_Device;

		// Vulkan instance
		inline static VkInstance s_VulkanInstance;
#if 0
		VkDebugReportCallbackEXT m_DebugReportCallback = VK_NULL_HANDLE;
#endif
		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;
		VkPipelineCache m_PipelineCache = nullptr;

		VulkanSwapChain m_SwapChain;
	};

}