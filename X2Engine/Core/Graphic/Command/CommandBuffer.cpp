#include "CommandBuffer.h"
#include "CommandPool.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"

#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include "Core/Graphic/Command/Semaphore.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Core/Graphic/Command/BufferMemoryBarrier.h"
#include "Asset/Mesh.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Rendering/Shader.h"

CommandBuffer::CommandBuffer(std::string name, CommandPool* parentCommandPool, VkCommandBufferLevel level)
    : m_name(name)
    , m_parentCommandPool(parentCommandPool)
    , m_commandData()
    , m_commandBuffer(VK_NULL_HANDLE)
    , m_fence(VK_NULL_HANDLE)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(Instance::getDevice(), &fenceInfo, nullptr, &m_fence) != VK_SUCCESS)
        throw(std::runtime_error("Failed to create synchronization objects for a command buffer."));


    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = parentCommandPool->m_commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(Instance::getDevice(), &allocInfo, &m_commandBuffer) != VK_SUCCESS)
        throw(std::runtime_error("Failed to allocate command buffers."));

}
CommandBuffer::~CommandBuffer()
{
    vkFreeCommandBuffers(Instance::getDevice(), m_parentCommandPool->m_commandPool, 1, &m_commandBuffer);
    vkDestroyFence(Instance::getDevice(), m_fence, nullptr);
}

void CommandBuffer::reset()
{
    vkResetFences(Instance::getDevice(), 1, &m_fence);
    vkResetCommandBuffer(m_commandBuffer, VkCommandBufferResetFlagBits::VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
}

void CommandBuffer::beginRecord(VkCommandBufferUsageFlags flag)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flag;

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
}

