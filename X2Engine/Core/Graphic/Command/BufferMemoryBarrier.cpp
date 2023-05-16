#include "BufferMemoryBarrier.h"
#include "Core/Graphic/Instance/Buffer.h"

BufferMemoryBarrier::BufferMemoryBarrier(Buffer* buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags)
	: m_bufferMemoryBarrier()
{
	m_bufferMemoryBarrier.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	m_bufferMemoryBarrier.pNext = nullptr;
	m_bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	m_bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	m_bufferMemoryBarrier.srcAccessMask = srcAccessFlags;
	m_bufferMemoryBarrier.dstAccessMask = dstAccessFlags;
	m_bufferMemoryBarrier.buffer = buffer->getBuffer();
	m_bufferMemoryBarrier.offset = buffer->Offset();
	m_bufferMemoryBarrier.size = buffer->Size();
}

BufferMemoryBarrier::BufferMemoryBarrier()
	: m_bufferMemoryBarrier()
{
}

const VkBufferMemoryBarrier& BufferMemoryBarrier::getBufferMemoryBarrier()
{
	return m_bufferMemoryBarrier;
}
