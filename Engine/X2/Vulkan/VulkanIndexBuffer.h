#pragma once

#include "X2/Core/Ref.h"
#include "X2/Core/Buffer.h"

#include "VulkanAllocator.h"

namespace X2 {

	class VulkanIndexBuffer : public RefCounted
	{
	public:
		VulkanIndexBuffer(uint32_t size);
		VulkanIndexBuffer(void* data, uint32_t size = 0);
		virtual ~VulkanIndexBuffer();

		virtual void SetData(void* buffer, uint32_t size, uint32_t offset = 0) ;
		virtual void Bind() const ;

		virtual uint32_t GetCount() const  { return m_Size / sizeof(uint32_t); }

		virtual uint32_t GetSize() const  { return m_Size; }
		virtual uint32_t GetRendererID() const ;

		VkBuffer GetVulkanBuffer() { return m_VulkanBuffer; }
	private:
		uint32_t m_Size = 0;
		Buffer m_LocalData;

		VkBuffer m_VulkanBuffer = nullptr;
		VmaAllocation m_MemoryAllocation;

	};

}