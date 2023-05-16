#pragma once
#include <vulkan/vulkan_core.h>
#include <map>
#include <string>


class RenderPassManager;
class Image;
class ImageView;

class RenderPassBase;
class FrameBuffer final
{
	friend class RenderPassManager;
private:
	VkFramebuffer m_frameBuffer;
	std::map<std::string, Image*> m_attachments;
	VkExtent2D m_extent2D;
public:
	FrameBuffer(RenderPassBase* renderPass, std::map<std::string, Image*> availableAttachments, std::map<std::string, std::string> availableAttachmentViews = {});
	FrameBuffer(RenderPassBase* renderPass, std::map<std::string, VkImageView*> imgViews, VkExtent2D extent);
	~FrameBuffer();
	FrameBuffer(const FrameBuffer&) = delete;
	FrameBuffer& operator=(const FrameBuffer&) = delete;
	FrameBuffer(FrameBuffer&&) = delete;
	FrameBuffer& operator=(FrameBuffer&&) = delete;
	VkFramebuffer& getFramebuffer();
	Image* getAttachment(std::string name);
	VkExtent2D getExtent2D();
};