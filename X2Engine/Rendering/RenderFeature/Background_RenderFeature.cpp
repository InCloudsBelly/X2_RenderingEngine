#include "Background_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Asset/Mesh.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Utils/OrientedBoundingBox.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Manager/LightManager.h"
//#include "Core/IO/CoreObject/Instance.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<Background_RenderFeature::Background_RenderPass>("Background_RenderFeature::Background_RenderPass").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<Background_RenderFeature::Background_RenderFeatureData>("Background_RenderFeature::Background_RenderFeatureData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<Background_RenderFeature>("Background_RenderFeature").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

Background_RenderFeature::Background_RenderPass::Background_RenderPass()
	: RenderPassBase()
{

}

Background_RenderFeature::Background_RenderPass::~Background_RenderPass()
{

}

void Background_RenderFeature::Background_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
{
	settings.addColorAttachment(
		"ColorAttachment",
		VK_FORMAT_R8G8B8A8_SRGB,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);
	settings.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "ColorAttachment" }
	);
	settings.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
	);
	settings.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
	);
}

Background_RenderFeature::Background_RenderFeatureData::Background_RenderFeatureData()
	: RenderFeatureDataBase()
	, frameBuffer(nullptr)
	, attachmentSizeInfoBuffer(nullptr)
	, needClearColorAttachment(false)
{

}

Background_RenderFeature::Background_RenderFeatureData::~Background_RenderFeatureData()
{

}


Background_RenderFeature::Background_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<Background_RenderPass>())
	, m_renderPassName(rttr::type::get<Background_RenderPass>().get_name().to_string())
{

}

Background_RenderFeature::~Background_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<Background_RenderPass>();
}

RenderFeatureDataBase* Background_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto extent = camera->attachments["ColorAttachment"]->getExtent2D();
	AttachmentSizeInfo attachmentSizeInfo = { {extent.width, extent.height}, {1.0 / extent.width, 1.0 / extent.height} };

	auto featureData = new Background_RenderFeatureData();
	featureData->frameBuffer = new FrameBuffer(m_renderPass, camera->attachments);
	featureData->attachmentSizeInfoBuffer = new Buffer(
		sizeof(AttachmentSizeInfo),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	featureData->attachmentSizeInfoBuffer->WriteData(&attachmentSizeInfo, sizeof(AttachmentSizeInfo));
	featureData->needClearColorAttachment = false;

	return featureData;
}

void Background_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
}

void Background_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<Background_RenderFeatureData*>(renderFeatureData);
	delete featureData->frameBuffer;
	delete featureData->attachmentSizeInfoBuffer;
}

void Background_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void Background_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<Background_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	if (featureData->needClearColorAttachment)
	{
		//Change layout
		{
			auto colorAttachmentBarrier = ImageMemoryBarrier
			(
				featureData->frameBuffer->getAttachment("ColorAttachment"),
				VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
				VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				0,
				VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT
			);
			commandBuffer->addPipelineImageBarrier(
				VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
				{ &colorAttachmentBarrier }
			);
		}

		///Clear
		VkClearColorValue clearValue = { 0, 0, 0, 255 };
		commandBuffer->clearColorImage(featureData->frameBuffer->getAttachment("ColorAttachment"), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clearValue);

		//Change layout
		{
			auto colorAttachmentBarrier = ImageMemoryBarrier
			(
				featureData->frameBuffer->getAttachment("ColorAttachment"),
				VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
				VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
			);
			commandBuffer->addPipelineImageBarrier(
				VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				{ &colorAttachmentBarrier }
			);
		}
	}

	///Render
	{
		commandBuffer->beginRenderPass(m_renderPass, featureData->frameBuffer);
		for (const auto& rendererComponent : *rendererComponents)
		{
			auto material = rendererComponent->getMaterial(m_renderPassName);
			if (material == nullptr) continue;
			material->setUniformBuffer("cameraInfo", camera->getCameraInfoBuffer());
			material->setUniformBuffer("attachmentSizeInfo", featureData->attachmentSizeInfoBuffer);

			commandBuffer->drawMesh(rendererComponent->mesh, material);
		}
		commandBuffer->endRenderPass();
	}



	commandBuffer->endRecord();
}

void Background_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void Background_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
