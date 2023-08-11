#include "Precompiled.h"
#include "VulkanStorageBuffer.h"

#include "VulkanContext.h"

#include "X2/Renderer/Renderer.h"

namespace X2 {

	VulkanStorageBuffer::VulkanStorageBuffer(uint32_t size, uint32_t binding)
		: m_Size(size), m_Binding(binding)
	{
		Ref<VulkanStorageBuffer> instance = this;
		Renderer::Submit([instance]() mutable
			{
				instance->RT_Invalidate();
			});
	}

	VulkanStorageBuffer::~VulkanStorageBuffer()
	{
		Release();
	}

	void VulkanStorageBuffer::Release()
	{
		if (!m_MemoryAlloc)
			return;

		Renderer::SubmitResourceFree([buffer = m_Buffer, memoryAlloc = m_MemoryAlloc]()
			{
				VulkanAllocator allocator("StorageBuffer");
				allocator.DestroyBuffer(buffer, memoryAlloc);
			});

		m_Buffer = nullptr;
		m_MemoryAlloc = nullptr;
	}

	void VulkanStorageBuffer::RT_Invalidate()
	{
		Release();

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferInfo.size = m_Size;

		VulkanAllocator allocator("StorageBuffer");
		m_MemoryAlloc = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_Buffer);

		m_DescriptorInfo.buffer = m_Buffer;
		m_DescriptorInfo.offset = 0;
		m_DescriptorInfo.range = m_Size;
	}

	void VulkanStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		memcpy(m_LocalStorage, data, size);
		Ref<VulkanStorageBuffer> instance = this;
		Renderer::Submit([instance, size, offset]() mutable
			{
				instance->RT_SetData(instance->m_LocalStorage, size, offset);
			});
	}

	void VulkanStorageBuffer::RT_SetData(const void* data, uint32_t size, uint32_t offset)
	{
		VulkanAllocator allocator("Staging");

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.size = size;

		VkBuffer stagingBuffer;
		auto stagingAlloc = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

		uint8_t* pData = allocator.MapMemory<uint8_t>(stagingAlloc);
		memcpy(pData, data, size);
		allocator.UnmapMemory(stagingAlloc);

		{
			VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true);

			VkBufferCopy copyRegion = {
				0,
				offset,
				size
			};
			vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
		}

		allocator.DestroyBuffer(stagingBuffer, stagingAlloc);
	}

	void VulkanStorageBuffer::Resize(uint32_t newSize)
	{
		m_Size = newSize;
		Ref<VulkanStorageBuffer> instance = this;
		Renderer::Submit([instance]() mutable
			{
				instance->RT_Invalidate();
			});
	}
}
