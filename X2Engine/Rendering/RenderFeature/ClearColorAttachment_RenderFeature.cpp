#include "Rendering/RenderFeature/ClearColorAttachment_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Asset/Mesh.h"
#include "Utils/OrientedBoundingBox.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Manager/LightManager.h"
#include "Core/IO/Manager/AssetManager.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<ClearColorAttachment_RenderFeature::ClearColorAttachment_RenderFeatureData>("ClearColorAttachment_RenderFeature::ClearColorAttachment_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<ClearColorAttachment_RenderFeature>("ClearColorAttachment_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

ClearColorAttachment_RenderFeature::ClearColorAttachment_RenderFeatureData::ClearColorAttachment_RenderFeatureData()
	: RenderFeatureDataBase()
	, clearValue({ 0, 0, 0, 1 })
{

}

ClearColorAttachment_RenderFeature::ClearColorAttachment_RenderFeatureData::~ClearColorAttachment_RenderFeatureData()
{

}


ClearColorAttachment_RenderFeature::ClearColorAttachment_RenderFeature()
	: RenderFeatureBase()
{

}

ClearColorAttachment_RenderFeature::~ClearColorAttachment_RenderFeature()
{

}

RenderFeatureDataBase* ClearColorAttachment_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	return new ClearColorAttachment_RenderFeatureData();
}

void ClearColorAttachment_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
}

void ClearColorAttachment_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	delete static_cast<ClearColorAttachment_RenderFeatureData*>(renderFeatureData);
}

void ClearColorAttachment_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void ClearColorAttachment_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<ClearColorAttachment_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//Change layout
	{
		auto colorAttachmentBarrier = ImageMemoryBarrier
		(
			camera->attachments["ColorAttachment"],
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
	commandBuffer->clearColorImage(camera->attachments["ColorAttachment"], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, featureData->clearValue);

	//Change layout
	{
		auto colorAttachmentBarrier = ImageMemoryBarrier
		(
			camera->attachments["ColorAttachment"],
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

	commandBuffer->endRecord();
}

void ClearColorAttachment_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void ClearColorAttachment_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
