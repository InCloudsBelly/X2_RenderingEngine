#include "RenderPassManager.h"
//#include "Utils/Log.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include <map>
#include <string>
#include <iostream>
#include "Core/Graphic/Instance/Image.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"

#include <stdexcept>

RenderPassManager::RenderPassManager()
    : m_renderPassWrappers()
    //, _managerMutex()
{
}

RenderPassManager::~RenderPassManager()
{
    for (auto iterator = m_renderPassWrappers.begin(); iterator != m_renderPassWrappers.end(); iterator ++)
    {
        if (iterator->second.refrenceCount != 0)
            std::cout << "Warn! " << iterator->first << " hasn't been delete!  RefrenceCount is " << iterator->second.refrenceCount << std::endl;
        delete iterator->second.renderPass;
    }
}

void RenderPassManager::createRenderPass(RenderPassBase* renderPass)
{
    renderPass->populateRenderPassSettings(renderPass->m_settings);

    std::map<std::string, uint32_t> attachmentIndexes;
    std::vector<VkAttachmentDescription> attachments = std::vector<VkAttachmentDescription>(renderPass->m_settings.attchmentDescriptors.size());
    std::vector<VkAttachmentReference> attachmentReferences = std::vector<VkAttachmentReference>(renderPass->m_settings.attchmentDescriptors.size());
    {
        uint32_t attachmentIndex = 0;
        for (const auto& pair : renderPass->m_settings.attchmentDescriptors)
        {
            const auto& attachmentDescriptor = pair.second;

            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = attachmentDescriptor.format;
            colorAttachment.samples = static_cast<VkSampleCountFlagBits>(attachmentDescriptor.sampleCount);
            colorAttachment.loadOp = attachmentDescriptor.loadOp;
            colorAttachment.storeOp = attachmentDescriptor.storeOp;
            colorAttachment.stencilLoadOp = attachmentDescriptor.stencilLoadOp;
            colorAttachment.stencilStoreOp = attachmentDescriptor.stencilStoreOp;
            colorAttachment.initialLayout = attachmentDescriptor.initialLayout;
            colorAttachment.finalLayout = attachmentDescriptor.finalLayout;

            VkAttachmentReference attachmentReference{};
            attachmentReference.attachment = attachmentIndex;
            attachmentReference.layout = attachmentDescriptor.layout;

            attachments[attachmentIndex] = colorAttachment;
            attachmentReferences[attachmentIndex] = attachmentReference;
            attachmentIndexes[attachmentDescriptor.name] = attachmentIndex;

            ++attachmentIndex;
        }
    }

    std::map<std::string, uint32_t> subpassMap;
    std::map<std::string, std::map<std::string, uint32_t>> colorAttachmentMap;
    std::vector<VkSubpassDescription> subpasss = std::vector<VkSubpassDescription>(renderPass->m_settings.subpassDescriptors.size());
    std::vector<std::vector<VkAttachmentReference>> colorAttachments = std::vector<std::vector<VkAttachmentReference>>(renderPass->m_settings.subpassDescriptors.size());
    std::vector<VkAttachmentReference> depthStencilAttachments = std::vector<VkAttachmentReference>(renderPass->m_settings.subpassDescriptors.size());
    {
        uint32_t subpassIndex = 0;
        for (const auto& pair : renderPass->m_settings.subpassDescriptors)
        {
            const auto& subpassDescriptor = pair.second;

            subpassMap[subpassDescriptor.name] = subpassIndex;
            colorAttachmentMap[subpassDescriptor.name] = std::map<std::string, uint32_t>();

            colorAttachments[subpassIndex].resize(subpassDescriptor.colorAttachmentNames.size());
            for (uint32_t i = 0; i < subpassDescriptor.colorAttachmentNames.size(); i++)
            {
                colorAttachments[subpassIndex][i] = attachmentReferences[attachmentIndexes[subpassDescriptor.colorAttachmentNames[i]]];
                colorAttachmentMap[subpassDescriptor.name][subpassDescriptor.colorAttachmentNames[i]] = i;
            }

            if (subpassDescriptor.useDepthStencilAttachment)
            {
                depthStencilAttachments[subpassIndex] = attachmentReferences[attachmentIndexes[subpassDescriptor.depthStencilAttachmentName]];
            }


            ++subpassIndex;
        }

        for (const auto& pair : renderPass->m_settings.subpassDescriptors)
        {
            const auto& subpassDescriptor = pair.second;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = subpassDescriptor.pipelineBindPoint;
            subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments[subpassMap[subpassDescriptor.name]].size());
            subpass.pColorAttachments = colorAttachments[subpassMap[subpassDescriptor.name]].data();
            if (subpassDescriptor.useDepthStencilAttachment)
            {
                subpass.pDepthStencilAttachment = &(depthStencilAttachments[subpassMap[subpassDescriptor.name]]);
            }

            subpasss[subpassMap[subpassDescriptor.name]] = subpass;
        }
    }

    std::vector< VkSubpassDependency> dependencys = std::vector< VkSubpassDependency>(renderPass->m_settings.dependencyDescriptors.size());
    uint32_t dependencyIndex = 0;
    for (const auto& dependencyDescriptor : renderPass->m_settings.dependencyDescriptors)
    {
        VkSubpassDependency dependency{};
        dependency.srcSubpass = dependencyDescriptor.srcSubpassName == "VK_SUBPASS_EXTERNAL" ? VK_SUBPASS_EXTERNAL : subpassMap[dependencyDescriptor.srcSubpassName];
        dependency.dstSubpass = dependencyDescriptor.dstSubpassName == "VK_SUBPASS_EXTERNAL" ? VK_SUBPASS_EXTERNAL : subpassMap[dependencyDescriptor.dstSubpassName];
        dependency.srcStageMask = dependencyDescriptor.srcStageMask;
        dependency.srcAccessMask = dependencyDescriptor.srcAccessMask;
        dependency.dstStageMask = dependencyDescriptor.dstStageMask;
        dependency.dstAccessMask = dependencyDescriptor.dstAccessMask;
        dependency.dependencyFlags = dependency.srcSubpass == dependency.dstSubpass ? VK_DEPENDENCY_BY_REGION_BIT : 0;

        dependencys[dependencyIndex] = dependency;

        ++dependencyIndex;
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasss.size());
    renderPassInfo.pSubpasses = subpasss.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencys.size());
    renderPassInfo.pDependencies = dependencys.data();

    VkRenderPass newVkRenderPass{};

    if (vkCreateRenderPass(Instance::getDevice(), &renderPassInfo, nullptr, &newVkRenderPass) != VK_SUCCESS)
        throw(std::runtime_error("Failed to create render pass"));
    
    renderPass->m_renderPass = newVkRenderPass;
    renderPass->m_subPassIndexMap = subpassMap;
    renderPass->m_colorAttachmentIndexMaps = colorAttachmentMap;
}

