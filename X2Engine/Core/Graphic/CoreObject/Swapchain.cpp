#include "Swapchain.h"

#include <algorithm>
#include <iostream>

#include <vulkan/vulkan.h>
#include <stdexcept>

#include "Core/Graphic/CoreObject/Window.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Instance/Image.h"
#include "Core/Graphic/Command/Semaphore.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"


Swapchain::~Swapchain() 
{
	vkDestroySwapchainKHR(Instance::getDevice(), m_swapchain, nullptr);

	for (auto& imageView : m_imageViews)
		vkDestroyImageView(Instance::getDevice(), *imageView, nullptr);
}

Swapchain::Swapchain()
{
	VkSurfaceFormatKHR surfaceFormat;
	VkPresentModeKHR presentMode;
	VkExtent2D extent;

	findSupportedProperties();

	chooseBestSettings(surfaceFormat, presentMode, extent);

	m_imageFormat = surfaceFormat.format;
	m_extent = extent;
	m_viewport = VkViewport{ 0.0f, 0.0f, (float)m_extent.width, (float)m_extent.height, 0.0f, 1.0f };
	m_scissor = VkRect2D{ {0,0}, {extent.width,extent.height} };

	// Chooses how many images we want to have in the swap chain.
	// (It's always recommended to request at least one more image that the
	// minimum because if we stick to this minimum, it means that we may
	// sometimes have to wait on the drive to complete internal operations
	// before we can acquire another imager to render to)
	m_minImageCount = (m_swapchainProperties.capabilities.minImageCount);
	uint32_t imageCount = m_minImageCount + 1;

	bool isMaxResolution = existsMaxNumberOfSupportedImages(m_swapchainProperties.capabilities);
	if (isMaxResolution == true && imageCount > m_swapchainProperties.capabilities.maxImageCount)
	{
		imageCount = m_swapchainProperties.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = Instance::getWindow()->getSurface();
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT; //need for copying to host

	//graphicsFamily == presentFamily == 0 
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// Optinal
	createInfo.queueFamilyIndexCount = 0;
	// Optional
	createInfo.pQueueFamilyIndices = nullptr;


	//uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };
	//if (indices.graphicsFamily != indices.presentFamily)
	//{
	//	createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	//	createInfo.queueFamilyIndexCount = 2;
	//	createInfo.pQueueFamilyIndices = queueFamilyIndices;
	//}
	//else
	//{
	//	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//	// Optinal
	//	createInfo.queueFamilyIndexCount = 0;
	//	// Optional
	//	createInfo.pQueueFamilyIndices = nullptr;
	//}
	// We can specify that a certain transform should be applied to images in
	// the swap chain if it's supported(supportedTransofrms in capabilities),
	// like a 90 degree clockwsie rotation or horizontal flip.
	createInfo.preTransform = (m_swapchainProperties.capabilities.currentTransform);

	// Specifies if the alpha channel should be used for mixing with other
   // windows in the window sysem. We'll almost always want to simply ignore
   // the alpha channel:
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.presentMode = presentMode;
	// We don't care abou the color pixels that are obscured, for example
	// because another window is in front of them. Unless you really need to be
	// able to read these pixels back an get predictable results, we'll get the
	// best performance by enabling clipping.
	createInfo.clipped = VK_TRUE;

	// Configures the old swapchain when the actual one becomes invaled or
	// unoptimized while the app is running(for example because the window was
	// resized).
	// For now we'll assume that we'll only ever create one swapchain.
	createInfo.oldSwapchain = VK_NULL_HANDLE;



	if (vkCreateSwapchainKHR(Instance::getDevice(), &createInfo, nullptr, &m_swapchain) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the Swapchain!");

	vkGetSwapchainImagesKHR(Instance::getDevice(), m_swapchain, &imageCount, nullptr);
	m_images.resize(imageCount);
	vkGetSwapchainImagesKHR(Instance::getDevice(), m_swapchain, &imageCount, m_images.data());



	createAllImageViews();
}

void Swapchain::destroy()
{
	
}

void  Swapchain::findSupportedProperties()
{
	// - Surface capabilities.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Instance::getPhysicalDevice(), Instance::getWindow()->getSurface(), &m_swapchainProperties.capabilities);

	// - Surface formats
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(Instance::getPhysicalDevice(), Instance::getWindow()->getSurface(), &formatCount, nullptr);

	if (formatCount != 0)
	{
		m_swapchainProperties.surfaceFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(Instance::getPhysicalDevice(), Instance::getWindow()->getSurface(), &formatCount, m_swapchainProperties.surfaceFormats.data());
	}

	// - Surface Presentation Modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(Instance::getPhysicalDevice(), Instance::getWindow()->getSurface(), &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		m_swapchainProperties.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(Instance::getPhysicalDevice(), Instance::getWindow()->getSurface(), &presentModeCount, m_swapchainProperties.presentModes.data());
	}
}


void Swapchain::createAllImageViews()
{
	m_imageViews.resize(m_images.size());

	for (uint32_t i = 0; i < m_images.size(); i++)
	{
		m_imageViews[i] = new VkImageView;

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_imageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;


		vkCreateImageView(Instance::getDevice(), &viewInfo, nullptr, m_imageViews[i]);
	}
}


const uint32_t Swapchain::getNextImageIndex(const VkSemaphore& semaphore) const
{
	uint32_t imageIndex;

	vkAcquireNextImageKHR(
		Instance::getDevice(),
		m_swapchain,
		UINT64_MAX,
		// Specifies synchr. objects that have to be signaled when the
		// presentation engine is finished using the image.
		semaphore,
		VK_NULL_HANDLE,
		&imageIndex
	);

	return imageIndex;
}

VkImageView* Swapchain::getImageView(const uint32_t index) {
	return m_imageViews[index];
}

void Swapchain::chooseBestSettings(
	VkSurfaceFormatKHR& surfaceFormat,
	VkPresentModeKHR& presentMode,
	VkExtent2D& extent)
{
	surfaceFormat = chooseBestSurfaceFormat(m_swapchainProperties.surfaceFormats);
	presentMode = chooseBestPresentMode(m_swapchainProperties.presentModes);
	extent = chooseBestExtent(m_swapchainProperties.capabilities);
}


VkSurfaceFormatKHR Swapchain::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		// sRGB -> gamma correction.
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

VkPresentModeKHR Swapchain::chooseBestPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return availablePresentMode;
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseBestExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (Instance::getWindow()->isAllowedToModifyTheResolution(capabilities) == false)
		return capabilities.currentExtent;

	int width, height;
	Instance::getWindow()->getResolutionInPixels(width, height);

	VkExtent2D actualExtent = { static_cast<uint32_t>(width),static_cast<uint32_t>(height) };

	actualExtent.width = std::clamp(
		actualExtent.width,
		capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width
	);
	actualExtent.height = std::clamp(
		actualExtent.height,
		capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height
	);


	return actualExtent;
}


