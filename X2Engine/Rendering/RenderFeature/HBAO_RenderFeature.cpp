#include "Rendering/RenderFeature/HBAO_RenderFeature.h"
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
	rttr::registration::class_<HBAO_RenderFeature::HBAO_RenderPass>("HBAO_RenderFeature::HBAO_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<HBAO_RenderFeature::HBAO_RenderFeatureData>("HBAO_RenderFeature::HBAO_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<HBAO_RenderFeature>("HBAO_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

HBAO_RenderFeature::HBAO_RenderPass::HBAO_RenderPass()
	: RenderPassBase()
{

}

HBAO_RenderFeature::HBAO_RenderPass::~HBAO_RenderPass()
{

}

void HBAO_RenderFeature::HBAO_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
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
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0,
		0
	);
	settings.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
}

HBAO_RenderFeature::HBAO_RenderFeatureData::HBAO_RenderFeatureData()
	: RenderFeatureDataBase()
	, material(nullptr)
	, frameBuffer(nullptr)
	, occlusionTexture(nullptr)
	, hbaoInfoBuffer(nullptr)
	, noiseTexture(nullptr)
	, noiseStagingBuffer(nullptr)
	, sampleRadius(0.2f)
	, sampleBiasAngle(35.0f)
	, stepCount(4)
	, directionCount(8)
	, noiseTextureWidth(64)
	, depthTexture(nullptr)
{

}

HBAO_RenderFeature::HBAO_RenderFeatureData::~HBAO_RenderFeatureData()
{

}


HBAO_RenderFeature::HBAO_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<HBAO_RenderPass>())
	, m_fullScreenMesh(Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR)+"default/BackgroundMesh.ply"))
	, m_hbaoShader(Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "HBAO_Shader.shader"))
	, m_textureSampler(
		new ImageSampler(
			VkFilter::VK_FILTER_LINEAR,
			VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			0.0f,
			VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
		)
	)
{

}

HBAO_RenderFeature::~HBAO_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<HBAO_RenderPass>();
	Instance::getAssetManager()->unload(m_fullScreenMesh);
	Instance::getAssetManager()->unload(m_hbaoShader);
	delete m_textureSampler;
}

RenderFeatureDataBase* HBAO_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new HBAO_RenderFeatureData();
	VkExtent2D extent = { camera->attachments["ColorAttachment"]->getExtent2D().width, camera->attachments["ColorAttachment"]->getExtent2D().height  };

	///Occlusion texture
	{
		featureData->occlusionTexture = Image::Create2DImage(
			extent,
			VK_FORMAT_R16_SFLOAT,
			VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	///Frame buffer
	{
		featureData->frameBuffer = new FrameBuffer(m_renderPass, { {"OcclusionTexture", featureData->occlusionTexture} });
	}

	return featureData;
}

void HBAO_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
	auto featureData = static_cast<HBAO_RenderFeatureData*>(renderFeatureData);
	VkExtent2D extent = { camera->attachments["ColorAttachment"]->getExtent2D().width , camera->attachments["ColorAttachment"]->getExtent2D().height  };

	///Hbao info
	{
		featureData->hbaoInfoBuffer = new Buffer{
			sizeof(HbaoInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		};
		featureData->hbaoInfoBuffer->WriteData(
			[featureData, extent](void* ptr)->void
			{
				HbaoInfo* hbaoInfo = reinterpret_cast<HbaoInfo*>(ptr);
				hbaoInfo->attachmentSize = glm::vec2(extent.width, extent.height);
				hbaoInfo->attachmentTexelSize = glm::vec2(1, 1) / hbaoInfo->attachmentSize;
				hbaoInfo->sampleRadius = featureData->sampleRadius;
				hbaoInfo->sampleBiasAngle = featureData->sampleBiasAngle;
				hbaoInfo->stepCount = featureData->stepCount;
				hbaoInfo->directionCount = featureData->directionCount;
				hbaoInfo->noiseTextureWidth = featureData->noiseTextureWidth;
			}
		);
	}

	///Build noise staging buffer
	{
		featureData->noiseStagingBuffer = new Buffer{
				sizeof(float) * featureData->noiseTextureWidth * featureData->noiseTextureWidth,
				VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_MEMORY_USAGE_CPU_ONLY
		};
		std::vector<float> noiseInfo = std::vector<float>(featureData->noiseTextureWidth * featureData->noiseTextureWidth);
		std::default_random_engine engine;
		std::uniform_real_distribution<float> u(0.0f, 1.0f);
		for (auto& noise : noiseInfo)
		{
			noise = u(engine);
		}
		featureData->noiseStagingBuffer->WriteData(noiseInfo.data(), sizeof(float) * featureData->noiseTextureWidth * featureData->noiseTextureWidth);
	}

	///Material
	{
		featureData->material = new Material(m_hbaoShader);
		featureData->material->setUniformBuffer("hbaoInfo", featureData->hbaoInfoBuffer);
		featureData->material->setSampledImage2D("depthTexture", featureData->depthTexture, m_textureSampler);
		featureData->material->setSampledImage2D("normalTexture", featureData->normalTexture, m_textureSampler);
		featureData->material->setSampledImage2D("positionTexture", featureData->positionTexture, m_textureSampler);
	}
}

void HBAO_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<HBAO_RenderFeatureData*>(renderFeatureData);
	delete featureData->frameBuffer;
	delete featureData->occlusionTexture;
	delete featureData->hbaoInfoBuffer;
	delete featureData->noiseStagingBuffer;
	delete featureData->noiseTexture;

	delete featureData;
}

void HBAO_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void HBAO_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<HBAO_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	///Copy noise to noise texture
	if (featureData->noiseTexture == nullptr)
	{
		featureData->noiseTexture = Image::Create2DImage(
			{ static_cast<uint32_t>(featureData->noiseTextureWidth), static_cast<uint32_t>(featureData->noiseTextureWidth) },
			VK_FORMAT_R32_SFLOAT,
			VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
		);
		featureData->material->setSampledImage2D("noiseTexture", featureData->noiseTexture, m_textureSampler);

		{
			auto noiseTextureBarrier = ImageMemoryBarrier
			(
				featureData->noiseTexture,
				VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
				VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				0,
				VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT
			);
			commandBuffer->addPipelineImageBarrier(
				VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
				{ &noiseTextureBarrier }
			);
		}

		commandBuffer->copyBufferToImage(featureData->noiseStagingBuffer, featureData->noiseTexture, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		{
			auto noiseTextureBarrier = ImageMemoryBarrier
			(
				featureData->noiseTexture,
				VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
			);
			commandBuffer->addPipelineImageBarrier(
				VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				{ &noiseTextureBarrier }
			);
		}
	}
	else
	{
		delete featureData->noiseStagingBuffer;
		featureData->noiseStagingBuffer = nullptr;
	}

	///Render
	{
		commandBuffer->beginRenderPass(m_renderPass, featureData->frameBuffer);

		featureData->material->setUniformBuffer("cameraInfo", camera->getCameraInfoBuffer());
		commandBuffer->drawMesh(m_fullScreenMesh, featureData->material);

		commandBuffer->endRenderPass();
	}

	commandBuffer->endRecord();
}

void HBAO_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void HBAO_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
