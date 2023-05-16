#include "Fence.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
//#include "Utils/Log.h"
#include <stdexcept>

Fence::Fence(VkFenceCreateFlags flag)
	: m_fence(VK_NULL_HANDLE)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = flag;

    if (vkCreateFence(Instance::getDevice(), &fenceInfo, nullptr, &m_fence))
        throw(std::runtime_error("Failed to create synchronization objects for a frame."));
}

Fence::Fence()
    : Fence(VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT)
{
}

Fence::~Fence()
{
    vkDestroyFence(Instance::getDevice(), m_fence, nullptr);
}

VkFence& Fence::getFence()
{
    return m_fence;
}

void Fence::reset()
{
    vkResetFences(Instance::getDevice(), 1, &m_fence);
}

void Fence::wait()
{
    vkWaitForFences(Instance::getDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}