const VkExtent2D& Swapchain::getExtent() const
{
	return m_extent;
}


const VkFormat& Swapchain::getImageFormat() const
{
	return m_imageFormat;
}

const VkSwapchainKHR& Swapchain::get() const
{
	return m_swapchain;
}


void Swapchain::presentImage(const uint32_t imageIndex, const std::vector<Semaphore*> signalSemaphores,Queue* presentQueue)
{
	std::vector <VkSemaphore> signal = std::vector <VkSemaphore>(signalSemaphores.size());
	for (uint32_t i = 0; i < signalSemaphores.size(); i++)
	{
		signal[i] = signalSemaphores[i]->getSemphore();
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	// Specifies which semaphores to wait on before the presentation can happen.
	presentInfo.waitSemaphoreCount = signal.size();
	presentInfo.pWaitSemaphores = signal.data();

	// Specifies the swapchains to present images to and the index of the
	// image for each swapchain.
	//VkSwapchainKHR swapchains[] = {m_swapchain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	// Allows us to specify an array of VkResult values to check for every
	// individual swapchain if presentation was successful.
	// Optional
	presentInfo.pResults = nullptr;

	vkQueuePresentKHR(presentQueue->queue, &presentInfo);
}

const bool Swapchain::existsMaxNumberOfSupportedImages(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	return (capabilities.maxImageCount != 0);
}

const uint32_t Swapchain::getImageCount() const
{
	return m_images.size();
}

const uint32_t Swapchain::getMinImageCount() const
{
	return m_minImageCount;
}
