#pragma once

#include "Vulkan.h"

#include "VulkanImage.h"
#include "X2/Asset/Asset.h"

#include <filesystem>

namespace X2 {

	struct TextureSpecification
	{
		ImageFormat Format = ImageFormat::RGBA;
		uint32_t Width = 1;
		uint32_t Height = 1;
		TextureWrap SamplerWrap = TextureWrap::Repeat;
		TextureFilter SamplerFilter = TextureFilter::Linear;

		bool GenerateMips = true;
		bool SRGB = false;
		bool Storage = false;
		bool StoreLocally = false;

		std::string DebugName;
	};


	class Texture : public Asset
	{
	public:
		virtual ~Texture() {}

		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual ImageFormat GetFormat() const = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual glm::uvec2 GetSize() const = 0;

		virtual uint32_t GetMipLevelCount() const = 0;
		virtual std::pair<uint32_t, uint32_t> GetMipSize(uint32_t mip) const = 0;

		virtual uint64_t GetHash() const = 0;

		virtual TextureType GetType() const = 0;
	};


	class VulkanTexture2D : public Texture
	{
	public:
		VulkanTexture2D(const TextureSpecification& specification, const std::filesystem::path& filepath);
		VulkanTexture2D(const TextureSpecification& specification, Buffer data = Buffer());
		~VulkanTexture2D() override;
		virtual void Resize(const glm::uvec2& size);
		virtual void Resize(uint32_t width, uint32_t height);

		void Invalidate();

		virtual ImageFormat GetFormat() const override { return m_Specification.Format; }
		virtual uint32_t GetWidth() const override { return m_Specification.Width; }
		virtual uint32_t GetHeight() const override { return m_Specification.Height; }
		virtual glm::uvec2 GetSize() const override { return { m_Specification.Width, m_Specification.Height }; }

		virtual void Bind(uint32_t slot = 0) const override;

		virtual Ref<VulkanImage2D> GetImage() const  { return m_Image; }
		const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_Image.As<VulkanImage2D>()->GetDescriptorInfo(); }

		void Lock();
		void Unlock();

		Buffer GetWriteableBuffer() ;
		bool Loaded() const  { return m_ImageData; }
		const std::filesystem::path& GetPath() const ;
		uint32_t GetMipLevelCount() const override;
		virtual std::pair<uint32_t, uint32_t> GetMipSize(uint32_t mip) const override;

		void GenerateMips();

		virtual uint64_t GetHash() const { return (uint64_t)m_Image.As<VulkanImage2D>()->GetDescriptorInfo().imageView; }
		virtual TextureType GetType() const override { return TextureType::Texture2D; }

		void CopyToHostBuffer(Buffer& buffer);
	private:
		std::filesystem::path m_Path;
		TextureSpecification m_Specification;

		Buffer m_ImageData;

		Ref<VulkanImage2D> m_Image;
	};

	class VulkanTextureCube : public Texture
	{
	public:
		VulkanTextureCube(const TextureSpecification& specification, Buffer data = nullptr);
		virtual ~VulkanTextureCube();

		void Release();

		virtual void Bind(uint32_t slot = 0) const override {}

		virtual ImageFormat GetFormat() const override { return m_Specification.Format; }

		virtual uint32_t GetWidth() const override { return m_Specification.Width; }
		virtual uint32_t GetHeight() const override { return m_Specification.Height; }
		virtual glm::uvec2 GetSize() const override { return { m_Specification.Width, m_Specification.Height }; }

		virtual uint32_t GetMipLevelCount() const override;
		virtual std::pair<uint32_t, uint32_t> GetMipSize(uint32_t mip) const override;

		virtual uint64_t GetHash() const override { return (uint64_t)m_Image; }
		virtual TextureType GetType() const override { return TextureType::TextureCube; }

		const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_DescriptorImageInfo; }
		VkImageView CreateImageViewSingleMip(uint32_t mip);

		void GenerateMips(bool readonly = false);

		void CopyToHostBuffer(Buffer& buffer);
		void CopyFromBuffer(const Buffer& buffer, uint32_t mips);
	private:
		void Invalidate();
	private:
		TextureSpecification m_Specification;

		bool m_MipsGenerated = false;

		Buffer m_LocalStorage;
		VmaAllocation m_MemoryAlloc;
		uint64_t m_GPUAllocationSize = 0;
		VkImage m_Image{ nullptr };
		VkDescriptorImageInfo m_DescriptorImageInfo = {};
	};

};
