#include "Rendering/RenderFeature/AO_Cover_RenderFeature.h"
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
#include <map>
#include <random>
#include "Utils/RandomSphericalCoordinateGenerator.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Instance/Image.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<AO_Cover_RenderFeature::AO_Cover_RenderPass>("AO_Cover_RenderFeature::AO_Cover_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<AO_Cover_RenderFeature::AO_Cover_RenderFeatureData>("AO_Cover_RenderFeature::AO_Cover_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<AO_Cover_RenderFeature>("AO_Cover_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

AO_Cover_RenderFeature::AO_Cover_RenderPass::AO_Cover_RenderPass()
	: RenderPassBase()
{

}

AO_Cover_RenderFeature::AO_Cover_RenderPass::~AO_Cover_RenderPass()
{

}

void AO_Cover_RenderFeature::AO_Cover_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
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
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
	);
	settings.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
	);
}

AO_Cover_RenderFeature::AO_Cover_RenderFeatureData::AO_Cover_RenderFeatureData()
	: RenderFeatureDataBase()
	, material(nullptr)
	, frameBuffer(nullptr)
	, occlusionTexture(nullptr)
	, intensity(1.0f)
	, coverInfoBuffer(nullptr)
{

}

AO_Cover_RenderFeature::AO_Cover_RenderFeatureData::~AO_Cover_RenderFeatureData()
{

}


AO_Cover_RenderFeature::AO_Cover_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<AO_Cover_RenderPass>())
	, m_fullScreenMesh(Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR) + "default/BackgroundMesh.ply"))
	, m_coverShader(Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "AO_Cover_Shader.shader"))
	, m_textureSampler(
		new ImageSampler(
			VkFilter::VK_FILTER_LINEAR,
			VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			0.0f,
			VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
		)
	)
{

}

AO_Cover_RenderFeature::~AO_Cover_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<AO_Cover_RenderPass>();
	Instance::getAssetManager()->unload(m_fullScreenMesh);
	
	Instance::getAssetManager()->unload(m_coverShader);

	delete m_textureSampler;
}

RenderFeatureDataBase* AO_Cover_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new AO_Cover_RenderFeatureData();

	///Frame buffer
	{
		featureData->frameBuffer = new FrameBuffer(m_renderPass, { {"ColorAttachment", camera->attachments["ColorAttachment"]} });
	}

	return featureData;
}

void AO_Cover_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
	auto featureData = static_cast<AO_Cover_RenderFeatureData*>(renderFeatureData);

	///Cover info
	{
		auto colorAttachmentSize = featureData->frameBuffer->getAttachment("ColorAttachment")->getExtent2D();
		featureData->coverInfoBuffer = new Buffer(
			sizeof(CoverInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		);
		auto coverInfo = CoverInfo{ glm::vec2(colorAttachmentSize.width, colorAttachmentSize.height) , glm::vec2(1, 1) / glm::vec2(colorAttachmentSize.width, colorAttachmentSize.height), featureData->intensity };
		featureData->coverInfoBuffer->WriteData(&coverInfo, sizeof(CoverInfo));
	}

	///Material
	{
		featureData->material = new Material(m_coverShader);
		featureData->material->setUniformBuffer("coverInfo", featureData->coverInfoBuffer);
		featureData->material->setSampledImage2D("occlusionTexture", featureData->occlusionTexture, m_textureSampler);
	}
}

void AO_Cover_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<AO_Cover_RenderFeatureData*>(renderFeatureData);
	delete featureData->frameBuffer;
	delete featureData->coverInfoBuffer;

	delete featureData;
}

void AO_Cover_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<AO_Cover_RenderFeatureData*>(renderFeatureData);
	featureData->coverInfoBuffer->WriteData(
		[featureData](void* ptr) -> void
		{
			CoverInfo* coverInfo = reinterpret_cast<CoverInfo*>(ptr);
			coverInfo->intensity = featureData->intensity;
		}
	);
}

void AO_Cover_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<AO_Cover_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	///Render
	{		commandBuffer->beginRenderPass(m_renderPass, featureData->frameBuffer);


		commandBuffer->drawMesh(m_fullScreenMesh, featureData->material);

		commandBuffer->endRenderPass();
	}

	commandBuffer->endRecord();
}

void AO_Cover_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void AO_Cover_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
