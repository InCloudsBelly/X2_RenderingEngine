#include "Buffer.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include <stdexcept>

Buffer::Buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage)
	: m_buffer(VK_NULL_HANDLE)
	, m_size(size)
	, m_usage(usage)
	, m_bufferView(VK_NULL_HANDLE)
	, m_format()
	, m_vmaUsage(vmaUsage)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = static_cast<VkDeviceSize>(size);
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = vmaUsage; //VMA_MEMORY_USAGE_GPU_ONLY;

	if(vmaCreateBuffer(Instance::getVmaAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create buffer."));

}

Buffer::Buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, VkFormat format)
	: m_buffer(VK_NULL_HANDLE)
	, m_size(size)
	, m_usage(usage)
	, m_bufferView(VK_NULL_HANDLE)
	, m_format(format)
	, m_vmaUsage(vmaUsage)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = static_cast<VkDeviceSize>(size);
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = vmaUsage; //VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateBuffer(Instance::getVmaAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create buffer."));


	VkBufferViewCreateInfo viewInfo{};
	viewInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	viewInfo.pNext = nullptr;
	viewInfo.flags = 0;
	viewInfo.buffer = m_buffer;
	viewInfo.format = format;
	viewInfo.offset = 0;
	viewInfo.range = m_size;

	if (vkCreateBufferView(Instance::getDevice(), &viewInfo, nullptr, &m_bufferView) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create buffer view."));

}

void Buffer::WriteData(const void* data, size_t dataSize)
{
	void* transferData;
	vmaMapMemory(Instance::getVmaAllocator(), m_allocation, &transferData);
	memcpy(transferData, data, dataSize);
	vmaUnmapMemory(Instance::getVmaAllocator(), m_allocation);
}

void Buffer::WriteData(std::function<void(void*)> writeFunction)
{
	void* transferData;
	vmaMapMemory(Instance::getVmaAllocator(), m_allocation, &transferData);
	writeFunction(transferData);
	vmaUnmapMemory(Instance::getVmaAllocator(), m_allocation);
}



VkBuffer& Buffer::getBuffer()
{
	return m_buffer;
}

VkBufferView& Buffer::getBufferView()
{
	return m_bufferView;
}

VkFormat& Buffer::getViewFormat()
{
	return m_format;
}

VmaAllocation& Buffer::getAllocation()
{
	return m_allocation;
}

size_t Buffer::Size()
{
	return m_size;
}

size_t Buffer::Offset()
{
	return 0;
}

Buffer::~Buffer()
{
	vmaDestroyBuffer(Instance::getVmaAllocator(), m_buffer, m_allocation);
}

