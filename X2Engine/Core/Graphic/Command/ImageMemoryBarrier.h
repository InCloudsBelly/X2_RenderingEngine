#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string>

class Image;

class ImageMemoryBarrier
{
	std::vector<VkImageMemoryBarrier> m_imageMemoryBarriers;
public:
	ImageMemoryBarrier(Image* image, std::string imageViewName, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags);
	ImageMemoryBarrier(Image* image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags);
	~ImageMemoryBarrier();
	const std::vector<VkImageMemoryBarrier>& getImageMemoryBarriers();
};