void CommandBuffer::addPipelineImageBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector<ImageMemoryBarrier*> imageMemoryBarriers)
{
    std::vector< VkImageMemoryBarrier> vkBarriers = std::vector< VkImageMemoryBarrier>();
    for (const auto& imageMemoryBarrier : imageMemoryBarriers)
    {
        vkBarriers.insert(vkBarriers.end(), imageMemoryBarrier->getImageMemoryBarriers().begin(), imageMemoryBarrier->getImageMemoryBarriers().end());
    }
    vkCmdPipelineBarrier(
        m_commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(vkBarriers.size()), vkBarriers.data()
    );

   
}
void CommandBuffer::addPipelineBufferBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector<BufferMemoryBarrier*> bufferMemoryBarriers)
{
    std::vector< VkBufferMemoryBarrier> vkBufferBarriers = std::vector< VkBufferMemoryBarrier>();
    for (const auto& bufferMemoryBarrier : bufferMemoryBarriers)
    {
        vkBufferBarriers.emplace_back(bufferMemoryBarrier->getBufferMemoryBarrier());
    }
    vkCmdPipelineBarrier(
        m_commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        static_cast<uint32_t>(vkBufferBarriers.size()), vkBufferBarriers.data(),
        0, nullptr
        );
}
void CommandBuffer::addPipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector<ImageMemoryBarrier*> imageMemoryBarriers, std::vector<BufferMemoryBarrier*> bufferMemoryBarriers)
{
    std::vector< VkImageMemoryBarrier> vkImageBarriers = std::vector< VkImageMemoryBarrier>();
    std::vector< VkBufferMemoryBarrier> vkBufferBarriers = std::vector< VkBufferMemoryBarrier>();
    for (const auto& imageMemoryBarrier : imageMemoryBarriers)
    {
        vkImageBarriers.insert(vkImageBarriers.end(), imageMemoryBarrier->getImageMemoryBarriers().begin(), imageMemoryBarrier->getImageMemoryBarriers().end());
    }
    for (const auto& bufferMemoryBarrier : bufferMemoryBarriers)
    {
        vkBufferBarriers.emplace_back(bufferMemoryBarrier->getBufferMemoryBarrier());
    }
    vkCmdPipelineBarrier(
        m_commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        static_cast<uint32_t>(vkBufferBarriers.size()), vkBufferBarriers.data(),
        static_cast<uint32_t>(vkImageBarriers.size()), vkImageBarriers.data()
    );
}
void CommandBuffer::copyImage(Image* srcImage, VkImageLayout srcImageLayout, Image* dstImage, VkImageLayout dstImageLayout)
{
    copyImage(srcImage, "DefaultImageView", srcImageLayout, dstImage, "DefaultImageView", dstImageLayout);
}
void CommandBuffer::copyImage(Image* srcImage, std::string srcImageViewName, VkImageLayout srcImageLayout, Image* dstImage, std::string dstImageViewName, VkImageLayout dstImageLayout)
{
    auto& srcImageView = srcImage->getRawImageView(srcImageViewName);
    auto& dstImageView = dstImage->getRawImageView(dstImageViewName);

    auto aspectFlag = srcImageView.getImageAspectFlags();
    auto srcBaseLayer = srcImageView.getBaseLayer();
    auto srcLayerCount = srcImageView.getLayerCount();
    auto srcMipmapLevel = srcImageView.getBaseMipmapLevel();
    auto srcMipmapLevelCount = srcImageView.getMipmapLevelCount();
    auto dstBaseLayer = dstImageView.getBaseLayer();
    auto dstLayerCount = dstImageView.getLayerCount();
    auto dstMipmapLevel = dstImageView.getBaseMipmapLevel();
    auto dstMipmapLevelCount = dstImageView.getMipmapLevelCount();

    std::vector<VkImageCopy> copys(srcMipmapLevelCount);
    for (int i = 0; i < srcMipmapLevelCount; i++)
    {
        auto& copy = copys[i];
        copy.srcSubresource.aspectMask = aspectFlag;
        copy.srcSubresource.baseArrayLayer = srcBaseLayer;
        copy.srcSubresource.layerCount = srcLayerCount;
        copy.srcSubresource.mipLevel = srcMipmapLevel + i;
        copy.srcOffset = { 0, 0, 0 };
        copy.dstSubresource.aspectMask = aspectFlag;
        copy.dstSubresource.baseArrayLayer = dstBaseLayer;
        copy.dstSubresource.layerCount = dstLayerCount;
        copy.dstSubresource.mipLevel = dstMipmapLevel + i;
        copy.dstOffset = { 0, 0, 0 };
        copy.extent = srcImageView.getExtent3D(srcMipmapLevel + i);
    }

    vkCmdCopyImage(m_commandBuffer, srcImage->getImage(), srcImageLayout, dstImage->getImage(), dstImageLayout, static_cast<uint32_t>(copys.size()), copys.data());
}
void CommandBuffer::copyBufferToImage(Buffer* srcBuffer, Image* dstImage, std::string&& imageViewName, VkImageLayout dstImageLayout, uint32_t mipmapLevelOffset)
{
    auto& dstImageView = dstImage->getRawImageView(imageViewName);

    auto aspectFlag = dstImageView.getImageAspectFlags();
    auto srcBaseLayer = dstImageView.getBaseLayer();
    auto srcLayerCount = dstImageView.getLayerCount();
    auto srcMipmapLevel = dstImageView.getBaseMipmapLevel();

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = aspectFlag;
    copy.imageSubresource.baseArrayLayer = srcBaseLayer;
    copy.imageSubresource.layerCount = srcLayerCount;
    copy.imageSubresource.mipLevel = srcMipmapLevel + mipmapLevelOffset;
    copy.imageOffset = { 0, 0, 0 };
    copy.imageExtent = dstImageView.getExtent3D(srcMipmapLevel + mipmapLevelOffset);

    vkCmdCopyBufferToImage(m_commandBuffer, srcBuffer->getBuffer(), dstImage->getImage(), dstImageLayout, 1, &copy);
}
void CommandBuffer::addPipelineImageBarrier(VkDependencyFlags dependencyFlag, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, std::vector<ImageMemoryBarrier*> imageMemoryBarriers)
{
    std::vector< VkImageMemoryBarrier> vkBarriers = std::vector< VkImageMemoryBarrier>();
    for (const auto& imageMemoryBarrier : imageMemoryBarriers)
    {
        vkBarriers.insert(vkBarriers.end(), imageMemoryBarrier->getImageMemoryBarriers().begin(), imageMemoryBarrier->getImageMemoryBarriers().end());
    }
    vkCmdPipelineBarrier(
        m_commandBuffer,
        srcStageMask, dstStageMask,
        dependencyFlag,
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(vkBarriers.size()), vkBarriers.data()
    );

}

