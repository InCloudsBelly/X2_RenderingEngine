#include "ImageMemoryBarrier.h"
#include "Core/Graphic/Instance/Image.h"
#include <string>

ImageMemoryBarrier::ImageMemoryBarrier(Image* image, std::string imageViewName, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags)
{
	m_imageMemoryBarriers.push_back({});
	auto& barrier = m_imageMemoryBarriers.back();
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->getImage();
	barrier.subresourceRange = image->getRawImageView(imageViewName).getImageSubresourceRange();
	barrier.srcAccessMask = srcAccessFlags;
	barrier.dstAccessMask = dstAccessFlags;
}

ImageMemoryBarrier::ImageMemoryBarrier(Image* image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags)
{
	m_imageMemoryBarriers.push_back({});
	auto& barrier = m_imageMemoryBarriers.back();
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->getImage();
	barrier.subresourceRange = image->getRawImageView().getImageSubresourceRange();
	barrier.srcAccessMask = srcAccessFlags;
	barrier.dstAccessMask = dstAccessFlags;
}



ImageMemoryBarrier::~ImageMemoryBarrier()
{
}

const std::vector<VkImageMemoryBarrier>& ImageMemoryBarrier::getImageMemoryBarriers()
{
	return m_imageMemoryBarriers;
}