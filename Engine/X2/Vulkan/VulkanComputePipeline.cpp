#include "Precompiled.h"
#include "VulkanComputePipeline.h"

#include "X2/Renderer/Renderer.h"

#include "VulkanContext.h"
//#include "VulkanDiagnostics.h"

#include "X2/Core/Timer.h"

namespace X2 {

	static VkFence s_ComputeFence = nullptr;

	VulkanComputePipeline::VulkanComputePipeline(Ref<VulkanShader> computeShader)
		: m_Shader(computeShader)
	{
		Ref<VulkanComputePipeline> instance = this;
		Renderer::Submit([instance]() mutable
			{
				instance->RT_CreatePipeline();
			});
		Renderer::RegisterShaderDependency(computeShader, this);
	}

	void VulkanComputePipeline::CreatePipeline()
	{
		Renderer::Submit([instance = Ref(this)]() mutable
			{
				instance->RT_CreatePipeline();
			});
	}

	void VulkanComputePipeline::RT_CreatePipeline()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// TODO: Abstract into some sort of compute pipeline

		auto descriptorSetLayouts = m_Shader->GetAllDescriptorSetLayouts();

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		const auto& pushConstantRanges = m_Shader->GetPushConstantRanges();
		std::vector<VkPushConstantRange> vulkanPushConstantRanges(pushConstantRanges.size());
		if (pushConstantRanges.size())
		{
			// TODO: should come from shader
			for (uint32_t i = 0; i < pushConstantRanges.size(); i++)
			{
				const auto& pushConstantRange = pushConstantRanges[i];
				auto& vulkanPushConstantRange = vulkanPushConstantRanges[i];

				vulkanPushConstantRange.stageFlags = pushConstantRange.ShaderStage;
				vulkanPushConstantRange.offset = pushConstantRange.Offset;
				vulkanPushConstantRange.size = pushConstantRange.Size;
			}

			pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)vulkanPushConstantRanges.size();
			pipelineLayoutCreateInfo.pPushConstantRanges = vulkanPushConstantRanges.data();
		}

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_ComputePipelineLayout));

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_ComputePipelineLayout;
		computePipelineCreateInfo.flags = 0;
		const auto& shaderStages = m_Shader->GetPipelineShaderStageCreateInfos();
		computePipelineCreateInfo.stage = shaderStages[0];

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));
		VK_CHECK_RESULT(vkCreateComputePipelines(device, m_PipelineCache, 1, &computePipelineCreateInfo, nullptr, &m_ComputePipeline));

		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, m_Shader->GetName(), m_ComputePipeline);
	}

	void VulkanComputePipeline::Execute(VkDescriptorSet* descriptorSets, uint32_t descriptorSetCount, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkQueue computeQueue = VulkanContext::GetCurrentDevice()->GetComputeQueue();
		//vkQueueWaitIdle(computeQueue); // TODO: don't

		VkCommandBuffer computeCommandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true, true);

		//Utils::SetVulkanCheckpoint(computeCommandBuffer, "VulkanComputePipeline::Execute");

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
		for (uint32_t i = 0; i < descriptorSetCount; i++)
		{
			vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelineLayout, 0, 1, &descriptorSets[i], 0, 0);
			vkCmdDispatch(computeCommandBuffer, groupCountX, groupCountY, groupCountZ);
		}

		vkEndCommandBuffer(computeCommandBuffer);
		if (!s_ComputeFence)
		{

			VkFenceCreateInfo fenceCreateInfo{};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &s_ComputeFence));

			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, fmt::format("Compute pipeline fence"), s_ComputeFence);
		}

		// Make sure previous compute shader in pipeline has completed (TODO: this shouldn't be needed for all cases)
		vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &s_ComputeFence);

		VkSubmitInfo computeSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, s_ComputeFence));

		// Wait for execution of compute shader to complete
		// Currently this is here for "safety"
		{
			X2_SCOPE_TIMER("Compute shader execution");
			vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		}
	}

	void VulkanComputePipeline::Begin(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		X2_CORE_ASSERT(!m_ActiveComputeCommandBuffer);

		if (renderCommandBuffer)
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			m_ActiveComputeCommandBuffer = renderCommandBuffer->GetCommandBuffer(frameIndex);
			m_UsingGraphicsQueue = true;
		}
		else
		{
			m_ActiveComputeCommandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			m_UsingGraphicsQueue = false;
		}
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
	}

	void VulkanComputePipeline::RT_Begin(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		X2_CORE_ASSERT(!m_ActiveComputeCommandBuffer);

		if (renderCommandBuffer)
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			m_ActiveComputeCommandBuffer = renderCommandBuffer->GetCommandBuffer(frameIndex);
			m_UsingGraphicsQueue = true;
		}
		else
		{
			m_ActiveComputeCommandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			m_UsingGraphicsQueue = false;
		}
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
	}


	void VulkanComputePipeline::Dispatch(VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
	{
		X2_CORE_ASSERT(m_ActiveComputeCommandBuffer);

		vkCmdBindDescriptorSets(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelineLayout, 0, 1, &descriptorSet, 0, 0);
		vkCmdDispatch(m_ActiveComputeCommandBuffer, groupCountX, groupCountY, groupCountZ);
	}

	void VulkanComputePipeline::End()
	{
		X2_CORE_ASSERT(m_ActiveComputeCommandBuffer);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		if (!m_UsingGraphicsQueue)
		{
			VkQueue computeQueue = VulkanContext::GetCurrentDevice()->GetComputeQueue();

			vkEndCommandBuffer(m_ActiveComputeCommandBuffer);

			if (!s_ComputeFence)
			{
				VkFenceCreateInfo fenceCreateInfo{};
				fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
				VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &s_ComputeFence));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, "Compute pipeline fence", s_ComputeFence);
			}
			vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
			vkResetFences(device, 1, &s_ComputeFence);

			VkSubmitInfo computeSubmitInfo{};
			computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo.commandBufferCount = 1;
			computeSubmitInfo.pCommandBuffers = &m_ActiveComputeCommandBuffer;
			VK_CHECK_RESULT(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, s_ComputeFence));

			// Wait for execution of compute shader to complete
			// Currently this is here for "safety"
			{
				X2_SCOPE_TIMER("Compute shader execution");
				vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
			}
		}
		m_ActiveComputeCommandBuffer = nullptr;
	}

	void VulkanComputePipeline::SetPushConstants(const void* data, uint32_t size) const
	{
		vkCmdPushConstants(m_ActiveComputeCommandBuffer, m_ComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, data);
	}


}
