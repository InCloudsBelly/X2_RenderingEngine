#pragma once

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>


struct NewSwapchainSupportedProperties
{
	// Basic surface capabilities:
	// - Min/max number of images in swap chain.
	// - Min/max width and height of images.
	VkSurfaceCapabilitiesKHR capabilities;
	// Surface formats:
	//    - Pixel format ->
	//    - Color space  -> 
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	// Presentation modes: 
	//    The logic of how the images will be diplayed on the screen.
	std::vector<VkPresentModeKHR> presentModes;
};

class Semaphore;
struct Queue;

class Swapchain
{
public:
	Swapchain();
	~Swapchain();

	void presentImage(const uint32_t imageIndex, const std::vector<Semaphore*> signalSemaphores, Queue* presentQueue);

	void destroy();

	const uint32_t getNextImageIndex(const VkSemaphore& semaphore) const;

	const VkExtent2D& getExtent() const;
	const VkFormat& getImageFormat() const;
	VkViewport& getViewport() { return m_viewport; }
	VkRect2D& getScissor() { return m_scissor; }
	std::vector<VkImageView*> getImageViews() { return m_imageViews; }

	const VkSwapchainKHR& get() const;
	const uint32_t getImageCount() const;
	const uint32_t getMinImageCount() const;

	VkImageView* getImageView(const uint32_t index);

private:
	void findSupportedProperties();

	void chooseBestSettings(VkSurfaceFormatKHR& surfaceFormat, VkPresentModeKHR& presentMode, VkExtent2D& extent);

	void createAllImageViews();

	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR chooseBestPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D chooseBestExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	const bool existsMaxNumberOfSupportedImages(const VkSurfaceCapabilitiesKHR& capabilities)const;


private:
	VkSwapchainKHR             m_swapchain;
	VkFormat                   m_imageFormat;
	VkExtent2D                 m_extent;
	VkViewport				   m_viewport;
	VkRect2D				   m_scissor;
	std::vector<VkImage>       m_images;
	std::vector<VkImageView*>  m_imageViews;

	NewSwapchainSupportedProperties m_swapchainProperties;

	// Used for the creation of the Imgui instance.
	uint32_t                   m_minImageCount;
};