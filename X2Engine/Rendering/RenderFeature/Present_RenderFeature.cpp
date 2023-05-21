#include "Present_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Core/Graphic/Command/Semaphore.h"

#include "Asset/Mesh.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Utils/OrientedBoundingBox.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Manager/LightManager.h"
//#include "Core/IO/CoreObject/Instance.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Graphic/Rendering/RendererBase.h"
#include "Core/Graphic/CoreObject/Swapchain.h"

#include "Core/Graphic/Rendering/Shader.h"

#define MAX_FRAMES_IN_FLIGHT 2

int Present_RenderFeature::currentFrame = 0;
int Present_RenderFeature::swapchainFrameIndex = 0;

RTTR_REGISTRATION
{
	rttr::registration::class_<Present_RenderFeature::Present_RenderPass>("Present_RenderFeature::Present_RenderPass").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<Present_RenderFeature::Present_RenderFeatureData>("Present_RenderFeature::Present_RenderFeatureData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<Present_RenderFeature>("Present_RenderFeature").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

Present_RenderFeature::Present_RenderPass::Present_RenderPass()
	: RenderPassBase()
{

}

Present_RenderFeature::Present_RenderPass::~Present_RenderPass()
{

}

void Present_RenderFeature::Present_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
{
	settings.addColorAttachment(
		"SwapchainAttachment",
		VK_FORMAT_B8G8R8A8_UNORM,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	);
	settings.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "SwapchainAttachment" }
	);
	settings.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0,
		0
	);
}

Present_RenderFeature::Present_RenderFeatureData::Present_RenderFeatureData()
	: RenderFeatureDataBase()
	, frameBuffers{}
	, attachmentSizeInfoBuffer(nullptr)
{

}

Present_RenderFeature::Present_RenderFeatureData::~Present_RenderFeatureData()
{

}


Present_RenderFeature::Present_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<Present_RenderPass>())
	, m_renderPassName(rttr::type::get<Present_RenderPass>().get_name().to_string())
{

}

Present_RenderFeature::~Present_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<Present_RenderPass>();
}

RenderFeatureDataBase* Present_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new Present_RenderFeatureData();

	auto extent = Instance::getSwapchain()->getExtent();
	AttachmentSizeInfo attachmentSizeInfo = { {extent.width, extent.height}, {1.0 / extent.width, 1.0 / extent.height} };
	featureData->attachmentSizeInfoBuffer = new Buffer(sizeof(AttachmentSizeInfo),VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VMA_MEMORY_USAGE_CPU_ONLY);
	featureData->attachmentSizeInfoBuffer->WriteData(&attachmentSizeInfo, sizeof(AttachmentSizeInfo));


	featureData->m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	featureData->m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		featureData->m_imageAvailableSemaphores[i] = new Semaphore();
		featureData->m_renderFinishedSemaphores[i] = new Semaphore();
	}

	
	//const uint32_t curSwapchaineImageIndex = Instance::getSwapchain()->getNextImageIndex(m_imageAvailableSemaphores[currentFrame]->getSemphore());

	featureData->frameBuffers.resize(Instance::getSwapchain()->getImageCount());
	for (uint32_t i = 0; i < featureData->frameBuffers.size(); ++i)
	{
		VkImageView* swapchainImageView = Instance::getSwapchain()->getImageView(i);
		featureData->frameBuffers[i] = new FrameBuffer(m_renderPass, {{"SwapchainAttachment", swapchainImageView}}, extent);
	}
	
	
	featureData->m_sampler = new ImageSampler(
		VkFilter::VK_FILTER_LINEAR,
		VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f,
		VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
	);

	return featureData;
}

void Present_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
}

void Present_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<Present_RenderFeatureData*>(renderFeatureData);
	for (auto ptr : featureData->frameBuffers)
		delete ptr;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		delete  featureData->m_imageAvailableSemaphores[i];
		delete  featureData->m_renderFinishedSemaphores[i];
	}

	delete featureData->attachmentSizeInfoBuffer;
	delete featureData->m_sampler;


}

void Present_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void Present_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<Present_RenderFeatureData*>(renderFeatureData);

	swapchainFrameIndex = Instance::getSwapchain()->getNextImageIndex(featureData->m_imageAvailableSemaphores[currentFrame]->getSemphore());

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


	///Render
	{
		auto colorAttachmentBarrier = ImageMemoryBarrier
		(
			CameraBase::mainCamera->attachments["ColorAttachment"],
			VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
		);
		commandBuffer->addPipelineImageBarrier(VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { &colorAttachmentBarrier });
		VkClearValue cv = { 0, 0, 0, 255 };

		commandBuffer->beginRenderPass(m_renderPass, featureData->frameBuffers[swapchainFrameIndex], { {"SwapchainAttachment", cv} });
		
		for (const auto& rendererComponent : *rendererComponents)
		{
			auto material = rendererComponent->getMaterial(m_renderPassName);
			if (material == nullptr) continue;

			material->setUniformBuffer("attachmentSizeInfo", featureData->attachmentSizeInfoBuffer);
			material->setSampledImage2D("colorTexture", CameraBase::mainCamera->attachments["ColorAttachment"], featureData->m_sampler);

			commandBuffer->drawMesh(rendererComponent->mesh, material);
		}
		commandBuffer->endRenderPass();
	}
	commandBuffer->endRecord();
}

void Present_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	auto featureData = static_cast<Present_RenderFeatureData*>(renderFeatureData);

	std::vector<Semaphore*> waitSemaphores = { featureData->m_imageAvailableSemaphores[currentFrame] };
	std::vector<VkPipelineStageFlags> waitStages = { (VkPipelineStageFlags)(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) };
	std::vector<Semaphore*> signalSemaphores = { featureData->m_renderFinishedSemaphores[currentFrame] };

	commandBuffer->submit(waitSemaphores, waitStages, signalSemaphores);
}

void Present_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();

	auto featureData = static_cast<Present_RenderFeatureData*>(renderFeatureData);
	Instance::getSwapchain()->presentImage(swapchainFrameIndex, { featureData->m_renderFinishedSemaphores[currentFrame] }, Instance::getQueue("PresentQueue"));
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
