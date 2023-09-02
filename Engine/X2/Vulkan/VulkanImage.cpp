#include "Precompiled.h"
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "VulkanAllocator.h"
#include "VulkanRenderer.h"
#include "VulkanContext.h"

#include "X2/Renderer/Renderer.h"

namespace X2 {

	static std::map<VkImage, WeakRef<VulkanImage2D>> s_ImageReferences;

	VulkanImage2D::VulkanImage2D(const ImageSpecification& specification)
		: m_Specification(specification)
	{
		X2_CORE_VERIFY(m_Specification.Width > 0 && m_Specification.Height > 0);
	}

	VulkanImage2D::~VulkanImage2D()
	{
		if (m_Info.Image)
		{
			const VulkanImageInfo& info = m_Info;
			Renderer::SubmitResourceFree([info, layerViews = m_PerLayerImageViews]()
				{
					const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
					vkDestroyImageView(vulkanDevice, info.ImageView, nullptr);
					vkDestroySampler(vulkanDevice, info.Sampler, nullptr);

					for (auto& view : layerViews)
					{
						if (view)
							vkDestroyImageView(vulkanDevice, view, nullptr);
					}

					VulkanAllocator allocator("VulkanImage2D");
					allocator.DestroyImage(info.Image, info.MemoryAlloc);
					s_ImageReferences.erase(info.Image);

					X2_CORE_WARN_TAG("Renderer", "VulkanImage2D::Release ImageView = {0}", (const void*)info.ImageView);
				});
			m_PerLayerImageViews.clear();
		}
	}

	void VulkanImage2D::Invalidate()
	{
		Ref<VulkanImage2D> instance = this;
		Renderer::Submit([instance]() mutable
			{
				instance->RT_Invalidate();
			});
	}

	void VulkanImage2D::Release()
	{
		if (m_Info.Image == nullptr)
			return;

		const VulkanImageInfo& info = m_Info;
		Renderer::SubmitResourceFree([info, mipViews = m_PerMipImageViews, layerViews = m_PerLayerImageViews]() mutable
			{
				const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				vkDestroyImageView(vulkanDevice, info.ImageView, nullptr);
				vkDestroySampler(vulkanDevice, info.Sampler, nullptr);

				for (auto& view : mipViews)
				{
					if (view.second)
						vkDestroyImageView(vulkanDevice, view.second, nullptr);
				}
				for (auto& view : layerViews)
				{
					if (view)
						vkDestroyImageView(vulkanDevice, view, nullptr);
				}
				VulkanAllocator allocator("VulkanImage2D");
				allocator.DestroyImage(info.Image, info.MemoryAlloc);
				s_ImageReferences.erase(info.Image);
			});
		m_Info.Image = nullptr;
		m_Info.ImageView = nullptr;
		m_Info.Sampler = nullptr;
		m_PerLayerImageViews.clear();
		m_PerMipImageViews.clear();

	}

