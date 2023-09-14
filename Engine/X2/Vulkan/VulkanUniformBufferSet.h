#pragma once

#include "X2/Core/Ref.h"
#include "VulkanUniformBuffer.h"

#include <map>

namespace X2 {

	class VulkanUniformBufferSet 
	{
	public:
		VulkanUniformBufferSet(uint32_t frames)
			: m_Frames(frames) {}
		virtual ~VulkanUniformBufferSet() {}

		virtual void Create(uint32_t size, uint32_t binding) 
		{
			for (uint32_t frame = 0; frame < m_Frames; frame++)
			{
				Ref<VulkanUniformBuffer> uniformBuffer = std::make_shared<VulkanUniformBuffer>(size, binding);
				Set(uniformBuffer, 0, frame);
			}
		}

		virtual Ref<VulkanUniformBuffer> Get(uint32_t binding, uint32_t set = 0, uint32_t frame = 0) 
		{
			X2_CORE_ASSERT(m_UniformBuffers.find(frame) != m_UniformBuffers.end());
			X2_CORE_ASSERT(m_UniformBuffers.at(frame).find(set) != m_UniformBuffers.at(frame).end());
			X2_CORE_ASSERT(m_UniformBuffers.at(frame).at(set).find(binding) != m_UniformBuffers.at(frame).at(set).end());

			return m_UniformBuffers.at(frame).at(set).at(binding);
		}

		virtual void Set(Ref<VulkanUniformBuffer> uniformBuffer, uint32_t set = 0, uint32_t frame = 0) 
		{
			m_UniformBuffers[frame][set][uniformBuffer->GetBinding()] = uniformBuffer;
		}
	private:
		uint32_t m_Frames;
		std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, Ref<VulkanUniformBuffer>>>> m_UniformBuffers; // frame->set->binding
	};
}
