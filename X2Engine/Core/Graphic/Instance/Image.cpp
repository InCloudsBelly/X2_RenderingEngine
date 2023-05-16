#include "Image.h"
#include <fstream>
#include <glm/glm.hpp>
#include <stdexcept>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>

#define VMA_IMPLEMENTATION 
#include <vma/vk_mem_alloc.h>

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Core/Graphic/Command/Semaphore.h"

#include "Utils/Bitmap.h"
#include "Utils/SphericalHarmonicsUtils.h"
#include "Utils/CubemapUtils.h"

Image* Image::Create2DImage(VkExtent2D extent, VkFormat format, VkImageUsageFlags imageUsage, VmaMemoryUsage memoryUsageFlags, VkImageAspectFlags aspect, VkImageTiling imageTiling, VkSampleCountFlagBits sampleCountFlagBits, uint32_t mipmapLevel)
{
	auto newImage = new Image();

	newImage->m_extent2D = extent;
	newImage->m_extent2Ds.push_back(extent);
	newImage->m_isNative = false;
	newImage->m_layerCount = 1;
	newImage->m_mipmapLevelCount = mipmapLevel;
	newImage->m_imageInfo.format = format;
	newImage->m_imageInfo.imageTiling = imageTiling;
	newImage->m_imageInfo.sampleCountFlagBits = sampleCountFlagBits;
	newImage->m_imageInfo.imageUsageFlags = imageUsage;
	newImage->m_imageInfo.memoryUsageFlags = memoryUsageFlags;
	newImage->m_imageInfo.imageCreateFlags = 0;
	newImage->m_imageInfo.imageViewInfos = {
		{
			"DefaultImageView",
			{
				VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				aspect,
				0,
				1,
				0,
				mipmapLevel
			}
		}
	};

	newImage->CreateVulkanInstance();

	return newImage;
}