RenderPassBase* RenderPassManager::loadRenderPass(std::string renderPassName)
{
    //std::lock_guard<std::mutex> locker(_managerMutex);

    auto iterator = m_renderPassWrappers.find(renderPassName);
    if (iterator == std::end(m_renderPassWrappers))
    {
        rttr::type renderPassType = rttr::type::get_by_name(renderPassName);
        if (!renderPassType.is_valid())
            throw(std::runtime_error("Do not have render pass type named: " + renderPassName + "."));

        rttr::variant renderPassVariant = renderPassType.create();
        if (!renderPassVariant.is_valid())
            throw(std::runtime_error("Can not create render pass variant named: " + renderPassName + "."));

        RenderPassBase* renderPass = renderPassVariant.get_value<RenderPassBase*>();
        if (renderPass == nullptr)
            throw(std::runtime_error("Can not cast render pass named: " + renderPassName + "."));

        createRenderPass(renderPass);

        m_renderPassWrappers.emplace(renderPassName, RenderPassWrapper{ 0, renderPass });
        iterator = m_renderPassWrappers.find(renderPassName);
    }
    iterator->second.refrenceCount++;
    return iterator->second.renderPass;
}

void RenderPassManager::unloadRenderPass(std::string renderPassName)
{
    //std::lock_guard<std::mutex> locker(_managerMutex);

    auto iterator = m_renderPassWrappers.find(renderPassName);
    if(iterator == std::end(m_renderPassWrappers))
        throw(std::runtime_error("Do not have render pass instance named: " + renderPassName + "."));

    iterator->second.refrenceCount--;
}

void RenderPassManager::unloadRenderPass(RenderPassBase* renderPass)
{
    //std::lock_guard<std::mutex> locker(_managerMutex);

    std::string renderPassName = renderPass->type().get_name().to_string();

    auto iterator = m_renderPassWrappers.find(renderPassName);
    if (iterator == std::end(m_renderPassWrappers))
        throw(std::runtime_error("Do not have render pass instance named: " + renderPassName + "."));
    iterator->second.refrenceCount--;
}

void RenderPassManager::collect()
{
    //std::lock_guard<std::mutex> locker(_managerMutex);

    for (auto iterator = m_renderPassWrappers.begin(); iterator != m_renderPassWrappers.end(); )
    {
        if (iterator->second.refrenceCount == 0)
        {
            delete iterator->second.renderPass;
            iterator = m_renderPassWrappers.erase(iterator);
        }
        else
        {
            iterator++;
        }
    }
}