	void VulkanImage2D::RT_Invalidate()
	{
		X2_CORE_VERIFY(m_Specification.Width > 0 && m_Specification.Height > 0);

		X2_CORE_VERIFY( !(m_Specification.Depth > 1 && m_Specification.Layers > 1));


		// Try release first if necessary
		Release();

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanAllocator allocator("Image2D");

		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: this (probably) shouldn't be implied
		if (m_Specification.Usage == ImageUsage::Attachment)
		{
			if (Utils::IsDepthFormat(m_Specification.Format))
				usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			else
				usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (m_Specification.Transfer || m_Specification.Usage == ImageUsage::Texture)
		{
			usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (m_Specification.Usage == ImageUsage::Storage)
		{
			usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if(m_Specification.Usage == ImageUsage::HostRead)
			usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

		VmaMemoryUsage memoryUsage = m_Specification.Usage == ImageUsage::HostRead ? VMA_MEMORY_USAGE_GPU_TO_CPU : VMA_MEMORY_USAGE_GPU_ONLY;

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = m_Specification.Depth == 1? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = vulkanFormat;
		imageCreateInfo.extent.width = m_Specification.Width;
		imageCreateInfo.extent.height = m_Specification.Height;
		imageCreateInfo.extent.depth = m_Specification.Depth;
		imageCreateInfo.mipLevels = m_Specification.Mips;
		imageCreateInfo.arrayLayers = m_Specification.Layers;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = m_Specification.Usage == ImageUsage::HostRead ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = usage;
		m_Info.MemoryAlloc = allocator.AllocateImage(imageCreateInfo, memoryUsage, m_Info.Image, &m_GPUAllocationSize);
		s_ImageReferences[m_Info.Image] = this;
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, m_Specification.DebugName, m_Info.Image);

		// Create a default image view
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = m_Specification.Layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : (m_Specification.Depth > 1? VK_IMAGE_VIEW_TYPE_3D: VK_IMAGE_VIEW_TYPE_2D);
		imageViewCreateInfo.format = vulkanFormat;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = m_Specification.Mips;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = m_Specification.Layers;
		imageViewCreateInfo.image = m_Info.Image;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_Info.ImageView));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::format("{} default image view", m_Specification.DebugName), m_Info.ImageView);

