#pragma once
#include "X2/Core/Buffer.h"

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include <unordered_map>

namespace X2 {

	enum class ImageFormat
	{
		None = 0,
		RED8UN,
		RED8UI,
		RED16UI,
		RED32UI,
		RED32F,
		RG8,
		RG16F,
		RG32F,
		RGB,
		RGBA,
		RGBA16F,
		RGBA32F,

		B10R11G11UF,
		A2B10G10R10U,

		SRGB,

		DEPTH32FSTENCIL8UINT,
		DEPTH32F,
		DEPTH24STENCIL8,

		// Defaults
		Depth = DEPTH24STENCIL8,
	};

	enum class ImageUsage
	{
		None = 0,
		Texture,
		Attachment,
		Storage,
		HostRead
	};

	enum class TextureWrap
	{
		None = 0,
		Clamp,
		Repeat
	};

	enum class TextureFilter
	{
		None = 0,
		Linear,
		Nearest,
		Cubic
	};

	enum class TextureType
	{
		None = 0,
		Texture2D,
		TextureCube
	};

	enum class ImageType
	{
		Image1D = 0,
		Image1DArray,
		Image2D,
		Image2DArray,
		Image3D,
		ImageCube,
		ImageCubeArray
	};

	struct ImageSpecification
	{
		std::string DebugName;

		ImageType Type = ImageType::Image2D;
		ImageFormat Format = ImageFormat::RGBA;
		ImageUsage Usage = ImageUsage::Texture;
		bool Transfer = false; // Will it be used for transfer ops?
		uint32_t Width = 1;
		uint32_t Height = 1;
		uint32_t Depth = 1;
		uint32_t Mips = 1;
		uint32_t Layers = 1;
	};


	struct VulkanImageInfo
	{
		VkImage Image = nullptr;
		VkImageView ImageView = nullptr;
		VkSampler Sampler = nullptr;
		VmaAllocation MemoryAlloc = nullptr;
	};


	class Image
	{
	public:
		virtual ~Image() = default;

		virtual void Resize(const uint32_t width, const uint32_t height) = 0;
		virtual void Invalidate() = 0;
		virtual void Release() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual glm::uvec2 GetSize() const = 0;

		virtual float GetAspectRatio() const = 0;

		virtual ImageSpecification& GetSpecification() = 0;
		virtual const ImageSpecification& GetSpecification() const = 0;

		virtual Buffer GetBuffer() const = 0;
		virtual Buffer& GetBuffer() = 0;

		virtual void CreatePerLayerImageViews() = 0;

		virtual uint64_t GetHash() const = 0;

		// TODO: usage (eg. shader read)
	};


	class VulkanImage2D : public Image
	{
	public:
	public:
		VulkanImage2D(const ImageSpecification& specification);
		virtual ~VulkanImage2D() override;

		virtual void Resize(const glm::uvec2& size) 
		{
			Resize(size.x, size.y);
		}
		virtual void Resize(const uint32_t width, const uint32_t height) override
		{
			m_Specification.Width = width;
			m_Specification.Height = height;
			Invalidate();
		}
		virtual void Invalidate() override;
		virtual void Release() override;

		virtual uint32_t GetWidth() const override { return m_Specification.Width; }
		virtual uint32_t GetHeight() const override { return m_Specification.Height; }
		virtual glm::uvec2 GetSize() const override { return { m_Specification.Width, m_Specification.Height }; }

		virtual float GetAspectRatio() const override { return (float)m_Specification.Width / (float)m_Specification.Height; }

		virtual ImageSpecification& GetSpecification() override { return m_Specification; }
		virtual const ImageSpecification& GetSpecification() const override { return m_Specification; }

		void RT_Invalidate();

		virtual void CreatePerLayerImageViews() override;

		void RT_CreatePerLayerImageViews();
		void RT_CreatePerSpecificLayerImageViews(const std::vector<uint32_t>& layerIndices);

		virtual VkImageView GetLayerImageView(uint32_t layer)
		{
			X2_CORE_ASSERT(layer < m_PerLayerImageViews.size());
			return m_PerLayerImageViews[layer];
		}

		VkImageView GetMipImageView(uint32_t mip);
		VkImageView RT_GetMipImageView(uint32_t mip);

		VulkanImageInfo& GetImageInfo() { return m_Info; }
		const VulkanImageInfo& GetImageInfo() const { return m_Info; }

		const VkDescriptorImageInfo& GetDescriptorInfo() const { return m_DescriptorImageInfo; }

		virtual Buffer GetBuffer() const override { return m_ImageData; }
		virtual Buffer& GetBuffer() override { return m_ImageData; }

		virtual uint64_t GetHash() const override { return (uint64_t)m_Info.Image; }

		void UpdateDescriptor();

		// Debug
		static const std::map<VkImage, VulkanImage2D*>& GetImageRefs();

		void CopyToHostBuffer(Buffer& buffer);
	private:
		ImageSpecification m_Specification;

		Buffer m_ImageData;

		VulkanImageInfo m_Info;
		VkDeviceSize m_GPUAllocationSize;

		std::vector<VkImageView> m_PerLayerImageViews;
		std::map<uint32_t, VkImageView> m_PerMipImageViews;
		VkDescriptorImageInfo m_DescriptorImageInfo = {};
	};



	namespace Utils {
		uint32_t GetImageFormatBPP(ImageFormat format);
		bool IsIntegerBased(const ImageFormat format);
		uint32_t CalculateMipCount(uint32_t width, uint32_t height);
		uint32_t GetImageMemorySize(ImageFormat format, uint32_t width, uint32_t height);
		bool IsDepthFormat(ImageFormat format);
		VkFormat VulkanImageFormat(ImageFormat format);


	}

}
