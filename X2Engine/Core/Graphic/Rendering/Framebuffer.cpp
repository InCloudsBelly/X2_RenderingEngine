#include "FrameBuffer.h"
#include "Core/Graphic/Instance/Image.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
//#include "Utils/Log.h"
#include <stdexcept>

#include <algorithm>

FrameBuffer::FrameBuffer(RenderPassBase* renderPass, std::map<std::string,Image*> availableAttachments, std::map<std::string, std::string> availableAttachmentViews)
    : m_frameBuffer(VK_NULL_HANDLE)
    , m_attachments()
    , m_extent2D()
{
    std::vector<VkImageView> views = std::vector<VkImageView>();
    for (const auto& descriptorPair : renderPass->getSettings()->attchmentDescriptors)
    {
        const auto& descriptor = descriptorPair.second;
        auto targetIter = availableAttachments.find(descriptorPair.first);
        if (targetIter == availableAttachments.end())
            throw(std::runtime_error("The available attachment do not match the render pass."));
        
        auto targetAttachment = targetIter->second;
        if (descriptor.format != targetAttachment->getFormat())
            throw(std::runtime_error("The available attachment's format do not match the render pass."));

        auto viewIter = availableAttachmentViews.find(descriptorPair.first);
        auto& targetImageView = targetAttachment->getRawImageView(viewIter == availableAttachmentViews.end() ? "DefaultImageView" : viewIter->second);
        views.emplace_back(targetImageView.getImageView());
        m_attachments.emplace(descriptorPair.first, targetAttachment);

        auto atrgetImageViewExtent2D = targetAttachment->getExtent2D(targetImageView.getBaseMipmapLevel());
        m_extent2D.width = std::max(m_extent2D.width, atrgetImageViewExtent2D.width);
        m_extent2D.height = std::max(m_extent2D.height, atrgetImageViewExtent2D.height);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass->getRenderPass();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(views.size());
    framebufferInfo.pAttachments = views.data();
    framebufferInfo.width = m_extent2D.width;
    framebufferInfo.height = m_extent2D.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(Instance::getDevice(), &framebufferInfo, nullptr, &m_frameBuffer) != VK_SUCCESS)
        throw(std::runtime_error("Failed to create framebuffer."));
}

FrameBuffer::FrameBuffer(RenderPassBase* renderPass, std::map<std::string, VkImageView*> imgViews, VkExtent2D extent)
    : m_frameBuffer(VK_NULL_HANDLE)
    , m_attachments()
    , m_extent2D(extent)
{
    std::vector<VkImageView> views = std::vector<VkImageView>();
    for (const auto& descriptorPair : renderPass->getSettings()->attchmentDescriptors)
    {
        const auto& descriptor = descriptorPair.second;
        auto targetIter = imgViews.find(descriptorPair.first);
        if (targetIter == imgViews.end())
            throw(std::runtime_error("The available imageview do not match the render pass."));

        views.emplace_back(*imgViews[descriptorPair.first]);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass->getRenderPass();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(views.size());
    framebufferInfo.pAttachments = views.data();
    framebufferInfo.width = m_extent2D.width;
    framebufferInfo.height = m_extent2D.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(Instance::getDevice(), &framebufferInfo, nullptr, &m_frameBuffer) != VK_SUCCESS)
        throw(std::runtime_error("Failed to create framebuffer."));
}

FrameBuffer::~FrameBuffer()
{
    vkDestroyFramebuffer(Instance::getDevice(), m_frameBuffer, nullptr);
}

VkFramebuffer& FrameBuffer::getFramebuffer()
{
    return m_frameBuffer;
}

Image* FrameBuffer::getAttachment(std::string name)
{
    return m_attachments[name];
}

VkExtent2D FrameBuffer::getExtent2D()
{
    return m_extent2D;
}