		// TODO: Renderer should contain some kind of sampler cache
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		if (Utils::IsIntegerBased(m_Specification.Format))
		{
			samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}
		else
		{
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}

		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 100.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_Info.Sampler));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SAMPLER, fmt::format("{} default sampler", m_Specification.DebugName), m_Info.Sampler);

		if (m_Specification.Usage == ImageUsage::Storage)
		{
			// Transition image to GENERAL layout
			VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true);

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = m_Specification.Mips;
			subresourceRange.layerCount = m_Specification.Layers;

			Utils::InsertImageMemoryBarrier(commandBuffer, m_Info.Image,
				0, 0,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				subresourceRange);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
		}
		else if (m_Specification.Usage == ImageUsage::HostRead)
		{
			// Transition image to TRANSFER_DST layout
			VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true);

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = m_Specification.Mips;
			subresourceRange.layerCount = m_Specification.Layers;

			Utils::InsertImageMemoryBarrier(commandBuffer, m_Info.Image,
				0, 0,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				subresourceRange);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
		}
		else if (m_Specification.Usage == ImageUsage::Texture)
		{
			// Transition image to TRANSFER_DST layout
			VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true);

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = m_Specification.Mips;
			subresourceRange.layerCount = m_Specification.Layers;

			Utils::InsertImageMemoryBarrier(commandBuffer, m_Info.Image,
				0, 0,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				subresourceRange);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
		}

		UpdateDescriptor();
	}

	void VulkanImage2D::CreatePerLayerImageViews()
	{
		Ref<VulkanImage2D> instance = this;
		Renderer::Submit([instance]() mutable
			{
				instance->RT_CreatePerLayerImageViews();
			});
	}

	void VulkanImage2D::RT_CreatePerLayerImageViews()
	{
		X2_CORE_ASSERT(m_Specification.Layers > 1);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		const VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

		m_PerLayerImageViews.resize(m_Specification.Layers);
		for (uint32_t layer = 0; layer < m_Specification.Layers; layer++)
		{
			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = vulkanFormat;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.subresourceRange = {};
			imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = m_Specification.Mips;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = layer;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.image = m_Info.Image;
			VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_PerLayerImageViews[layer]));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::format("{} image view layer: {}", m_Specification.DebugName, layer), m_PerLayerImageViews[layer]);
		}
	}

	VkImageView VulkanImage2D::GetMipImageView(uint32_t mip)
	{
		if (m_PerMipImageViews.find(mip) == m_PerMipImageViews.end())
		{
			Ref<VulkanImage2D> instance = this;
			Renderer::Submit([instance, mip]() mutable
				{
					instance->RT_GetMipImageView(mip);
				});
			return nullptr;
		}

		return m_PerMipImageViews.at(mip);
	}

	VkImageView VulkanImage2D::RT_GetMipImageView(const uint32_t mip)
	{
		if (m_PerMipImageViews.find(mip) == m_PerMipImageViews.end())
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

			VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = vulkanFormat;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.subresourceRange = {};
			imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
			imageViewCreateInfo.subresourceRange.baseMipLevel = mip;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.image = m_Info.Image;

			VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_PerMipImageViews[mip]));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::format("{} image view mip: {}", m_Specification.DebugName, mip), m_PerMipImageViews[mip]);
		}
		return m_PerMipImageViews.at(mip);
	}

	void VulkanImage2D::RT_CreatePerSpecificLayerImageViews(const std::vector<uint32_t>& layerIndices)
	{
		X2_CORE_ASSERT(m_Specification.Layers > 1);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		const VkFormat vulkanFormat = Utils::VulkanImageFormat(m_Specification.Format);

		//X2_CORE_ASSERT(m_PerLayerImageViews.size() == m_Specification.Layers);
		if (m_PerLayerImageViews.empty())
			m_PerLayerImageViews.resize(m_Specification.Layers);

		for (uint32_t layer : layerIndices)
		{
			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = vulkanFormat;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.subresourceRange = {};
			imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = m_Specification.Mips;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = layer;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.image = m_Info.Image;
			VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_PerLayerImageViews[layer]));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::format("{} image view layer: {}", m_Specification.DebugName, layer), m_PerLayerImageViews[layer]);
		}

	}

	void VulkanImage2D::UpdateDescriptor()
	{
		if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8 || m_Specification.Format == ImageFormat::DEPTH32F || m_Specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
			m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		else if (m_Specification.Usage == ImageUsage::Storage)
			m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		else
			m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (m_Specification.Usage == ImageUsage::Storage)
			m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		else if (m_Specification.Usage == ImageUsage::HostRead)
			m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		m_DescriptorImageInfo.imageView = m_Info.ImageView;
		m_DescriptorImageInfo.sampler = m_Info.Sampler;

		//X2_CORE_WARN_TAG("Renderer", "VulkanImage2D::UpdateDescriptor to ImageView = {0}", (const void*)m_Info.ImageView);
	}

	const std::map<VkImage, WeakRef<VulkanImage2D>>& VulkanImage2D::GetImageRefs()
	{
		return s_ImageReferences;
	}

	void VulkanImage2D::CopyToHostBuffer(Buffer& buffer)
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();
		VulkanAllocator allocator("Image2D");

		uint64_t bufferSize = m_Specification.Width * m_Specification.Height * Utils::GetImageFormatBPP(m_Specification.Format);

		// Create staging buffer
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = bufferSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBuffer);

		uint32_t mipCount = 1;
		uint32_t mipWidth = m_Specification.Width, mipHeight = m_Specification.Height;

		VkCommandBuffer copyCmd = device->GetCommandBuffer(true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipCount;
		subresourceRange.layerCount = 1;

		Utils::InsertImageMemoryBarrier(copyCmd, m_Info.Image,
			VK_ACCESS_TRANSFER_READ_BIT, 0,
			m_DescriptorImageInfo.imageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			subresourceRange);

		uint64_t mipDataOffset = 0;
		for (uint32_t mip = 0; mip < mipCount; mip++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = mip;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = mipWidth;
			bufferCopyRegion.imageExtent.height = mipHeight;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = mipDataOffset;

			vkCmdCopyImageToBuffer(
				copyCmd,
				m_Info.Image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				stagingBuffer,
				1,
				&bufferCopyRegion);

			uint64_t mipDataSize = mipWidth * mipHeight * sizeof(float) * 4 * 6;
			mipDataOffset += mipDataSize;
			mipWidth /= 2;
			mipHeight /= 2;
		}

		Utils::InsertImageMemoryBarrier(copyCmd, m_Info.Image,
			VK_ACCESS_TRANSFER_READ_BIT, 0,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_DescriptorImageInfo.imageLayout,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			subresourceRange);

		device->FlushCommandBuffer(copyCmd);

		// Copy data from staging buffer
		uint8_t* srcData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
		buffer.Allocate(bufferSize);
		memcpy(buffer.Data, srcData, bufferSize);
		allocator.UnmapMemory(stagingBufferAllocation);

		allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);
	}

}

