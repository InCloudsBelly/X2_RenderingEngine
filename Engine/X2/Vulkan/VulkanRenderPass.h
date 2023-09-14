#pragma once
#include "Vulkan.h"
#include "X2/Core/Ref.h"
#include "VulkanFramebuffer.h"

namespace X2
{
    struct RenderPassSpecification
    {
        Ref<VulkanFramebuffer> TargetFramebuffer;
        std::string DebugName;
        glm::vec4 MarkerColor;
    };

	class VulkanRenderPass 
	{
	public:
		VulkanRenderPass(const RenderPassSpecification& spec);
		virtual ~VulkanRenderPass();

		virtual RenderPassSpecification& GetSpecification()  { return m_Specification; }
		virtual const RenderPassSpecification& GetSpecification() const  { return m_Specification; }
	private:
		RenderPassSpecification m_Specification;
	};
}