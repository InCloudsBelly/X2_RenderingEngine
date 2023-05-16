#include <vulkan/vulkan_core.h>
#include <string>
#include <vector>
#include <map>
#include "Core/Graphic/Rendering/Material.h"

class Mesh;

class RenderPassBase;
class FrameBuffer;
class Material;


class CommandPool;
class Semaphore;
class ImageMemoryBarrier;
class BufferMemoryBarrier;
class CommandBuffer final
{
	friend class CommandPool;
private:
	struct CommandData 
	{
		uint32_t indexCount{};
		VkViewport viewport{};
		VkRect2D scissor{};
	};
	CommandPool* m_parentCommandPool;
	VkCommandBuffer m_commandBuffer;
	VkFence m_fence;

	CommandData m_commandData;
public:
	std::string const m_name;

private:
	CommandBuffer(std::string name, CommandPool* parentCommandPool, VkCommandBufferLevel level);
	~CommandBuffer();

	CommandBuffer(const CommandBuffer&) = delete;
	CommandBuffer& operator=(const CommandBuffer&) = delete;
	CommandBuffer(CommandBuffer&&) = delete;
	CommandBuffer& operator=(CommandBuffer&&) = delete;
public:
	void reset();
	void beginRecord(VkCommandBufferUsageFlags flag);
	template<typename TConstant>
	void pushConstant(Material* material, VkShaderStageFlags stage, TConstant&& constant);
	void addPipelineImageBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector <ImageMemoryBarrier*> imageMemoryBarriers);
	void addPipelineImageBarrier(VkDependencyFlags dependencyFlag, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector<ImageMemoryBarrier*> imageMemoryBarriers);
	void addPipelineBufferBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector <BufferMemoryBarrier*> bufferMemoryBarriers);
	void addPipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector <ImageMemoryBarrier*> imageMemoryBarriers, std::vector <BufferMemoryBarrier*> bufferMemoryBarriers);
	void copyImage(Image* srcImage, VkImageLayout srcImageLayout, Image* dstImage, VkImageLayout dstImageLayout);
	void copyImage(Image* srcImage, std::string srcImageViewName, VkImageLayout srcImageLayout, Image* dstImage, std::string dstImageViewName, VkImageLayout dstImageLayout);
	void copyBufferToImage(Buffer* srcBuffer, Image* dstImage, std::string&& imageViewName, VkImageLayout dstImageLayout, uint32_t mipmapLevelOffset = 0);
	void copyBufferToImage(Buffer* srcBuffer, Image* dstImage, VkImageLayout dstImageLayout, uint32_t mipmapLevelOffset = 0);
	void copyImageToBuffer(Image* srcImage, VkImageLayout srcImageLayout, Buffer* dstBuffer, uint32_t mipmapLevelOffset = 0);
	void fillBuffer(Buffer* dstBuffer, uint32_t data);
	void copyBuffer(Buffer* srcBuffer, Buffer* dstBuffer);
	void copyBuffer(Buffer* srcBuffer, VkDeviceSize srcOffset, Buffer* dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size);
	void endRecord();
	void clearDepthImage(Image* image, VkImageLayout layout, float depth);
	void clearColorImage(Image* image, VkImageLayout layout, VkClearColorValue targetColor);
	void submit(std::vector<Semaphore*> waitSemaphores, std::vector<VkPipelineStageFlags> waitStages, std::vector<Semaphore*> signalSemaphores, VkFence* fence);
	void submit(std::vector<Semaphore*> waitSemaphores, std::vector<VkPipelineStageFlags> waitStages, std::vector<Semaphore*> signalSemaphores);
	void submit(std::vector<Semaphore*> waitSemaphores, std::vector<VkPipelineStageFlags> waitStages);
	void submit(std::vector<Semaphore*> signalSemaphores);
	void submit();
	void waitForFinish();
	void beginRenderPass(RenderPassBase* renderPass, FrameBuffer* frameBuffer, std::map<std::string, VkClearValue> clearValues);
	void beginRenderPass(RenderPassBase* renderPass, FrameBuffer* frameBuffer);
	void endRenderPass();
	void drawMesh(Mesh* mesh, Material* material);
	void dispatch(Material* material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
	void blit(Image* srcImage, std::string&& srcImageViewName, VkImageLayout srcImageLayout, Image* dstImage, std::string&& dstImageViewName, VkImageLayout dstImageLayout, VkFilter filter);
	void blit(Image* srcImage, VkImageLayout srcImageLayout, Image* dstImage, VkImageLayout dstImageLayout, VkFilter filter);

	CommandPool* ParentCommandPool();
};

template<typename TConstant>
inline void CommandBuffer::pushConstant(Material* material, VkShaderStageFlags stage, TConstant&& constant)
{
	vkCmdPushConstants(m_commandBuffer, material->getShader()->getPipelineLayout(), stage, 0, sizeof(TConstant), &constant);
}