#pragma once

#include "X2/Core/Ref.h"

#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "VulkanRenderCommandBuffer.h"

#include "vulkan/vulkan.h"

namespace X2 {

	class VulkanComputePipeline : public RefCounted
	{
	public:
		VulkanComputePipeline(Ref<VulkanShader> computeShader);

		void Execute(VkDescriptorSet* descriptorSets, uint32_t descriptorSetCount, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		virtual void Begin(Ref<VulkanRenderCommandBuffer> renderCommandBuffer = nullptr) ;
		virtual void RT_Begin(Ref<VulkanRenderCommandBuffer> renderCommandBuffer = nullptr) ;
		void Dispatch(VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const;
		virtual void End();

		virtual Ref<VulkanShader> GetShader() { return m_Shader; }

		VkCommandBuffer GetActiveCommandBuffer() { return m_ActiveComputeCommandBuffer; }

		void SetPushConstants(const void* data, uint32_t size) const;
		void CreatePipeline();
	private:
		void RT_CreatePipeline();
	private:
		Ref<VulkanShader> m_Shader;

		VkPipelineLayout m_ComputePipelineLayout = nullptr;
		VkPipelineCache m_PipelineCache = nullptr;
		VkPipeline m_ComputePipeline = nullptr;

		VkCommandBuffer m_ActiveComputeCommandBuffer = nullptr;

		bool m_UsingGraphicsQueue = false;
	};

}
