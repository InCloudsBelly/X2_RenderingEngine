#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>

class Buffer;

class BufferMemoryBarrier
{
	VkBufferMemoryBarrier m_bufferMemoryBarrier;
public:
	BufferMemoryBarrier(Buffer* buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags);
	BufferMemoryBarrier();
	const VkBufferMemoryBarrier& getBufferMemoryBarrier();
};