uint32_t X2::Utils::GetImageFormatBPP(ImageFormat format)
{
	switch (format)
	{
	case ImageFormat::RED8UN:  return 1;
	case ImageFormat::RED8UI:  return 1;
	case ImageFormat::RED16UI: return 2;
	case ImageFormat::RED32UI: return 4;
	case ImageFormat::RED32F:  return 4;
	case ImageFormat::RGB:
	case ImageFormat::SRGB:    return 3;
	case ImageFormat::RGBA:    return 4;
	case ImageFormat::RGBA16F: return 2 * 4;
	case ImageFormat::RGBA32F: return 4 * 4;
	case ImageFormat::B10R11G11UF: return 4;
	}
	X2_CORE_ASSERT(false);
	return 0;
}

bool X2::Utils::IsIntegerBased(const ImageFormat format)
{
	switch (format)
	{
	case ImageFormat::RED16UI:
	case ImageFormat::RED32UI:
	case ImageFormat::RED8UI:
	case ImageFormat::DEPTH32FSTENCIL8UINT:
		return true;
	case ImageFormat::DEPTH32F:
	case ImageFormat::RED8UN:
	case ImageFormat::RGBA32F:
	case ImageFormat::B10R11G11UF:
	case ImageFormat::RG16F:
	case ImageFormat::RG32F:
	case ImageFormat::RED32F:
	case ImageFormat::RG8:
	case ImageFormat::RGBA:
	case ImageFormat::RGBA16F:
	case ImageFormat::RGB:
	case ImageFormat::SRGB:
	case ImageFormat::DEPTH24STENCIL8:
		return false;
	}
	X2_CORE_ASSERT(false);
	return false;
}

uint32_t X2::Utils::CalculateMipCount(uint32_t width, uint32_t height)
{
	return (uint32_t)std::floor(std::log2(glm::min(width, height))) + 1;
}

uint32_t X2::Utils::GetImageMemorySize(ImageFormat format, uint32_t width, uint32_t height)
{
	return width * height * GetImageFormatBPP(format);
}

bool X2::Utils::IsDepthFormat(ImageFormat format)
{
	if (format == ImageFormat::DEPTH24STENCIL8 || format == ImageFormat::DEPTH32F || format == ImageFormat::DEPTH32FSTENCIL8UINT)
		return true;

	return false;
}

VkFormat X2::Utils::VulkanImageFormat(ImageFormat format)
{
	switch (format)
	{
	case ImageFormat::RED8UN:               return VK_FORMAT_R8_UNORM;
	case ImageFormat::RED8UI:               return VK_FORMAT_R8_UINT;
	case ImageFormat::RED16UI:               return VK_FORMAT_R16_UINT;
	case ImageFormat::RED32UI:               return VK_FORMAT_R32_UINT;
	case ImageFormat::RED32F:				return VK_FORMAT_R32_SFLOAT;
	case ImageFormat::RG8:				    return VK_FORMAT_R8G8_UNORM;
	case ImageFormat::RG16F:				return VK_FORMAT_R16G16_SFLOAT;
	case ImageFormat::RG32F:				return VK_FORMAT_R32G32_SFLOAT;
	case ImageFormat::RGBA:					return VK_FORMAT_R8G8B8A8_UNORM;
	case ImageFormat::RGBA16F:				return VK_FORMAT_R16G16B16A16_SFLOAT;
	case ImageFormat::RGBA32F:				return VK_FORMAT_R32G32B32A32_SFLOAT;
	case ImageFormat::B10R11G11UF:			return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case ImageFormat::DEPTH32FSTENCIL8UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case ImageFormat::DEPTH32F:				return VK_FORMAT_D32_SFLOAT;
	case ImageFormat::DEPTH24STENCIL8:		return VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetDepthFormat();
	}
	X2_CORE_ASSERT(false);
	return VK_FORMAT_UNDEFINED;
}