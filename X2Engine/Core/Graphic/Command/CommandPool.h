#pragma once
#include <vulkan/vulkan_core.h>
#include <map>
#include <string>
#include <memory>
#include <set>


class CommandBuffer;
class CommandPool final
{
	friend class CommandBuffer;
private:
	VkCommandPool  m_commandPool;
	std::set<CommandBuffer*> m_commandBuffers;
	std::string m_queueName;
public:
	CommandPool(std::string queueName, VkCommandPoolCreateFlags flag);
	~CommandPool();

	CommandPool(const CommandPool&) = delete;
	CommandPool& operator=(const CommandPool&) = delete;
	CommandPool(CommandPool&&) = delete;
	CommandPool& operator=(CommandPool&&) = delete;

	VkCommandPool& getCommandPool();

	CommandBuffer* createCommandBuffer(VkCommandBufferLevel level);
	void destoryCommandBuffer(CommandBuffer* commandBuffer);

	void reset();
};