Image* Image::CreateNative2DImage(VkImage vkImage, VkImageView vkImageView, VkExtent2D extent, VkFormat format, VkImageUsageFlags imageUsage, VkImageAspectFlags aspect)
{
	auto newImage = new Image();

	newImage->m_extent2D = extent;
	newImage->m_extent2Ds.push_back(extent);
	newImage->m_isNative = true;
	newImage->m_layerCount = 1;
	newImage->m_mipmapLevelCount = 1;
	newImage->m_imageInfo.format = format;
	newImage->m_imageInfo.imageTiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
	newImage->m_imageInfo.imageUsageFlags = imageUsage;
	newImage->m_imageInfo.memoryUsageFlags = VMA_MEMORY_USAGE_GPU_ONLY;
	newImage->m_imageInfo.imageCreateFlags = 0;
	newImage->m_imageInfo.imageViewInfos = {
		{
			"DefaultImageView",
			{
				VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				aspect,
				0,
				1,
				0,
				1
			}
		}
	};

	newImage->m_image = vkImage;

	ImageView imageView = {};

	imageView.vkImageSubresourceRange.aspectMask = aspect;
	imageView.vkImageSubresourceRange.baseMipLevel = 0;
	imageView.vkImageSubresourceRange.levelCount = 1;
	imageView.vkImageSubresourceRange.baseArrayLayer = 0;
	imageView.vkImageSubresourceRange.layerCount = 1;

	imageView.vkExtent2Ds = &newImage->m_extent2Ds;

	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = vkImage;
	imageViewCreateInfo.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.subresourceRange = imageView.vkImageSubresourceRange;

	auto status = vkCreateImageView(Instance::getDevice(), &imageViewCreateInfo, nullptr, &imageView.vkImageView);
	if (status != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view.");

	newImage->m_imageViews.emplace("DefaultImageView", imageView);

	return newImage;
}

Image* Image::Create2DImageArray(VkExtent2D extent, VkFormat format, VkImageUsageFlags imageUsage, VmaMemoryUsage memoryUsageFlags, VkImageAspectFlags aspect, uint32_t arraySize, VkImageTiling imageTiling, VkSampleCountFlagBits sampleCountFlagBits)
{
	auto newImage = new Image();

	newImage->m_extent2D = extent;
	newImage->m_extent2Ds.push_back(extent);
	newImage->m_isNative = false;
	newImage->m_layerCount = arraySize;
	newImage->m_mipmapLevelCount = 1;
	newImage->m_imageInfo.format = format;
	newImage->m_imageInfo.imageTiling = imageTiling;
	newImage->m_imageInfo.sampleCountFlagBits = sampleCountFlagBits;
	newImage->m_imageInfo.imageUsageFlags = imageUsage;
	newImage->m_imageInfo.memoryUsageFlags = memoryUsageFlags;
	newImage->m_imageInfo.imageCreateFlags = 0;
	newImage->m_imageInfo.imageViewInfos = {
		{
			"DefaultImageView",
			{
				VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				aspect,
				0,
				arraySize,
				0,
				1
			}
		}
	};

	newImage->CreateVulkanInstance();

	return newImage;
}

Image* Image::CreateCubeImage(VkExtent2D extent, VkFormat format, VkImageUsageFlags imageUsage, VmaMemoryUsage memoryUsageFlags, VkImageAspectFlags aspect, VkImageTiling imageTiling, uint32_t mipmapLevelCount, VkSampleCountFlagBits sampleCountFlagBits)
{
	auto newImage = new Image();

	newImage->m_extent2D = extent;
	newImage->m_extent2Ds.emplace_back(extent);
	if (mipmapLevelCount != 1)
	{
		uint32_t maxMipmapLevelCount = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
		mipmapLevelCount = std::min(maxMipmapLevelCount, mipmapLevelCount);
		newImage->m_extent2Ds.resize(mipmapLevelCount);
		for (int levelIndex = 1; levelIndex < mipmapLevelCount; levelIndex++)
		{
			newImage->m_extent2Ds[levelIndex].width = newImage->m_extent2Ds[levelIndex - 1].width / 2 < 1 ? 1 : newImage->m_extent2Ds[levelIndex - 1].width / 2;
			newImage->m_extent2Ds[levelIndex].height = newImage->m_extent2Ds[levelIndex - 1].height / 2 < 1 ? 1 : newImage->m_extent2Ds[levelIndex - 1].height / 2;
		}
	}
	else
	{
		newImage->m_extent2Ds.resize(1);
	}
	newImage->m_isNative = false;
	newImage->m_layerCount = 6;
	newImage->m_mipmapLevelCount = mipmapLevelCount;
	newImage->m_imageInfo.format = format;
	newImage->m_imageInfo.imageTiling = imageTiling;
	newImage->m_imageInfo.sampleCountFlagBits = sampleCountFlagBits;
	newImage->m_imageInfo.imageUsageFlags = imageUsage;
	newImage->m_imageInfo.memoryUsageFlags = memoryUsageFlags;
	newImage->m_imageInfo.imageCreateFlags = VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	newImage->m_imageInfo.imageViewInfos = {
		{
			"DefaultImageView",
			{
				VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE,
				aspect,
				0,
				6,
				0,
				mipmapLevelCount
			}
		}
	};

	newImage->CreateVulkanInstance();

	return newImage;
}

Image::Image()
	: m_isNative(false)
	, m_imageInfo()
	, m_image(VK_NULL_HANDLE)
	, m_extent2D()
	, m_imageViews()
	, m_layerCount()
{
}

Image::~Image()
{
	for (auto& imageViewPair : m_imageViews)
	{
		vkDestroyImageView(Instance::getDevice(), imageViewPair.second.vkImageView, nullptr);

	}

	vmaDestroyImage(Instance::getVmaAllocator(), m_image, m_allocation);
}

void Image::destroy()
{
	
}

void Image::AddImageView(std::string name, VkImageViewType imageViewType, VkImageAspectFlags imageAspectFlags, uint32_t baseArrayLayer, uint32_t layerCount, uint32_t baseMipmapLevel, uint32_t mipmapLevelCount)
{
	ImageViewInfo imageViewInfo{};
	imageViewInfo.imageViewType = imageViewType;
	imageViewInfo.imageAspectFlags = imageAspectFlags;
	imageViewInfo.baseLayer = baseArrayLayer;
	imageViewInfo.layerCount = layerCount;
	imageViewInfo.baseMipmapLevel = baseMipmapLevel;
	imageViewInfo.mipmapLevelCount = mipmapLevelCount;

	m_imageInfo.imageViewInfos.emplace(name, imageViewInfo);

	ImageView imageView = {};

	imageView.vkImageSubresourceRange.aspectMask = imageAspectFlags;
	imageView.vkImageSubresourceRange.baseMipLevel = baseMipmapLevel;
	imageView.vkImageSubresourceRange.levelCount = mipmapLevelCount;
	imageView.vkImageSubresourceRange.baseArrayLayer = baseArrayLayer;
	imageView.vkImageSubresourceRange.layerCount = layerCount;

	imageView.vkExtent2Ds = &m_extent2Ds;

	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = m_image;
	imageViewCreateInfo.viewType = imageViewType;
	imageViewCreateInfo.format = m_imageInfo.format;
	imageViewCreateInfo.subresourceRange = imageView.vkImageSubresourceRange;

	auto status = vkCreateImageView(Instance::getDevice(), &imageViewCreateInfo, nullptr, &imageView.vkImageView);
	if (status != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view.");

	m_imageViews.emplace(name, imageView);
}

void Image::RemoveImageView(std::string name)
{
	vkDestroyImageView(Instance::getDevice(), m_imageViews[name].vkImageView, nullptr);
	m_imageViews.erase(name);
	m_imageInfo.imageViewInfos.erase(name);
}

Image::ImageView& Image::getRawImageView(std::string imageViewName)
{
	return m_imageViews[imageViewName];
}

VkImageView& Image::getImageView(std::string imageViewName)
{
	return m_imageViews[imageViewName].getImageView();
}

Image::ImageInfo& Image::getImageInfo()
{
	return m_imageInfo;
}

VkImage& Image::getImage()
{
	return m_image;
}

uint32_t Image::getMipMapLevelCount()
{
	return m_mipmapLevelCount;
}

uint32_t Image::getLayerCount()
{
	return m_layerCount;
}

VkFormat Image::getFormat()
{
	return m_imageInfo.format;
}

VkImageUsageFlags Image::getImageUsageFlags()
{
	return m_imageInfo.imageUsageFlags;
}

VmaMemoryUsage Image::getMemoryUsageFlags()
{
	return m_imageInfo.memoryUsageFlags;
}

VkImageTiling Image::getImageTiling()
{
	return m_imageInfo.imageTiling;
}

VkSampleCountFlagBits Image::getSampleCountFlagBits()
{
	return m_imageInfo.sampleCountFlagBits;
}

VmaAllocation& Image::getVmaAllocation()
{
	return m_allocation;
}

VkExtent3D Image::getExtent3D()
{
	return VkExtent3D{ m_extent2D.width, m_extent2D.height, 1 };
}

VkExtent2D Image::getExtent2D()
{
	return m_extent2D;
}

VkExtent2D Image::getExtent2D(uint32_t const mipmapLevel) const
{
	return m_extent2Ds[mipmapLevel];
}

void Image::onLoad(CommandBuffer* transferCommandBuffer)
{
	std::string path = getPath();
	size_t pos = path.find_last_of(".");
	std::string filetype;

	if (pos != std::string::npos) {
		filetype = path.substr(pos + 1);
	}

	if (filetype == "hdr")
		loadCubeMap(path, transferCommandBuffer);
	else if (filetype == "png" || filetype == "jpg" || filetype == "lut" || filetype == "tga")
		load2DImage(path, transferCommandBuffer);
	else
		throw(std::runtime_error("unknown Image File Type :" + filetype + ", filePath : " + path));


}

void Image::CreateVulkanInstance()
{
	///Create vk image
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VkImageType::VK_IMAGE_TYPE_2D;
		imageInfo.extent = { m_extent2D.width, m_extent2D.height, 1 };
		imageInfo.mipLevels = m_mipmapLevelCount;
		imageInfo.arrayLayers = m_layerCount;
		imageInfo.format = m_imageInfo.format;
		imageInfo.tiling = m_imageInfo.imageTiling;
		imageInfo.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;

		if (m_imageInfo.imageUsageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
			imageInfo.usage = m_imageInfo.imageUsageFlags;
		else
			imageInfo.usage = m_imageInfo.imageUsageFlags | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		imageInfo.samples = m_imageInfo.sampleCountFlagBits;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = m_imageInfo.imageCreateFlags;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = m_imageInfo.memoryUsageFlags;

		auto status = vmaCreateImage(Instance::getVmaAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr);
		if (status != VK_SUCCESS)
			throw std::runtime_error("Failed to create image.");
	}

	///Create vk image view
	{
		if (m_imageInfo.imageViewInfos.find("DefaultImageView") == m_imageInfo.imageViewInfos.end())
		{
			throw std::runtime_error("Failed to find DefaultImageView.");
		}

		for (auto& imageInfoPair : m_imageInfo.imageViewInfos)
		{
			ImageView imageView = {};

			imageView.vkImageSubresourceRange.aspectMask = imageInfoPair.second.imageAspectFlags;
			imageView.vkImageSubresourceRange.baseMipLevel = imageInfoPair.second.baseMipmapLevel;
			imageView.vkImageSubresourceRange.levelCount = imageInfoPair.second.mipmapLevelCount;
			imageView.vkImageSubresourceRange.baseArrayLayer = imageInfoPair.second.baseLayer;
			imageView.vkImageSubresourceRange.layerCount = imageInfoPair.second.layerCount;
			imageView.vkExtent2Ds = &m_extent2Ds;

			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = m_image;
			imageViewCreateInfo.viewType = imageInfoPair.second.imageViewType;
			imageViewCreateInfo.format = m_imageInfo.format;
			imageViewCreateInfo.subresourceRange = imageView.vkImageSubresourceRange;

			auto status = vkCreateImageView(Instance::getDevice(), &imageViewCreateInfo, nullptr, &imageView.vkImageView);
			if (status != VK_SUCCESS)
				throw std::runtime_error("Failed to create image view.");

			m_imageViews.emplace(imageInfoPair.first, imageView);
		}
	}
}

VkImageView& Image::ImageView::getImageView()
{
	return vkImageView;
}

const VkImageSubresourceRange& Image::ImageView::getImageSubresourceRange()
{
	return vkImageSubresourceRange;
}

uint32_t Image::ImageView::getBaseMipmapLevel()
{
	return vkImageSubresourceRange.baseMipLevel;
}

uint32_t Image::ImageView::getMipmapLevelCount()
{
	return vkImageSubresourceRange.levelCount;
}

uint32_t Image::ImageView::getBaseLayer()
{
	return vkImageSubresourceRange.baseArrayLayer;
}

uint32_t Image::ImageView::getLayerCount()
{
	return vkImageSubresourceRange.layerCount;
}

VkImageAspectFlags Image::ImageView::getImageAspectFlags()
{
	return vkImageSubresourceRange.aspectMask;
}

VkExtent2D Image::ImageView::getExtent2D(uint32_t levelIndex)
{
	return (*vkExtent2Ds)[levelIndex];
}

VkExtent3D Image::ImageView::getExtent3D(uint32_t levelIndex)
{
	return { (*vkExtent2Ds)[levelIndex].width, (*vkExtent2Ds)[levelIndex].height, 1 };
}













ImageSampler::ImageSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxAnisotropy, VkBorderColor borderColor, uint32_t mipmapLevel)
	: m_magFilter(magFilter)
	, m_minFilter(minFilter)
	, m_mipmapMode(mipmapMode)
	, m_addressModeU(addressModeU)
	, m_addressModeV(addressModeV)
	, m_addressModeW(addressModeW)
	, m_maxAnisotropy(std::min(maxAnisotropy, 1.0f))
	, m_anisotropyEnable(maxAnisotropy < 1.0f ? VK_FALSE : VK_TRUE)
	, m_borderColor(borderColor)
	, m_mipmapLevel(mipmapLevel)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = m_magFilter;
	samplerInfo.minFilter = m_minFilter;
	samplerInfo.addressModeU = m_addressModeU;
	samplerInfo.addressModeV = m_addressModeV;
	samplerInfo.addressModeW = m_addressModeW;
	samplerInfo.anisotropyEnable = m_anisotropyEnable;
	samplerInfo.maxAnisotropy = m_maxAnisotropy;
	samplerInfo.borderColor = m_borderColor;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = m_mipmapMode;
	samplerInfo.minLod = 0.0f;
	samplerInfo.mipLodBias = 0.0f;

	if (mipmapLevel > 1)
		samplerInfo.maxLod = (float)mipmapLevel;
	else
		samplerInfo.maxLod = 0.0f;


	auto status = vkCreateSampler(Instance::getDevice(), &samplerInfo, nullptr, &m_sampler);
	if (status != VK_SUCCESS)
		throw std::runtime_error("Failed to create sampler.");

}
ImageSampler::ImageSampler(VkFilter filter)
	: m_magFilter(filter)
	, m_minFilter(filter)
	, m_mipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
	, m_addressModeU(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
	, m_addressModeV(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
	, m_addressModeW(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
	, m_maxAnisotropy(0)
	, m_anisotropyEnable(VK_FALSE)
	, m_borderColor()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = m_magFilter;
	samplerInfo.minFilter = m_minFilter;
	samplerInfo.addressModeU = m_addressModeU;
	samplerInfo.addressModeV = m_addressModeV;
	samplerInfo.addressModeW = m_addressModeW;
	samplerInfo.anisotropyEnable = m_anisotropyEnable;
	samplerInfo.maxAnisotropy = m_maxAnisotropy;
	samplerInfo.borderColor = m_borderColor;
	samplerInfo.unnormalizedCoordinates = VK_TRUE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = m_mipmapMode;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.mipLodBias = 0.0f;

	auto status = vkCreateSampler(Instance::getDevice(), &samplerInfo, nullptr, &m_sampler);
	if (status != VK_SUCCESS)
		throw std::runtime_error("Failed to create sampler.");

}

ImageSampler::ImageSampler(VkFilter filter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode, float maxAnisotropy, VkBorderColor borderColor)
	: ImageSampler(filter, filter, mipmapMode, addressMode, addressMode, addressMode, maxAnisotropy, borderColor)
{
}

ImageSampler::~ImageSampler() {
	vkDestroySampler(Instance::getDevice(), m_sampler, nullptr);
}



VkSampler& ImageSampler::getSampler()
{
	return m_sampler;
}

void Image::load2DImage(std::string path, CommandBuffer* transferCommandBuffer)
{
	struct image_data
	{
		VkDeviceSize imageSize = 0;
		stbi_uc* pixels;
		int texWidth, texHeight, texChannels;
	};

	image_data imageData;

	imageData.pixels = stbi_load(
		path.c_str(),
		&imageData.texWidth,
		&imageData.texHeight,
		&imageData.texChannels,
		STBI_rgb_alpha
	);


	VkExtent2D extent;
	extent.width = imageData.texWidth;
	extent.height = imageData.texHeight;

	if (imageData.pixels == nullptr)
	{
		throw(std::runtime_error(path + " pixels is null!"));
	}

	imageData.imageSize = imageData.texWidth * imageData.texHeight * 4;




	Buffer* stagingBuffer = new Buffer(imageData.imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	stagingBuffer->WriteData(imageData.pixels, imageData.imageSize);
	stbi_image_free(imageData.pixels);


	uint32_t mipLevel = std::floor(std::log2(std::max(imageData.texWidth, imageData.texHeight))) + 1; 
	m_extent2Ds.resize(mipLevel);
	
	// create 2D Image
	{
		m_extent2D = extent;
		//mip extents
		{
			m_extent2Ds[0] = extent;
			for (int levelIndex = 1; levelIndex < mipLevel; levelIndex++)
			{
				m_extent2Ds[levelIndex].width = m_extent2Ds[levelIndex - 1].width / 2 < 1 ? 1 : m_extent2Ds[levelIndex - 1].width / 2;
				m_extent2Ds[levelIndex].height = m_extent2Ds[levelIndex - 1].height / 2 < 1 ? 1 : m_extent2Ds[levelIndex - 1].height / 2;
			}
		}

		m_isNative = false;
		m_layerCount = 1;
		m_mipmapLevelCount = mipLevel;
		m_imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;          //2D -> VK_FORMAT_R8G8B8A8_SRGB
		m_imageInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
		m_imageInfo.sampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT;
		m_imageInfo.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
		m_imageInfo.memoryUsageFlags = VMA_MEMORY_USAGE_GPU_ONLY;
		m_imageInfo.imageCreateFlags = 0;
		m_imageInfo.imageViewInfos = {
			{
				"DefaultImageView",
				{
					VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
					VK_IMAGE_ASPECT_COLOR_BIT,
					0,
					1,
					0,
					mipLevel
				}
			}
		};

		CreateVulkanInstance();
	}


	// MipMap Level

	for (int levelIndex = 0; levelIndex < m_mipmapLevelCount; levelIndex++)
	{
		AddImageView(
			"Transfer_Level" + std::to_string(levelIndex),
			m_imageInfo.imageViewInfos["DefaultImageView"].imageViewType,
			m_imageViews["DefaultImageView"].getImageAspectFlags(),
			0,
			m_layerCount,
			levelIndex,
			1
		);
	}

	//Copy buffer to image
	{
		transferCommandBuffer->reset();
		transferCommandBuffer->beginRecord(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		{
			ImageMemoryBarrier barrier = ImageMemoryBarrier(
				this,
				"DefaultImageView",
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT
			);
			transferCommandBuffer->addPipelineImageBarrier(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				{ &barrier }
			);
		}

		transferCommandBuffer->copyBufferToImage(stagingBuffer, this, "DefaultImageView", VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);


		for (int generateLevelIndex = 1; generateLevelIndex < m_mipmapLevelCount; generateLevelIndex++)
		{
			{
				ImageMemoryBarrier barrier = ImageMemoryBarrier(
					this,
					"Transfer_Level" + std::to_string(generateLevelIndex - 1),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT
				);
				transferCommandBuffer->addPipelineImageBarrier(
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					{ &barrier }
				);
			}
			transferCommandBuffer->blit(
				this, "Transfer_Level" + std::to_string(generateLevelIndex - 1), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				this, "Transfer_Level" + std::to_string(generateLevelIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_FILTER_LINEAR
				//_imageImfo.imageViewInfos["Transfer_Level" + std::to_string(generateLevelIndex)].imageViewType == VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE ? VK_FILTER_CUBIC_IMG : VK_FILTER_LINEAR
			);

			{
				ImageMemoryBarrier barrier = ImageMemoryBarrier(
					this,
					"Transfer_Level" + std::to_string(generateLevelIndex - 1),
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_ACCESS_SHADER_READ_BIT
				);
				transferCommandBuffer->addPipelineImageBarrier(
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					{ &barrier }
				);
			}
		}
		{
			ImageMemoryBarrier barrier = ImageMemoryBarrier(
				this,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT
			);
			transferCommandBuffer->addPipelineImageBarrier(
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				{ &barrier }
			);
		}

		transferCommandBuffer->endRecord();
		transferCommandBuffer->submit();
		transferCommandBuffer->waitForFinish();
	}

	delete stagingBuffer;
}

void Image::loadCubeMap(std::string path, CommandBuffer* transferCommandBuffer)
{
	struct image_data
	{
		VkDeviceSize imageSize = 0;
		stbi_uc* pixels;
		int texWidth, texHeight, texChannels;
	};

	image_data imageData;

	const float* img = stbi_loadf(
		path.c_str(),
		&imageData.texWidth,
		&imageData.texHeight,
		&imageData.texChannels,
		// Desired channels
		// (we'll later convert it to 4)
		3
	);
	if (img == nullptr)
	{
		throw std::runtime_error("Failed to load texture image: " + path);
	}


	std::vector<float> img32(imageData.texWidth * imageData.texHeight * 4);
	cubemapUtils::float24to32(imageData.texWidth, imageData.texHeight, img, img32.data());
	stbi_image_free((void*)img);

	Bitmap in(imageData.texWidth, imageData.texHeight, 4, eBitmapFormat_Float, img32.data());
	Bitmap out = cubemapUtils::convertEquirectangularMapToVerticalCross(in);
	stbi_write_hdr("screenshot.hdr", out.w_, out.h_, out.comp_, (const float*)out.data_.data());

	Bitmap cubemap = cubemapUtils::convertVerticalCrossToCubeMapFaces(out);



	uint8_t* allData = cubemap.data_.data();
	uint32_t imageSize = cubemap.w_ * cubemap.h_ * 4 * Bitmap::getBytesPerComponent(cubemap.fmt_);
	uint32_t allImageSize = imageSize * 6;


	Buffer* stagingBuffer = new Buffer(allImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	
	stagingBuffer->WriteData(allData, static_cast<uint32_t>(allImageSize));;


	VkExtent2D extent{ (uint32_t)cubemap.w_,(uint32_t)cubemap.h_ };

	// create CubeMap Image
	{

		m_extent2D = extent;
		m_extent2Ds.emplace_back(extent);

		m_isNative = false;
		m_layerCount = 6;
		m_mipmapLevelCount = 1;
		m_imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;          //CubeMap-> VK_FORMAT_R32G32B32A32_SFLOAT
		m_imageInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
		m_imageInfo.sampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT;
		m_imageInfo.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
		m_imageInfo.memoryUsageFlags = VMA_MEMORY_USAGE_GPU_ONLY;
		m_imageInfo.imageCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		m_imageInfo.imageViewInfos = {
			{
				"DefaultImageView",
				{
					VK_IMAGE_VIEW_TYPE_CUBE,
					VK_IMAGE_ASPECT_COLOR_BIT,
					0,
					6,
					0,
					1
				}
			}
		};

		CreateVulkanInstance();
	}

	
	//Copy buffer to image
	{
		transferCommandBuffer->reset();
		transferCommandBuffer->beginRecord(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		{
			ImageMemoryBarrier barrier = ImageMemoryBarrier(
				this,
				"DefaultImageView",
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT
			);
			transferCommandBuffer->addPipelineImageBarrier(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				{ &barrier }
			);
		}

		transferCommandBuffer->copyBufferToImage(stagingBuffer, this, "DefaultImageView", VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);


		{
			ImageMemoryBarrier barrier = ImageMemoryBarrier(
				this,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT
			);
			transferCommandBuffer->addPipelineImageBarrier(
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				{ &barrier }
			);
		}

		transferCommandBuffer->endRecord();
		transferCommandBuffer->submit();
		transferCommandBuffer->waitForFinish();
	}

	delete stagingBuffer;



	//tex.sampler = new ImageSampler(
	//	VK_FILTER_LINEAR,
	//	VK_FILTER_LINEAR,
	//	VK_SAMPLER_MIPMAP_MODE_LINEAR,
	//	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	//	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	//	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	//	16,
	//	VK_BORDER_COLOR_INT_OPAQUE_BLACK,
	//	1
	//);
}


////PRT
//std::string path = pathToTexture + "/coefficients.txt";
//std::ifstream ifs(path);
//if (ifs)
//{
//	int i = 0;
//	float r, g, b;
//	while (ifs >> r >> g >> b)
//	{
//		getRenderResource()->m_coefficient[i] = glm::vec3(r, g, b);
//		i++;
//	}
//}
//else
//{
//	std::vector<glm::vec3> coefs = SphericalHarmonicsUtils::computeSkyboxSH(cubemap);
//
//	int offset = 0;
//	std::ofstream coeffile(pathToTexture + "/coefficients.txt");
//	for (const glm::vec3& c : coefs)
//	{
//		coeffile << c.r << "\t" << c.g << "\t" << c.b << std::endl;
//		std::cout << "coef[" << offset << "] = " << coefs[offset].x << ", " << coefs[offset].y << ", " << coefs[offset].z << std::endl;
//		getRenderResource()->m_coefficient[offset] = coefs[offset];
//		offset++;
//	}
//}
