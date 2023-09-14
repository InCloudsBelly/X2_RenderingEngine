#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"


#include "X2/Core/Ref.h"

struct GLFWwindow;

namespace X2
{
	class VulkanContext 
	{
	public:
		VulkanContext();
		virtual ~VulkanContext();

		virtual void Init() ;
		void Destroy();

		VulkanDevice* GetDevice() { return m_Device.get(); }

		static VkInstance GetInstance() { return s_VulkanInstance; }

		static VulkanContext* Get(); 
		static VulkanDevice* GetCurrentDevice() { return Get()->GetDevice(); }
	private:
		// Devices
		Scope<VulkanPhysicalDevice> m_PhysicalDevice;
		Scope<VulkanDevice> m_Device;

		// Vulkan instance
		inline static VkInstance s_VulkanInstance;
#if 0
		VkDebugReportCallbackEXT m_DebugReportCallback = VK_NULL_HANDLE;
#endif
		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;

		VulkanSwapChain m_SwapChain;
	};

}