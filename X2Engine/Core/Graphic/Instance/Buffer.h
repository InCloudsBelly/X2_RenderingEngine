#pragma once

#include <vulkan/vulkan_core.h>
#include <functional>
#include <vma/vk_mem_alloc.h>
#include "Core/Logic/Object/Object.h"

class Buffer final : public Object
{
private:
	VkBuffer m_buffer;
	VkBufferView m_bufferView;
	VmaAllocation m_allocation;

	size_t m_size;
	VkBufferUsageFlags m_usage;
	VkFormat m_format;

	VmaMemoryUsage m_vmaUsage;

	Buffer(const Buffer& source) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer&&) = delete;
	Buffer& operator=(Buffer&&) = delete;
public:
	Buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmausage);
	Buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmausage, VkFormat format);


	void WriteData(const void* data, size_t dataSize);
	void WriteData(std::function<void(void*)> writeFunction);

	VkBuffer& getBuffer();
	VkBufferView& getBufferView();
	VkFormat& getViewFormat();
	VmaAllocation& getAllocation();

	size_t Size();
	size_t Offset();

	~Buffer();
};