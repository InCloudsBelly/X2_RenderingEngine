#pragma once
#include <string>
#include <vulkan/vulkan_core.h>
#include <map>
#include <vector>
#include "Core/Logic/Object/Object.h"

class Renderer;

class CameraBase;

class Instance;

class Image;
class FrameBuffer;

class CommandPool;
class Semaphore;

class RenderPassManager;

class RenderPassBase : public Object
{
	friend class RenderPassManager;
	friend class Instance;
	//friend class CoreObject::Thread;
public:
	class RenderPassSettings final
	{
	public:
		struct AttachmentDescriptor
		{
			std::string name;
			VkFormat format;
			VkSampleCountFlags sampleCount;
			VkAttachmentLoadOp loadOp;
			VkAttachmentStoreOp storeOp;
			VkAttachmentLoadOp stencilLoadOp;
			VkAttachmentStoreOp stencilStoreOp;
			VkImageLayout initialLayout;
			VkImageLayout layout;
			VkImageLayout finalLayout;
			bool useStencil;
			AttachmentDescriptor(std::string name, VkFormat format, VkSampleCountFlags sampleCount, VkImageLayout layout, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout, bool isStencil, VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp);
		};
		struct SubpassDescriptor
		{
			std::string name;
			VkPipelineBindPoint pipelineBindPoint;
			std::vector<std::string> colorAttachmentNames;
			std::string depthStencilAttachmentName;
			bool useDepthStencilAttachment;

			SubpassDescriptor(std::string name, VkPipelineBindPoint pipelineBindPoint, std::vector<std::string> colorAttachmentNames, bool useDepthStencilAttachment, std::string depthStencilAttachmentName);
		};
		struct DependencyDescriptor
		{
			std::string srcSubpassName;
			std::string dstSubpassName;
			VkPipelineStageFlags srcStageMask;
			VkPipelineStageFlags dstStageMask;
			VkAccessFlags srcAccessMask;
			VkAccessFlags dstAccessMask;
			DependencyDescriptor(std::string srcSubpassName, std::string dstSubpassName, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
		};
		std::map <std::string, AttachmentDescriptor> attchmentDescriptors;
		std::map <std::string, SubpassDescriptor> subpassDescriptors;
		std::vector <DependencyDescriptor> dependencyDescriptors;
		RenderPassSettings();
		void addColorAttachment(std::string name, VkFormat format, VkSampleCountFlags sampleCount, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout);
		void addDepthAttachment(std::string name, VkFormat format, VkSampleCountFlags sampleCount, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout);
		void addSubpass(std::string name, VkPipelineBindPoint pipelineBindPoint, std::vector<std::string> colorAttachmentNames);
		void addSubpass(std::string name, VkPipelineBindPoint pipelineBindPoint, std::vector<std::string> colorAttachmentNames, std::string depthAttachmentName);
		void addDependency(std::string srcSubpassName, std::string dstSubpassName, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
	};
private:
	VkRenderPass m_renderPass;
	RenderPassSettings m_settings;
	std::map<std::string, uint32_t> m_subPassIndexMap;
	std::map<std::string, std::map<std::string, uint32_t>> m_colorAttachmentIndexMaps;
	std::map<std::string, std::map<std::string, uint32_t>> m_depthAttachmentMap;
protected:
	RenderPassBase();
	virtual ~RenderPassBase();
	RenderPassBase(const RenderPassBase&) = delete;
	RenderPassBase& operator=(const RenderPassBase&) = delete;
	RenderPassBase(RenderPassBase&&) = delete;
	RenderPassBase& operator=(RenderPassBase&&) = delete;

	virtual void destroy();
	virtual void populateRenderPassSettings(RenderPassSettings& creator) = 0;
public:
	const RenderPassSettings* getSettings();
	VkRenderPass getRenderPass();
	uint32_t getSubPassIndex(std::string subPassName);
	const std::map<std::string, uint32_t>* getColorAttachmentIndexMap(std::string subPassName);

	RTTR_ENABLE(Object)
};