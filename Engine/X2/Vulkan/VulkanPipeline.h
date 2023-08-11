#pragma once

#include "X2/Core/Ref.h"
#include "VulkanShader.h"
#include "Vulkan.h"
#include "VulkanVertexBuffer.h"

#include <map>


namespace X2 {

	class VulkanRenderPass;
	class VulkanUniformBuffer;

	enum class PrimitiveTopology
	{
		None = 0,
		Points,
		Lines,
		Triangles,
		LineStrip,
		TriangleStrip,
		TriangleFan
	};

	enum class DepthCompareOperator
	{
		None = 0,
		Never,
		NotEqual,
		Less,
		LessOrEqual,
		Greater,
		GreaterOrEqual,
		Equal,
		Always,
	};

	struct PipelineSpecification
	{
		Ref<VulkanShader> Shader;
		VertexBufferLayout Layout;
		VertexBufferLayout InstanceLayout;
		VertexBufferLayout BoneInfluenceLayout;
		Ref<VulkanRenderPass> RenderPass;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		DepthCompareOperator DepthOperator = DepthCompareOperator::LessOrEqual;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		bool Wireframe = false;
		float LineWidth = 1.0f;

		std::string DebugName;
	};

	struct PipelineStatistics
	{
		uint64_t InputAssemblyVertices = 0;
		uint64_t InputAssemblyPrimitives = 0;
		uint64_t VertexShaderInvocations = 0;
		uint64_t ClippingInvocations = 0;
		uint64_t ClippingPrimitives = 0;
		uint64_t FragmentShaderInvocations = 0;
		uint64_t ComputeShaderInvocations = 0;

		// TODO(Yan): tesselation shader stats when we have them
	};

	class VulkanPipeline : public RefCounted
	{
	public:
		VulkanPipeline(const PipelineSpecification& spec);
		virtual ~VulkanPipeline();

		virtual PipelineSpecification& GetSpecification() { return m_Specification; }
		virtual const PipelineSpecification& GetSpecification() const { return m_Specification; }

		virtual void Invalidate() ;
		virtual void SetUniformBuffer(Ref<VulkanUniformBuffer> uniformBuffer, uint32_t binding, uint32_t set = 0) ;

		virtual void Bind() ;

		VkPipeline GetVulkanPipeline() { return m_VulkanPipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() { return m_PipelineLayout; }
		VkDescriptorSet GetDescriptorSet(uint32_t set = 0)
		{
			X2_CORE_ASSERT(m_DescriptorSets.DescriptorSets.size() > set);
			return m_DescriptorSets.DescriptorSets[set];
		}

		const std::vector<VkDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets.DescriptorSets; }

		void RT_SetUniformBuffer(Ref<VulkanUniformBuffer> uniformBuffer, uint32_t binding, uint32_t set = 0);
	private:
		PipelineSpecification m_Specification;

		VkPipelineLayout m_PipelineLayout = nullptr;
		VkPipeline m_VulkanPipeline = nullptr;
		VkPipelineCache m_PipelineCache = nullptr;
		VulkanShader::ShaderMaterialDescriptorSet m_DescriptorSets;
	};

}