void CommandBuffer::copyBufferToImage(Buffer* srcBuffer, Image* dstImage, VkImageLayout dstImageLayout, uint32_t mipmapLevelOffset)
{
    copyBufferToImage(srcBuffer, dstImage, "DefaultImageView", dstImageLayout, mipmapLevelOffset);
}

void CommandBuffer::copyImageToBuffer(Image* srcImage, VkImageLayout srcImageLayout, Buffer* dstBuffer, uint32_t mipmapLevelOffset)
{
    auto& srcImageView = srcImage->getRawImageView();

    auto aspectFlag = srcImageView.getImageAspectFlags();
    auto srcBaseLayer = srcImageView.getBaseLayer();
    auto srcLayerCount = srcImageView.getLayerCount();
    auto srcMipmapLevel = srcImageView.getBaseMipmapLevel();

    VkBufferImageCopy copy = {};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = aspectFlag;
    copy.imageSubresource.baseArrayLayer = srcBaseLayer;
    copy.imageSubresource.layerCount = srcLayerCount;
    copy.imageSubresource.mipLevel = srcMipmapLevel + mipmapLevelOffset;
    copy.imageOffset = { 0, 0, 0 };
    copy.imageExtent = srcImageView.getExtent3D(srcMipmapLevel + mipmapLevelOffset);

    vkCmdCopyImageToBuffer(m_commandBuffer, srcImage->getImage(), srcImageLayout, dstBuffer->getBuffer(), 1, &copy);
}

void CommandBuffer::fillBuffer(Buffer* dstBuffer, uint32_t data)
{
    vkCmdFillBuffer(m_commandBuffer, dstBuffer->getBuffer(), 0, dstBuffer->Size(), data);
}

void CommandBuffer::copyBuffer(Buffer* srcBuffer, Buffer* dstBuffer)
{
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcBuffer->Offset();
    copyRegion.dstOffset = dstBuffer->Offset();
    copyRegion.size = srcBuffer->Size();
    vkCmdCopyBuffer(m_commandBuffer, srcBuffer->getBuffer(), dstBuffer->getBuffer(), 1, &copyRegion);
}

void CommandBuffer::copyBuffer(Buffer* srcBuffer, VkDeviceSize srcOffset, Buffer* dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size)
{
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(m_commandBuffer, srcBuffer->getBuffer(), dstBuffer->getBuffer(), 1, &copyRegion);
}

void CommandBuffer::endRecord()
{
    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
        throw(std::runtime_error("Failed to end command buffer called: " + m_name + "."));
}

void CommandBuffer::clearDepthImage(Image* image, VkImageLayout layout, float depth)
{
    VkClearDepthStencilValue clearValues = { depth, 0 };
    vkCmdClearDepthStencilImage(m_commandBuffer, image->getImage(), layout, &clearValues, 1, &image->getRawImageView().getImageSubresourceRange());
}

void CommandBuffer::clearColorImage(Image* image, VkImageLayout layout, VkClearColorValue targetColor)
{
    vkCmdClearColorImage(m_commandBuffer, image->getImage(), layout, &targetColor, 1, &image->getRawImageView().getImageSubresourceRange());
}


void CommandBuffer::submit(std::vector<Semaphore*> waitSemaphores, std::vector<VkPipelineStageFlags> waitStages, std::vector<Semaphore*> signalSemaphores)
{
    std::vector <VkSemaphore> wait = std::vector <VkSemaphore>(waitSemaphores.size());
    for (uint32_t i = 0; i < waitSemaphores.size(); i++)
    {
        wait[i] = waitSemaphores[i]->getSemphore(); 
    }
    std::vector <VkSemaphore> signal = std::vector <VkSemaphore>(signalSemaphores.size());
    for (uint32_t i = 0; i < signalSemaphores.size(); i++)
    {
        signal[i] = signalSemaphores[i]->getSemphore();
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
    submitInfo.pWaitSemaphores = wait.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signal.size());
    submitInfo.pSignalSemaphores = signal.data();

    {
        //std::unique_lock<std::mutex> lock(CoreObject::Queue_(m_parentCommandPool->_queueName)->mutex);

        vkQueueSubmit(Instance::getQueue(m_parentCommandPool->m_queueName)->queue, 1, &submitInfo, m_fence);
    }
}

