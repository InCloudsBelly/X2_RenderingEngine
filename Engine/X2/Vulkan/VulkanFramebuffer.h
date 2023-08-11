#pragma once


#include "Vulkan.h"
#include "VulkanImage.h"

namespace X2 {

	class VulkanFramebuffer;
	enum class FramebufferBlendMode
	{
		None = 0,
		OneZero,
		SrcAlphaOneMinusSrcAlpha,
		Additive,
		Zero_SrcColor
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(ImageFormat format) : Format(format) {}

		ImageFormat Format;
		bool Blend = true;
		FramebufferBlendMode BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
		// TODO: filtering/wrap
	};

	struct FramebufferAttachmentSpecification
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(const std::initializer_list<FramebufferTextureSpecification>& attachments)
			: Attachments(attachments) {}

		std::vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		float Scale = 1.0f;
		uint32_t Width = 0;
		uint32_t Height = 0;
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		float DepthClearValue = 0.0f;
		bool ClearColorOnLoad = true;
		bool ClearDepthOnLoad = true;

		FramebufferAttachmentSpecification Attachments;
		uint32_t Samples = 1; // multisampling

		// TODO: Temp, needs scale
		bool NoResize = false;

		// Master switch (individual attachments can be disabled in FramebufferTextureSpecification)
		bool Blend = true;
		// None means use BlendMode in FramebufferTextureSpecification
		FramebufferBlendMode BlendMode = FramebufferBlendMode::None;

		// SwapChainTarget = screen buffer (i.e. no framebuffer)
		bool SwapChainTarget = false;

		// Will it be used for transfer ops?
		bool Transfer = false;

		// Note: these are used to attach multi-layered color/depth images 
		Ref<VulkanImage2D> ExistingImage;
		std::vector<uint32_t> ExistingImageLayers;

		// Specify existing images to attach instead of creating
		// new images. attachment index -> image
		std::map<uint32_t, Ref<VulkanImage2D>> ExistingImages;

		// At the moment this will just create a new render pass
		// with an existing framebuffer
		Ref<VulkanFramebuffer> ExistingFramebuffer;

		std::string DebugName;
	};

	class VulkanFramebuffer :public RefCounted
	{
	public:
		VulkanFramebuffer(const FramebufferSpecification& spec);
		virtual ~VulkanFramebuffer();

		virtual void Resize(uint32_t width, uint32_t height, bool forceRecreate = false) ;
		virtual void AddResizeCallback(const std::function<void(Ref<VulkanFramebuffer>)>& func) ;

		virtual void Bind() const  {}
		virtual void Unbind() const  {}

		virtual void BindTexture(uint32_t attachmentIndex = 0, uint32_t slot = 0) const  {}

		virtual uint32_t GetWidth() const  { return m_Width; }
		virtual uint32_t GetHeight() const  { return m_Height; }
		virtual uint32_t GetRendererID() const { return m_RendererID; }
		virtual uint32_t GetColorAttachmentRendererID() const { return 0; }
		virtual uint32_t GetDepthAttachmentRendererID() const { return 0; }

		virtual Ref<VulkanImage2D> GetImage(uint32_t attachmentIndex = 0) const { X2_CORE_ASSERT(attachmentIndex < m_AttachmentImages.size()); return m_AttachmentImages[attachmentIndex]; }
		virtual Ref<VulkanImage2D> GetDepthImage() const { return m_DepthAttachmentImage; }
		size_t GetColorAttachmentCount() const { return m_Specification.SwapChainTarget ? 1 : m_AttachmentImages.size(); }
		bool HasDepthAttachment() const { return (bool)m_DepthAttachmentImage; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }
		VkFramebuffer GetVulkanFramebuffer() const { return m_Framebuffer; }
		const std::vector<VkClearValue>& GetVulkanClearValues() const { return m_ClearValues; }

		virtual const FramebufferSpecification& GetSpecification() const { return m_Specification; }

		void Invalidate();
		void RT_Invalidate();
		void Release();
	private:
		FramebufferSpecification m_Specification;
		uint32_t m_RendererID = 0;
		uint32_t m_Width = 0, m_Height = 0;

		std::vector<Ref<VulkanImage2D>> m_AttachmentImages;
		Ref<VulkanImage2D> m_DepthAttachmentImage;

		std::vector<VkClearValue> m_ClearValues;

		VkRenderPass m_RenderPass = nullptr;
		VkFramebuffer m_Framebuffer = nullptr;

		std::vector<std::function<void(Ref<VulkanFramebuffer>)>> m_ResizeCallbacks;
	};

}
