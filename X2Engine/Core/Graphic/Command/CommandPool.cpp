#include "CommandPool.h"
#include "CommandBuffer.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
//#include "Utils/Log.h"

CommandPool::CommandPool(std::string queueName, VkCommandPoolCreateFlags flag)
    : m_commandBuffers()
    , m_queueName(queueName)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = flag;
    poolInfo.queueFamilyIndex = Instance::getQueue(std::string(queueName))->queueFamilyIndex;


    if (vkCreateCommandPool(Instance::getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        throw(std::runtime_error("Failed to create command pool."));
}

CommandPool::~CommandPool()
{
    for (auto& commandBuffer : m_commandBuffers)
    {
        delete commandBuffer;
    }
    vkDestroyCommandPool(Instance::getDevice(), m_commandPool, nullptr);
}

VkCommandPool& CommandPool::getCommandPool()
{
    return m_commandPool;
}

CommandBuffer* CommandPool::createCommandBuffer(VkCommandBufferLevel level)
{
    auto p = new CommandBuffer("TemporaryCommandBuffer", this, level);
    m_commandBuffers.emplace(p);
    return p;
}

void CommandPool::destoryCommandBuffer(CommandBuffer* commandBuffer)
{
    m_commandBuffers.erase(commandBuffer);
    delete commandBuffer;
}

void CommandPool::reset()
{
    for (const auto& commandBuffer : m_commandBuffers)
    {
        delete commandBuffer;
    }
    m_commandBuffers.clear();
    vkResetCommandPool(Instance::getDevice(), m_commandPool, VkCommandPoolResetFlagBits::VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}