void CommandBuffer::submit(std::vector<Semaphore*> waitSemaphores, std::vector<VkPipelineStageFlags> waitStages)
{
    std::vector <VkSemaphore> wait = std::vector <VkSemaphore>(waitSemaphores.size());
    for (uint32_t i = 0; i < waitSemaphores.size(); i++)
    {
        wait[i] = waitSemaphores[i]->getSemphore();
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(wait.size());
    submitInfo.pWaitSemaphores = wait.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    {
        //std::unique_lock<std::mutex> lock(CoreObject::Queue_(m_parentCommandPool->_queueName)->mutex);

        vkQueueSubmit(Instance::getQueue(m_parentCommandPool->m_queueName)->queue, 1, &submitInfo, m_fence);
    }
}

void CommandBuffer::submit(std::vector<Semaphore*> signalSemaphores)
{
    std::vector <VkSemaphore> signal = std::vector <VkSemaphore>(signalSemaphores.size());
    for (uint32_t i = 0; i < signalSemaphores.size(); i++)
    {
        signal[i] = signalSemaphores[i]->getSemphore();
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signal.size());
    submitInfo.pSignalSemaphores = signal.data();

    {
        //std::unique_lock<std::mutex> lock(CoreObject::Queue_(m_parentCommandPool->_queueName)->mutex);

        vkQueueSubmit(Instance::getQueue(m_parentCommandPool->m_queueName)->queue, 1, &submitInfo, m_fence);
    }
}

void CommandBuffer::submit()
{
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    {
        //std::unique_lock<std::mutex> lock(CoreObject::Queue_(m_parentCommandPool->_queueName)->mutex);

        if (vkQueueSubmit(Instance::getQueue(m_parentCommandPool->m_queueName)->queue, 1, &submitInfo, m_fence))
            throw(std::runtime_error("Failed to submit command buffer called: " + m_name + "."));
    }
}

void CommandBuffer::waitForFinish()
{

    if (vkWaitForFences(Instance::getDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX))
        throw(std::runtime_error("Failed to wait command buffer called: " + m_name + "."));
}

void CommandBuffer::beginRenderPass(RenderPassBase* renderPass, FrameBuffer* frameBuffer, std::map<std::string, VkClearValue> clearValues)
{
    std::vector< VkClearValue> attachmentClearValues = std::vector< VkClearValue>(renderPass->getSettings()->attchmentDescriptors.size());
    {
        size_t i = 0;
        for (const auto& attachment : renderPass->getSettings()->attchmentDescriptors)
        {
            if (clearValues.count(attachment.first))
            {
                attachmentClearValues[i++] = clearValues[attachment.first];
            }
            else
            {
                attachmentClearValues[i++] = {};
            }
        }
    }
    
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass->getRenderPass();
    renderPassInfo.framebuffer = frameBuffer->getFramebuffer();
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = frameBuffer->getExtent2D();
    renderPassInfo.clearValueCount = static_cast<uint32_t>(attachmentClearValues.size());
    renderPassInfo.pClearValues = attachmentClearValues.data();

    m_commandData.viewport.x = 0.0f;
    m_commandData.viewport.y = 0.0f;
    m_commandData.viewport.width = static_cast<float>(frameBuffer->getExtent2D().width);
    m_commandData.viewport.height = static_cast<float>(frameBuffer->getExtent2D().height);
    m_commandData.viewport.minDepth = 0.0f;
    m_commandData.viewport.maxDepth = 1.0f;

    m_commandData.scissor.offset = { 0, 0 };
    m_commandData.scissor.extent = frameBuffer->getExtent2D();

    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(m_commandBuffer, 0, 1, &m_commandData.viewport);
    vkCmdSetScissor(m_commandBuffer, 0, 1, &m_commandData.scissor);
}

void CommandBuffer::beginRenderPass(RenderPassBase* renderPass, FrameBuffer* frameBuffer)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass->getRenderPass();
    renderPassInfo.framebuffer = frameBuffer->getFramebuffer();
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = frameBuffer->getExtent2D();
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    m_commandData.viewport.x = 0.0f;
    m_commandData.viewport.y = 0.0f;
    m_commandData.viewport.width = static_cast<float>(frameBuffer->getExtent2D().width);
    m_commandData.viewport.height = static_cast<float>(frameBuffer->getExtent2D().height);
    m_commandData.viewport.minDepth = 0.0f;
    m_commandData.viewport.maxDepth = 1.0f;

    m_commandData.scissor.offset = { 0, 0 };
    m_commandData.scissor.extent = frameBuffer->getExtent2D();

    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(m_commandBuffer, 0, 1, &m_commandData.viewport);
    vkCmdSetScissor(m_commandBuffer, 0, 1, &m_commandData.scissor);
}

