#include "SimpleForward_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Core/Graphic/Command/Semaphore.h"
#include "Core/Graphic/Instance/Image.h"

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

#include "Rendering/RenderFeature/CSM_ShadowCaster_RenderFeature.h"
#include "Rendering/RenderFeature/CascadeEVSM_ShadowCaster_RenderFeature.h"

#include "Core/Graphic/Rendering/Shader.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<SimpleForward_RenderFeature::SimpleForward_RenderPass>("SimpleForward_RenderFeature::SimpleForward_RenderPass").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<SimpleForward_RenderFeature::SimpleForward_RenderFeatureData>("SimpleForward_RenderFeature::SimpleForward_RenderFeatureData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<SimpleForward_RenderFeature>("SimpleForward_RenderFeature").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

SimpleForward_RenderFeature::SimpleForward_RenderPass::SimpleForward_RenderPass()
	: RenderPassBase()
{

}

SimpleForward_RenderFeature::SimpleForward_RenderPass::~SimpleForward_RenderPass()
{

}

void SimpleForward_RenderFeature::SimpleForward_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
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
	settings.addDepthAttachment(
		"DepthAttachment",
		VK_FORMAT_D32_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);

	settings.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "ColorAttachment" },
		"DepthAttachment"
	);
	settings.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
	);
	settings.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
	);
}

SimpleForward_RenderFeature::SimpleForward_RenderFeatureData::SimpleForward_RenderFeatureData()
	: RenderFeatureDataBase()
	, frameBuffer(nullptr)
	, needClearDepthAttachment(false)
{

}

SimpleForward_RenderFeature::SimpleForward_RenderFeatureData::~SimpleForward_RenderFeatureData()
{

}


SimpleForward_RenderFeature::SimpleForward_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<SimpleForward_RenderPass>())
	, m_renderPassName(rttr::type::get<SimpleForward_RenderPass>().get_name().to_string())
{

}

SimpleForward_RenderFeature::~SimpleForward_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<SimpleForward_RenderPass>();
}

RenderFeatureDataBase* SimpleForward_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new SimpleForward_RenderFeatureData();

	featureData->frameBuffer = new FrameBuffer(m_renderPass, camera->attachments);
	featureData->needClearDepthAttachment = false;
	featureData->m_sampler = new ImageSampler(
		VkFilter::VK_FILTER_LINEAR,
		VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f,
		VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
	);

	featureData->defaultAlbedoTexture = Instance::getAssetManager()->load<Image>(std::string(MODEL_DIR) + "defaultTexture/DefaultTexture.png");
		
	return featureData;
}

void SimpleForward_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
}

void SimpleForward_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<SimpleForward_RenderFeatureData*>(renderFeatureData);
	delete featureData->frameBuffer;
	delete featureData->m_sampler;

	Instance::getAssetManager()->unload(featureData->defaultAlbedoTexture);

}

void SimpleForward_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void SimpleForward_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<SimpleForward_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	if (featureData->needClearDepthAttachment)
	{
		//Depth Clear
		{
			//Change layout
			{
				auto colorAttachmentBarrier = ImageMemoryBarrier
				(
					featureData->frameBuffer->getAttachment("DepthAttachment"),
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
			commandBuffer->clearDepthImage(featureData->frameBuffer->getAttachment("DepthAttachment"), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1.0f);

			//Change layout
			{
				auto colorAttachmentBarrier = ImageMemoryBarrier
				(
					featureData->frameBuffer->getAttachment("DepthAttachment"),
					VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
					VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
				);
				commandBuffer->addPipelineImageBarrier(
					VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
					{ &colorAttachmentBarrier }
				);
			}
		}
	}

	///Render
	{
		commandBuffer->beginRenderPass(m_renderPass, featureData->frameBuffer);
		
		auto viewMatrix = camera->getViewMatrix();

		for (const auto& rendererComponent : *rendererComponents)
		{
			auto material = rendererComponent->getMaterial(m_renderPassName);
			if (material == nullptr) continue;

			material->setUniformBuffer("cameraInfo", camera->getCameraInfoBuffer());
			material->setUniformBuffer("meshObjectInfo", rendererComponent->getObjectInfoBuffer());
			material->setUniformBuffer("lightInfos", Instance::getLightManager().getForwardLightInfosBuffer());
			Instance::getLightManager().setAmbientLightParameters(material);

			if (featureData->shadowFeatureData)
			{
				if(featureData->shadowType == ShadowType::CSM)
					static_cast<CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeatureData*>(featureData->shadowFeatureData)->setShadowReceiverMaterialParameters(material);
				else if (featureData->shadowType == ShadowType::CASCADED_EVSM)
					static_cast<CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData*>(featureData->shadowFeatureData)->setShadowReceiverMaterialParameters(material);
			}
			
			commandBuffer->drawMesh(rendererComponent->mesh, material);
		}
		commandBuffer->endRenderPass();
	}
	
	commandBuffer->endRecord();
}

void SimpleForward_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void SimpleForward_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
