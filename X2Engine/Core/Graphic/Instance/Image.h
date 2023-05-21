#pragma once
#include <vulkan/vulkan_core.h>
#include <vma/vk_mem_alloc.h>

#include <vector>
#include <string>
#include <map>


#include "Core/IO/Asset/AssetBase.h"


class Image;
class ImageSampler;


class Image : public AssetBase
{
	friend class Swapchain;
public:
	struct ImageViewInfo
	{
		VkImageViewType imageViewType;
		VkImageAspectFlags imageAspectFlags;
		uint32_t baseLayer;
		uint32_t layerCount;
		uint32_t baseMipmapLevel;
		uint32_t mipmapLevelCount;
	};
	struct ImageInfo
	{
		VkFormat format;
		//FREE_IMAGE_TYPE targetType;
		VkSampleCountFlagBits sampleCountFlagBits;
		VkImageTiling imageTiling;
		VkImageUsageFlags imageUsageFlags;
		VmaMemoryUsage memoryUsageFlags;
		VkImageCreateFlags imageCreateFlags;
		bool autoGenerateMipmap;
		bool topDown;
		std::map<std::string, ImageViewInfo> imageViewInfos;

	};

	struct ImageView
	{
		friend class Image;
		friend class Swapchain;
	private:
		VkImageView vkImageView;
		VkImageSubresourceRange vkImageSubresourceRange;
		std::vector<VkExtent2D>* vkExtent2Ds;
	public:
		VkImageView& getImageView();
		const VkImageSubresourceRange& getImageSubresourceRange();
		uint32_t getBaseMipmapLevel();
		uint32_t getMipmapLevelCount();
		uint32_t getBaseLayer();
		uint32_t getLayerCount();
		VkImageAspectFlags getImageAspectFlags();
		VkExtent2D getExtent2D(uint32_t levelIndex);
		VkExtent3D getExtent3D(uint32_t levelIndex);
	};

private:
	bool m_isNative;
	ImageInfo m_imageInfo;
	VkImage m_image;
	VmaAllocation m_allocation;
	VkExtent2D m_extent2D;
	std::vector<VkExtent2D> m_extent2Ds;
	std::map<std::string, ImageView> m_imageViews;
	uint32_t m_layerCount;
	uint32_t m_mipmapLevelCount;

	void load2DImage(std::string path, CommandBuffer* transferCommandBuffer);
	void loadCubeMap(std::string path, CommandBuffer* transferCommandBuffer);

public:

	static Image* Create2DImage(
		VkExtent2D extent,
		VkFormat format,
		VkImageUsageFlags imageUsage,
		VmaMemoryUsage memoryUsage,
		VkImageAspectFlags aspect,
		VkImageTiling imageTiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
		VkSampleCountFlagBits sampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		uint32_t mimapLevel = 1
	);
	static Image* CreateNative2DImage(
		VkImage vkImage,
		VkImageView vkImageView,
		VkExtent2D extent,
		VkFormat format,
		VkImageUsageFlags imageUsage,
		VkImageAspectFlags aspect
	);
	static Image* Create2DImageArray(
		VkExtent2D extent,
		VkFormat format,
		VkImageUsageFlags imageUsage,
		VmaMemoryUsage memoryUsage,
		VkImageAspectFlags aspect,
		uint32_t arraySize,
		VkImageTiling imageTiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
		VkSampleCountFlagBits sampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT
	);
	static Image* CreateCubeImage(
		VkExtent2D extent,
		VkFormat format,
		VkImageUsageFlags imageUsage,
		VmaMemoryUsage memoryUsage,
		VkImageAspectFlags aspect,
		VkImageTiling imageTiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
		uint32_t mipmapLevelCount = 1,
		VkSampleCountFlagBits sampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT
	);

	Image();
	~Image();
	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;
	Image(Image&&) = delete;
	Image& operator=(Image&&) = delete;

	void destroy();

	void AddImageView(std::string name, VkImageViewType imageViewType, VkImageAspectFlags imageAspectFlags, uint32_t baseArrayLayer, uint32_t layerCount, uint32_t baseMipmapLevel = 0, uint32_t mipmapLevelCount = 1);
	void RemoveImageView(std::string name);

	ImageSampler* m_sampler = nullptr;

	ImageView& getRawImageView(std::string imageViewName = "DefaultImageView");
	VkImageView& getImageView(std::string imageViewName = "DefaultImageView");
	ImageInfo& getImageInfo();
	VkImage& getImage();
	VkExtent3D getExtent3D();
	VkExtent2D getExtent2D();
	VkExtent2D getExtent2D(uint32_t const mipmapLevel) const;
	uint32_t getMipMapLevelCount();
	uint32_t getLayerCount();
	VkFormat getFormat();
	ImageSampler* getSampler();
	VkImageUsageFlags getImageUsageFlags();
	VmaMemoryUsage getMemoryUsageFlags();
	VkImageTiling getImageTiling();
	VkSampleCountFlagBits getSampleCountFlagBits();
	VmaAllocation& getVmaAllocation();

private:
	void onLoad(CommandBuffer* transferCommandBuffer) override;
	void CreateVulkanInstance();
};


class ImageSampler
{
private:
	VkFilter m_magFilter;
	VkFilter m_minFilter;
	VkSamplerMipmapMode m_mipmapMode;
	VkSamplerAddressMode m_addressModeU;
	VkSamplerAddressMode m_addressModeV;
	VkSamplerAddressMode m_addressModeW;
	VkBool32 m_anisotropyEnable;
	float m_maxAnisotropy;
	VkBorderColor m_borderColor;
	uint32_t m_mipmapLevel;

	VkSampler m_sampler;

	ImageSampler(const ImageSampler&) = delete;
	ImageSampler& operator=(const ImageSampler&) = delete;
	ImageSampler(ImageSampler&&) = delete;
	ImageSampler& operator=(ImageSampler&&) = delete;

public:
	ImageSampler(
		VkFilter magFilter,
		VkFilter minFilter,
		VkSamplerMipmapMode mipmapMode,
		VkSamplerAddressMode addressModeU,
		VkSamplerAddressMode addressModeV,
		VkSamplerAddressMode addressModeW,
		float maxAnisotropy,
		VkBorderColor borderColor,
		uint32_t mipmapLevel = 1
	);
	ImageSampler(VkFilter filter);
	ImageSampler(
		VkFilter filter,
		VkSamplerMipmapMode mipmapMode,
		VkSamplerAddressMode addressMode,
		float maxAnisotropy,
		VkBorderColor borderColor,
		uint32_t mipmapLevel = 1
	);
	~ImageSampler();
	VkSampler& getSampler();
};
