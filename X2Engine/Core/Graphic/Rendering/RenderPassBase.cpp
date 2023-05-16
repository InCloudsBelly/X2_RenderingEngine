#include "RenderPassBase.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
//#include "Utils/Log.h"
#include "Core/Graphic/Command/Semaphore.h"
#include "Core/Graphic/Manager/RenderPassManager.h"

RTTR_REGISTRATION
{
    rttr::registration::class_<RenderPassBase>("RenderPassBase");
}


RenderPassBase::RenderPassSettings::AttachmentDescriptor::AttachmentDescriptor(std::string name, VkFormat format, VkSampleCountFlags sampleCount, VkImageLayout layout, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout, bool isStencil, VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp)
    : name(name)
    , format(format)
    , sampleCount(sampleCount)
    , layout(layout)
    , loadOp(loadOp)
    , storeOp(storeOp)
    , stencilLoadOp(stencilLoadOp)
    , stencilStoreOp(stencilStoreOp)
    , initialLayout(initialLayout)
    , finalLayout(finalLayout)
    , useStencil(isStencil)
{
}

RenderPassBase::RenderPassSettings::SubpassDescriptor::SubpassDescriptor(std::string name, VkPipelineBindPoint pipelineBindPoint, std::vector<std::string> colorAttachmentNames, bool useDepthStencilAttachment, std::string depthStencilAttachmentName)
    : name(name)
    , pipelineBindPoint(pipelineBindPoint)
    , colorAttachmentNames(colorAttachmentNames)
    , useDepthStencilAttachment(useDepthStencilAttachment)
    , depthStencilAttachmentName(depthStencilAttachmentName)
{
}

RenderPassBase::RenderPassSettings::DependencyDescriptor::DependencyDescriptor(std::string srcSubpassName, std::string dstSubpassName, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
    : srcSubpassName(srcSubpassName)
    , dstSubpassName(dstSubpassName)
    , srcStageMask(srcStageMask)
    , dstStageMask(dstStageMask)
    , srcAccessMask(srcAccessMask)
    , dstAccessMask(dstAccessMask)
{
}

RenderPassBase::RenderPassSettings::RenderPassSettings()
    : attchmentDescriptors()
    , subpassDescriptors()
    , dependencyDescriptors()
{
}

void RenderPassBase::RenderPassSettings::addColorAttachment(std::string name, VkFormat format, VkSampleCountFlags sampleCount, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
    attchmentDescriptors.insert({ name, AttachmentDescriptor(name, format, sampleCount, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, loadOp, storeOp, initialLayout, finalLayout, false, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_NONE) });
}

void RenderPassBase::RenderPassSettings::addDepthAttachment(std::string name, VkFormat format, VkSampleCountFlags sampleCount, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
    attchmentDescriptors.insert({ name, AttachmentDescriptor(name, format, sampleCount, VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, loadOp, storeOp, initialLayout, finalLayout, false, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_NONE) });
}

void RenderPassBase::RenderPassSettings::addSubpass(std::string name, VkPipelineBindPoint pipelineBindPoint, std::vector<std::string> colorAttachmentNames)
{
    subpassDescriptors.insert({ name, SubpassDescriptor(name, pipelineBindPoint, colorAttachmentNames, false, "") });
}

void RenderPassBase::RenderPassSettings::addSubpass(std::string name, VkPipelineBindPoint pipelineBindPoint, std::vector<std::string> colorAttachmentNames, std::string depthAttachmentName)
{
    subpassDescriptors.insert({ name, SubpassDescriptor(name, pipelineBindPoint, colorAttachmentNames, true, depthAttachmentName) });
}

void RenderPassBase::RenderPassSettings::addDependency(std::string srcSubpassName, std::string dstSubpassName, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
    dependencyDescriptors.push_back(DependencyDescriptor(srcSubpassName, dstSubpassName, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask));
}

RenderPassBase::RenderPassBase()
    : m_renderPass(VK_NULL_HANDLE)
    , m_subPassIndexMap()
    , m_colorAttachmentIndexMaps()
{

}

RenderPassBase::~RenderPassBase()
{
    vkDestroyRenderPass(Instance::getDevice(), m_renderPass, nullptr);
}

void RenderPassBase::destroy()
{
}

const RenderPassBase::RenderPassSettings* RenderPassBase::getSettings()
{
    return &m_settings;
}

VkRenderPass RenderPassBase::getRenderPass()
{
    return m_renderPass;
}

uint32_t RenderPassBase::getSubPassIndex(std::string subPassName)
{
    return m_subPassIndexMap[subPassName];
}

const std::map<std::string, uint32_t>* RenderPassBase::getColorAttachmentIndexMap(std::string subPassName)
{
    return &m_colorAttachmentIndexMaps[subPassName];
}