void CommandBuffer::endRenderPass()
{
    vkCmdEndRenderPass(m_commandBuffer);
}

void CommandBuffer::drawMesh(Mesh* mesh, Material* material)
{
    auto sets = material->getDescriptorSets();
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material->getShader()->getPipeline());
    if (!sets.empty())
    {
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material->getPipelineLayout(), 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
    }

    VkBuffer vertexBuffers[] = { mesh->getVertexBuffer().getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(m_commandBuffer, mesh->getIndexBuffer().getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(m_commandBuffer, static_cast<uint32_t>(mesh->getIndices().size()), 1, 0, 0, 0);
}

void CommandBuffer::dispatch(Material* material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    auto sets = material->getDescriptorSets();
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, material->getShader()->getPipeline());
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, material->getPipelineLayout(), 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
    vkCmdDispatch(m_commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::blit(Image* srcImage, std::string&& srcImageViewName, VkImageLayout srcImageLayout, Image* dstImage, std::string&& dstImageViewName, VkImageLayout dstImageLayout, VkFilter filter)
{
    auto& srcImageView = srcImage->getRawImageView(srcImageViewName);
    auto& dstImageView = dstImage->getRawImageView(dstImageViewName);

    auto src = srcImage->getExtent3D();
    auto dst = dstImage->getExtent3D();

    auto aspectFlag = srcImageView.getImageAspectFlags();
    auto srcBaseLayer = srcImageView.getBaseLayer();
    auto srcLayerCount = srcImageView.getLayerCount();
    auto srcMipmapLevel = srcImageView.getBaseMipmapLevel();
    auto srcMipmapLevelCount = srcImageView.getMipmapLevelCount();
    auto dstBaseLayer = dstImageView.getBaseLayer();
    auto dstLayerCount = dstImageView.getLayerCount();
    auto dstMipmapLevel = dstImageView.getBaseMipmapLevel();
    auto dstMipmapLevelCount = dstImageView.getMipmapLevelCount();

    std::vector<VkImageBlit> blits(srcMipmapLevelCount);
    for (int i = 0; i < srcMipmapLevelCount; i++)
    {
        auto& blit = blits[i];
        auto srcExtent = srcImageView.getExtent3D(srcMipmapLevel + i);
        auto dstExtent = dstImageView.getExtent3D(dstMipmapLevel + i);
        blit.srcSubresource.aspectMask = aspectFlag;
        blit.srcSubresource.baseArrayLayer = srcBaseLayer;
        blit.srcSubresource.layerCount = srcLayerCount;
        blit.srcSubresource.mipLevel = srcMipmapLevel + i;
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { static_cast<int>(srcExtent .width), static_cast<int>(srcExtent .height), static_cast<int>(srcExtent .depth)};
        blit.dstSubresource.aspectMask = aspectFlag;
        blit.dstSubresource.baseArrayLayer = dstBaseLayer;
        blit.dstSubresource.layerCount = dstLayerCount;
        blit.dstSubresource.mipLevel = dstMipmapLevel + i;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { static_cast<int>(dstExtent.width), static_cast<int>(dstExtent.height), static_cast<int>(dstExtent.depth) };
    }

    vkCmdBlitImage(m_commandBuffer, srcImage->getImage(), srcImageLayout, dstImage->getImage(), dstImageLayout, static_cast<uint32_t>(blits.size()), blits.data(), filter);
}

void CommandBuffer::blit(Image* srcImage, VkImageLayout srcImageLayout, Image* dstImage, VkImageLayout dstImageLayout, VkFilter filter)
{
    blit(srcImage, "DefaultImageView", srcImageLayout, dstImage, "DefaultImageView", dstImageLayout, filter);
}

CommandPool* CommandBuffer::ParentCommandPool()
{
    return m_parentCommandPool;
}
