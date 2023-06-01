#include "Rendering/RenderFeature/AO_Blur_RenderFeature.h"
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
	rttr::registration::class_<AO_Blur_RenderFeature::AO_Blur_RenderPass>("AO_Blur_RenderFeature::AO_Blur_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<AO_Blur_RenderFeature::AO_Blur_RenderFeatureData>("AO_Blur_RenderFeature::AO_Blur_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<AO_Blur_RenderFeature>("AO_Blur_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

AO_Blur_RenderFeature::AO_Blur_RenderPass::AO_Blur_RenderPass()
	: RenderPassBase()
{

}

AO_Blur_RenderFeature::AO_Blur_RenderPass::~AO_Blur_RenderPass()
{

}

void AO_Blur_RenderFeature::AO_Blur_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
{
	settings.addColorAttachment(
		"OcclusionTexture",
		VK_FORMAT_R16_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	settings.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "OcclusionTexture" }
	);
	settings.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	);
	settings.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
}

AO_Blur_RenderFeature::AO_Blur_RenderFeatureData::AO_Blur_RenderFeatureData()
	: RenderFeatureDataBase()
	, horizontalMaterial(nullptr)
	, verticalMaterial(nullptr)
	, horizontalFrameBuffer(nullptr)
	, verticalFrameBuffer(nullptr)
	, horizontalBlurInfoBuffer(nullptr)
	, verticalBlurInfoBuffer(nullptr)
	, temporaryOcclusionTexture(nullptr)
	, occlusionTexture(nullptr)
	, normalTexture(nullptr)
	, sampleOffsetFactor(1.5f)
	, iterateCount(1)
{

}

AO_Blur_RenderFeature::AO_Blur_RenderFeatureData::~AO_Blur_RenderFeatureData()
{

}


AO_Blur_RenderFeature::AO_Blur_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<AO_Blur_RenderPass>())
	, m_fullScreenMesh(Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR)+"default/BackgroundMesh.ply"))
	, m_blurShader(Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR)+ "AO_Blur_Shader.shader"))
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

AO_Blur_RenderFeature::~AO_Blur_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<AO_Blur_RenderPass>();
	Instance::getAssetManager()->unload(m_fullScreenMesh);
	Instance::getAssetManager()->unload(m_blurShader);
	delete m_textureSampler;
}

RenderFeatureDataBase* AO_Blur_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new AO_Blur_RenderFeatureData();

	featureData->horizontalMaterial = new Material(m_blurShader);
	featureData->verticalMaterial = new Material(m_blurShader);

	return featureData;
}

void AO_Blur_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
	auto featureData = static_cast<AO_Blur_RenderFeatureData*>(renderFeatureData);

	auto occlusionTextureSize = featureData->occlusionTexture->getExtent2D();
	///Occlusion texture
	{
		featureData->temporaryOcclusionTexture = Image::Create2DImage(
			occlusionTextureSize,
			VK_FORMAT_R16_SFLOAT,
			VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	///Frame buffer
	{
		featureData->horizontalFrameBuffer = new FrameBuffer(m_renderPass, { {"OcclusionTexture", featureData->temporaryOcclusionTexture} });
		featureData->verticalFrameBuffer = new FrameBuffer(m_renderPass, { {"OcclusionTexture", featureData->occlusionTexture} });
	}

	///Blur info
	{
		featureData->horizontalBlurInfoBuffer = new Buffer{
			sizeof(BlurInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		};
		auto horizontalBlurInfo = BlurInfo{ glm::vec2(occlusionTextureSize.width, occlusionTextureSize.height) , glm::vec2(1, 1) / glm::vec2(occlusionTextureSize.width, occlusionTextureSize.height), glm::vec2(featureData->sampleOffsetFactor, 0) };
		featureData->horizontalBlurInfoBuffer->WriteData(&horizontalBlurInfo, sizeof(BlurInfo));

		featureData->verticalBlurInfoBuffer = new Buffer{
			sizeof(BlurInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		};
		auto verticalBlurInfo = BlurInfo{ glm::vec2(occlusionTextureSize.width, occlusionTextureSize.height) , glm::vec2(1, 1) / glm::vec2(occlusionTextureSize.width, occlusionTextureSize.height), glm::vec2(0, featureData->sampleOffsetFactor) };
		featureData->verticalBlurInfoBuffer->WriteData(&verticalBlurInfo, sizeof(BlurInfo));
	}

	///Material
	{
		featureData->horizontalMaterial->setUniformBuffer("blurInfo", featureData->horizontalBlurInfoBuffer);
		featureData->horizontalMaterial->setSampledImage2D("normalTexture", featureData->normalTexture, m_textureSampler);
		featureData->horizontalMaterial->setSampledImage2D("occlusionTexture", featureData->occlusionTexture, m_textureSampler);

		featureData->verticalMaterial->setUniformBuffer("blurInfo", featureData->verticalBlurInfoBuffer);
		featureData->verticalMaterial->setSampledImage2D("normalTexture", featureData->normalTexture, m_textureSampler);
		featureData->verticalMaterial->setSampledImage2D("occlusionTexture", featureData->temporaryOcclusionTexture, m_textureSampler);
	}
}

void AO_Blur_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<AO_Blur_RenderFeatureData*>(renderFeatureData);
	delete featureData->horizontalFrameBuffer;
	delete featureData->verticalFrameBuffer;
	delete featureData->horizontalBlurInfoBuffer;
	delete featureData->verticalBlurInfoBuffer;
	delete featureData->temporaryOcclusionTexture;

	delete featureData;
}

void AO_Blur_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void AO_Blur_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData,CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<AO_Blur_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (int i = 0; i < featureData->iterateCount; i++)
	{
		///Horizontal blur
		{
			commandBuffer->beginRenderPass(
				m_renderPass,
				featureData->horizontalFrameBuffer
			);

			commandBuffer->drawMesh(m_fullScreenMesh, featureData->horizontalMaterial);

			commandBuffer->endRenderPass();
		}

		///Vertical blur
		{
			commandBuffer->beginRenderPass(m_renderPass, featureData->verticalFrameBuffer);

			commandBuffer->drawMesh(m_fullScreenMesh, featureData->verticalMaterial);

			commandBuffer->endRenderPass();
		}
	}

	commandBuffer->endRecord();
}

void AO_Blur_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData,CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void AO_Blur_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData,CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
