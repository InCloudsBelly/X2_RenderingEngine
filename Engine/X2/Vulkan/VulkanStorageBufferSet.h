#pragma once

#include "X2/Core/Ref.h"
#include "VulkanStorageBuffer.h"

#include <map>
#include "X2/Core/Assert.h"

namespace X2 {

	class VulkanStorageBufferSet 
	{
	public:
		explicit VulkanStorageBufferSet(uint32_t frames)
			: m_Frames(frames) {}

		~VulkanStorageBufferSet() = default;

		virtual void Create(uint32_t size, uint32_t binding) 
		{
			for (uint32_t frame = 0; frame < m_Frames; frame++)
			{
				const Ref<VulkanStorageBuffer> storageBuffer = std::make_shared<VulkanStorageBuffer>(size, binding);
				Set(storageBuffer, 0, frame);
			}
		}


		virtual void Resize(const uint32_t binding, const uint32_t set, const uint32_t newSize) 
		{
			for (uint32_t frame = 0; frame < m_Frames; frame++)
			{
				m_StorageBuffers.at(frame).at(set).at(binding)->Resize(newSize);
			}
		}

		virtual Ref<VulkanStorageBuffer> Get(uint32_t binding, uint32_t set = 0, uint32_t frame = 0) 
		{
			X2_CORE_ASSERT(m_StorageBuffers.find(frame) != m_StorageBuffers.end());
			X2_CORE_ASSERT(m_StorageBuffers.at(frame).find(set) != m_StorageBuffers.at(frame).end());
			X2_CORE_ASSERT(m_StorageBuffers.at(frame).at(set).find(binding) != m_StorageBuffers.at(frame).at(set).end());

			return m_StorageBuffers.at(frame).at(set).at(binding);
		}

		virtual void Set(Ref<VulkanStorageBuffer> storageBuffer, uint32_t set = 0, uint32_t frame = 0)
		{
			m_StorageBuffers[frame][set][storageBuffer->GetBinding()] = storageBuffer;
		}

	private:
		uint32_t m_Frames;
		std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, Ref<VulkanStorageBuffer>>>> m_StorageBuffers; // frame->set->binding
	};
}