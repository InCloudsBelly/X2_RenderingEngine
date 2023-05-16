#pragma once
#include <vulkan/vulkan_core.h>

class Semaphore
{
private:
	VkSemaphore m_semaphore;
	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;
	Semaphore(Semaphore&&) = delete;
	Semaphore& operator=(Semaphore&&) = delete;
public:
	VkSemaphore& getSemphore();
	Semaphore();
	~Semaphore();
};