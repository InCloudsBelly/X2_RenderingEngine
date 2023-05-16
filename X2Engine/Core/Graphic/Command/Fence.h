#pragma once
#include <vulkan/vulkan_core.h>


class Fence
{
private:
	VkFence m_fence;
	Fence(const Fence&) = delete;
	Fence& operator=(const Fence&) = delete;
	Fence(Fence&&) = delete;
	Fence& operator=(Fence&&) = delete;
public:
	Fence();
	Fence(VkFenceCreateFlags flag);
	~Fence();
	VkFence& getFence();
	void reset();
	void wait();
};