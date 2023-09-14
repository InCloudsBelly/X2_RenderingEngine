#pragma once

#include "X2/Core/Ref.h"
#include "vulkan/vulkan.h"
#include <string>
#include <vector>

#include "VulkanPipeline.h"

namespace X2 {

	class VulkanRenderCommandBuffer 
	{
	public:
		VulkanRenderCommandBuffer(uint32_t count = 0, std::string debugName = "");
		VulkanRenderCommandBuffer(std::string debugName, bool swapchain);
		~VulkanRenderCommandBuffer() ;

		virtual void Begin() ;
		virtual void End() ;
		virtual void Submit() ;

		virtual float GetExecutionGPUTime(uint32_t frameIndex, uint32_t queryIndex = 0) const 
		{
			if (queryIndex == UINT32_MAX || queryIndex / 2 >= m_TimestampNextAvailableQuery / 2)
				return 0.0f;

			return m_ExecutionGPUTimes[frameIndex][queryIndex / 2];
		}

		virtual const PipelineStatistics& GetPipelineStatistics(uint32_t frameIndex) const { return m_PipelineStatisticsQueryResults[frameIndex]; }

		virtual uint32_t BeginTimestampQuery() ;
		virtual void EndTimestampQuery(uint32_t queryID) ;

		VkCommandBuffer GetActiveCommandBuffer() const { return m_ActiveCommandBuffer; }

		VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const
		{
			X2_CORE_ASSERT(frameIndex < m_CommandBuffers.size());
			return m_CommandBuffers[frameIndex];
		}
	private:
		std::string m_DebugName;
		VkCommandPool m_CommandPool = nullptr;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		VkCommandBuffer m_ActiveCommandBuffer = nullptr;
		std::vector<VkFence> m_WaitFences;

		bool m_OwnedBySwapChain = false;

		uint32_t m_TimestampQueryCount = 0;
		uint32_t m_TimestampNextAvailableQuery = 2;
		std::vector<VkQueryPool> m_TimestampQueryPools;
		std::vector<VkQueryPool> m_PipelineStatisticsQueryPools;
		std::vector<std::vector<uint64_t>> m_TimestampQueryResults;
		std::vector<std::vector<float>> m_ExecutionGPUTimes;

		uint32_t m_PipelineQueryCount = 0;
		std::vector<PipelineStatistics> m_PipelineStatisticsQueryResults;
	};

}
