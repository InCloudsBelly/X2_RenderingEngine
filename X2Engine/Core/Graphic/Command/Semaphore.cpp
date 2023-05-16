#include "Semaphore.h"
//#include "Utils/Log.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include <stdexcept>

VkSemaphore& Semaphore::getSemphore()
{
	return m_semaphore;
}

Semaphore::Semaphore()
	: m_semaphore(VK_NULL_HANDLE)
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(Instance::getDevice(), &semaphoreInfo, nullptr, &m_semaphore) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create semaphore."));
}

Semaphore::~Semaphore()
{
	vkDestroySemaphore(Instance::getDevice(), m_semaphore, nullptr);
	m_semaphore = VK_NULL_HANDLE;
}
