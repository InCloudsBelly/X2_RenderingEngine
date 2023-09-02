#include "Precompiled.h"
#include "VulkanRenderCommandBuffer.h"

#include <utility>

#include "X2/Core/Application.h"

#include "VulkanContext.h"'

#include "X2/Renderer/Renderer.h"

namespace X2 {

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(uint32_t count, std::string debugName)
		: m_DebugName(std::move(debugName))
	{
		auto device = VulkanContext::GetCurrentDevice();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = device->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device->GetVulkanDevice(), &cmdPoolInfo, nullptr, &m_CommandPool));
		VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_COMMAND_POOL, m_DebugName, m_CommandPool);

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = m_CommandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		if (count == 0)
			count = framesInFlight;
		commandBufferAllocateInfo.commandBufferCount = count;
		m_CommandBuffers.resize(count);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device->GetVulkanDevice(), &commandBufferAllocateInfo, m_CommandBuffers.data()));

		for (uint32_t i = 0; i < count; ++i)
			VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_COMMAND_BUFFER, fmt::format("{} (frame in flight: {})", m_DebugName, i), m_CommandBuffers[i]);

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		m_WaitFences.resize(framesInFlight);
		for (size_t i = 0; i < m_WaitFences.size(); ++i)
		{
			VK_CHECK_RESULT(vkCreateFence(device->GetVulkanDevice(), &fenceCreateInfo, nullptr, &m_WaitFences[i]));
			VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_FENCE, fmt::format("{} (frame in flight: {}) fence", m_DebugName, i), m_WaitFences[i]);
		}

		VkQueryPoolCreateInfo queryPoolCreateInfo = {};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.pNext = nullptr;

		// Timestamp queries
		const uint32_t maxUserQueries = 24;
		m_TimestampQueryCount = 2 + 2 * maxUserQueries;

		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolCreateInfo.queryCount = m_TimestampQueryCount;
		m_TimestampQueryPools.resize(framesInFlight);
		for (auto& timestampQueryPool : m_TimestampQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device->GetVulkanDevice(), &queryPoolCreateInfo, nullptr, &timestampQueryPool));

		m_TimestampQueryResults.resize(framesInFlight);
		for (auto& timestampQueryResults : m_TimestampQueryResults)
			timestampQueryResults.resize(m_TimestampQueryCount);

		m_ExecutionGPUTimes.resize(framesInFlight);
		for (auto& executionGPUTimes : m_ExecutionGPUTimes)
			executionGPUTimes.resize(m_TimestampQueryCount / 2);

		// Pipeline statistics queries
		m_PipelineQueryCount = 7;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		queryPoolCreateInfo.queryCount = m_PipelineQueryCount;
		queryPoolCreateInfo.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

		m_PipelineStatisticsQueryPools.resize(framesInFlight);
		for (auto& pipelineStatisticsQueryPools : m_PipelineStatisticsQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device->GetVulkanDevice(), &queryPoolCreateInfo, nullptr, &pipelineStatisticsQueryPools));

		m_PipelineStatisticsQueryResults.resize(framesInFlight);
	}

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(std::string debugName, bool swapchain)
		: m_DebugName(std::move(debugName)), m_OwnedBySwapChain(true)
	{
		auto device = VulkanContext::GetCurrentDevice();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		VkQueryPoolCreateInfo queryPoolCreateInfo = {};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.pNext = nullptr;

		// Timestamp queries
		const uint32_t maxUserQueries = 24;
		m_TimestampQueryCount = 2 + 2 * maxUserQueries;

		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolCreateInfo.queryCount = m_TimestampQueryCount;
		m_TimestampQueryPools.resize(framesInFlight);
		for (auto& timestampQueryPool : m_TimestampQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device->GetVulkanDevice(), &queryPoolCreateInfo, nullptr, &timestampQueryPool));

		m_TimestampQueryResults.resize(framesInFlight);
		for (auto& timestampQueryResults : m_TimestampQueryResults)
			timestampQueryResults.resize(m_TimestampQueryCount);

		m_ExecutionGPUTimes.resize(framesInFlight);
		for (auto& executionGPUTimes : m_ExecutionGPUTimes)
			executionGPUTimes.resize(m_TimestampQueryCount / 2);

		// Pipeline statistics queries
		m_PipelineQueryCount = 7;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		queryPoolCreateInfo.queryCount = m_PipelineQueryCount;
		queryPoolCreateInfo.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

		m_PipelineStatisticsQueryPools.resize(framesInFlight);
		for (auto& pipelineStatisticsQueryPools : m_PipelineStatisticsQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device->GetVulkanDevice(), &queryPoolCreateInfo, nullptr, &pipelineStatisticsQueryPools));

		m_PipelineStatisticsQueryResults.resize(framesInFlight);
	}

	VulkanRenderCommandBuffer::~VulkanRenderCommandBuffer()
	{
		if (m_OwnedBySwapChain)
			return;

		VkCommandPool commandPool = m_CommandPool;
		Renderer::SubmitResourceFree([commandPool]()
			{
				auto device = VulkanContext::GetCurrentDevice();
				vkDestroyCommandPool(device->GetVulkanDevice(), commandPool, nullptr);
			});
	}

	void VulkanRenderCommandBuffer::Begin()
	{
		m_TimestampNextAvailableQuery = 2;

		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance]() mutable
			{
				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				VkCommandBufferBeginInfo cmdBufInfo = {};
				cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				cmdBufInfo.pNext = nullptr;

				VkCommandBuffer commandBuffer = nullptr;
				if (instance->m_OwnedBySwapChain)
				{
					VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
					commandBuffer = swapChain.GetDrawCommandBuffer(frameIndex);
				}
				else
				{
					commandBuffer = instance->m_CommandBuffers[frameIndex];
				}
				instance->m_ActiveCommandBuffer = commandBuffer;
				VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

				// Timestamp query
				vkCmdResetQueryPool(commandBuffer, instance->m_TimestampQueryPools[frameIndex], 0, instance->m_TimestampQueryCount);
				vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, instance->m_TimestampQueryPools[frameIndex], 0);

				// Pipeline stats query
				vkCmdResetQueryPool(commandBuffer, instance->m_PipelineStatisticsQueryPools[frameIndex], 0, instance->m_PipelineQueryCount);
				vkCmdBeginQuery(commandBuffer, instance->m_PipelineStatisticsQueryPools[frameIndex], 0, 0);
			});
	}

	void VulkanRenderCommandBuffer::End()
	{
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance]() mutable
			{
				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = instance->m_ActiveCommandBuffer;
				vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, instance->m_TimestampQueryPools[frameIndex], 1);
				vkCmdEndQuery(commandBuffer, instance->m_PipelineStatisticsQueryPools[frameIndex], 0);
				VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

				instance->m_ActiveCommandBuffer = nullptr;
			});
	}

	void VulkanRenderCommandBuffer::Submit()
	{
		if (m_OwnedBySwapChain)
			return;

		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance]() mutable
			{
				auto device = VulkanContext::GetCurrentDevice();

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				VkCommandBuffer commandBuffer = instance->m_CommandBuffers[frameIndex];
				submitInfo.pCommandBuffers = &commandBuffer;

				VK_CHECK_RESULT(vkWaitForFences(device->GetVulkanDevice(), 1, &instance->m_WaitFences[frameIndex], VK_TRUE, UINT64_MAX));
				VK_CHECK_RESULT(vkResetFences(device->GetVulkanDevice(), 1, &instance->m_WaitFences[frameIndex]));

				X2_CORE_TRACE_TAG("Renderer", "Submitting Render Command Buffer {}", instance->m_DebugName);

				VK_CHECK_RESULT(vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, instance->m_WaitFences[frameIndex]));

				// Retrieve timestamp query results
				vkGetQueryPoolResults(device->GetVulkanDevice(), instance->m_TimestampQueryPools[frameIndex], 0, instance->m_TimestampNextAvailableQuery,
					instance->m_TimestampNextAvailableQuery * sizeof(uint64_t), instance->m_TimestampQueryResults[frameIndex].data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

				for (uint32_t i = 0; i < instance->m_TimestampNextAvailableQuery; i += 2)
				{
					uint64_t startTime = instance->m_TimestampQueryResults[frameIndex][i];
					uint64_t endTime = instance->m_TimestampQueryResults[frameIndex][i + 1];
					float nsTime = endTime > startTime ? (endTime - startTime) * device->GetPhysicalDevice()->GetLimits().timestampPeriod : 0.0f;
					instance->m_ExecutionGPUTimes[frameIndex][i / 2] = nsTime * 0.000001f; // Time in ms
				}

				// Retrieve pipeline stats results
				vkGetQueryPoolResults(device->GetVulkanDevice(), instance->m_PipelineStatisticsQueryPools[frameIndex], 0, 1,
					sizeof(PipelineStatistics), &instance->m_PipelineStatisticsQueryResults[frameIndex], sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
			});
	}

	uint32_t VulkanRenderCommandBuffer::BeginTimestampQuery()
	{
		uint32_t queryIndex = m_TimestampNextAvailableQuery;
		m_TimestampNextAvailableQuery += 2;
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance, queryIndex]()
			{
				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = instance->m_CommandBuffers[frameIndex];
				vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, instance->m_TimestampQueryPools[frameIndex], queryIndex);
			});
		return queryIndex;
	}

	void VulkanRenderCommandBuffer::EndTimestampQuery(uint32_t queryID)
	{
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance, queryID]()
			{
				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = instance->m_CommandBuffers[frameIndex];
				vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, instance->m_TimestampQueryPools[frameIndex], queryID + 1);
			});
	}

}
