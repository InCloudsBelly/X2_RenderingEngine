#include "PrefilteredEnvironmentMap_RenderFeature.h"
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
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/IO/Manager/AssetManager.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/glm.hpp>
#include <iostream>

#define X_POSITIVE_INDEX 0
#define X_NEGATIVE_INDEX 1
#define Y_POSITIVE_INDEX 2
#define Y_NEGATIVE_INDEX 3
#define Z_POSITIVE_INDEX 4
#define Z_NEGATIVE_INDEX 5

RTTR_REGISTRATION
{
	rttr::registration::class_<PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderPass>("PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeatureData>("PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<PrefilteredEnvironmentMap_RenderFeature>("PrefilteredEnvironmentMap_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderPass::PrefilteredEnvironmentMap_RenderPass()
	: RenderPassBase()
{

}

PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderPass::~PrefilteredEnvironmentMap_RenderPass()
{

}

void PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
{
	settings.addColorAttachment(
		"ColorAttachment",
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
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
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0
	);
	settings.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0
	);
}

PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeatureData::PrefilteredEnvironmentMap_RenderFeatureData()
	: RenderFeatureDataBase()
	, m_frameBuffers()
	, m_generateMaterial(nullptr)
	, m_targetCubeImage(nullptr)
	, m_boxMesh(nullptr)
	, m_generateShader(nullptr)
	, m_environmentImageSampler()
	, resolution({ 512, 512 })
	, mipLevel(floor(log2(512)) + 1)
{

}

PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeatureData::~PrefilteredEnvironmentMap_RenderFeatureData()
{

}


PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeature()
	: RenderFeatureBase()
	, m_renderPass(Instance::getRenderPassManager().loadRenderPass<PrefilteredEnvironmentMap_RenderPass>())
{

}

PrefilteredEnvironmentMap_RenderFeature::~PrefilteredEnvironmentMap_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<PrefilteredEnvironmentMap_RenderPass>();
}

RenderFeatureDataBase* PrefilteredEnvironmentMap_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new PrefilteredEnvironmentMap_RenderFeatureData();

	featureData->m_generateShader = Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "PrefilteredEnvironmentMap_Shader.shader");
	featureData->m_boxMesh = Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR) + "default/Box.ply");
	featureData->m_targetCubeImage = Image::CreateCubeImage(
		featureData->resolution,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
		featureData->mipLevel,
		VK_SAMPLE_COUNT_1_BIT
	);

	featureData->m_targetCubeImage->m_sampler = new ImageSampler(
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		16,
		VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		featureData->mipLevel
	);


	for (int i = X_POSITIVE_INDEX; i <= Z_NEGATIVE_INDEX; i++)
	{
		featureData->m_frameBuffers[i].resize(featureData->mipLevel);

		for (int mipLevel = 0; mipLevel < featureData->mipLevel; mipLevel++)
		{
			featureData->m_targetCubeImage->AddImageView(
				"ColorAttachmentView_" + std::to_string(i) + "_MipLevel_" + std::to_string(mipLevel),
				VK_IMAGE_VIEW_TYPE_2D,
				VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
				i,
				1,
				mipLevel,
				1
			);


			featureData->m_frameBuffers[i][mipLevel] = new FrameBuffer(
				m_renderPass,
				{
					{"ColorAttachment", featureData->m_targetCubeImage}
				},
				{
					{"ColorAttachment", "ColorAttachmentView_" + std::to_string(i) + "_MipLevel_" + std::to_string(mipLevel)}
				}
				);
		}
	}

	featureData->m_generateMaterial = new Material(featureData->m_generateShader);
	featureData->m_generateMaterial->setSampledImageCube("environmentImage", Instance::g_backgroundImage, Instance::g_backgroundImage->getSampler());

	return featureData;
}

void PrefilteredEnvironmentMap_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{

}

void PrefilteredEnvironmentMap_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<PrefilteredEnvironmentMap_RenderFeatureData*>(renderFeatureData);

	for (int i = X_POSITIVE_INDEX; i <= Z_NEGATIVE_INDEX; i++)
	{
		for (int mipLevel = 0; mipLevel < featureData->m_frameBuffers[i].size(); mipLevel++)
			delete featureData->m_frameBuffers[i][mipLevel];
	}
	delete featureData->m_targetCubeImage;
	delete featureData->m_environmentImageSampler;

	//Mesh & shader
	Instance::getAssetManager()->unload(featureData->m_boxMesh);
	delete featureData->m_generateMaterial;
}

void PrefilteredEnvironmentMap_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{

}

void PrefilteredEnvironmentMap_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<PrefilteredEnvironmentMap_RenderFeatureData*>(renderFeatureData);

	struct PushBlockPrefilterEnv
	{
		glm::mat4 mvp;
		float roughness;
		int samplesCount = 32;
	};

	PushBlockPrefilterEnv pushBlock;

	std::vector<glm::mat4> matrices = {
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,   0.0f,  0.0f), glm::vec3(0.0f,  -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f,  -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  -1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,   1.0f,  0.0f), glm::vec3(0.0f,  0.0f,   1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,   0.0f,  1.0f), glm::vec3(0.0f,  -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,   0.0f, -1.0f), glm::vec3(0.0f,  -1.0f,  0.0f))
	};


	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


	//Change layout
	{
		auto colorAttachmentBarrier = ImageMemoryBarrier
		(
			featureData->m_targetCubeImage,
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
	VkClearColorValue clearValue = { 0.0, 0.0, 0.0, 1.0 };
	commandBuffer->clearColorImage(featureData->m_targetCubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clearValue);

	//Change layout
	{
		auto colorAttachmentBarrier = ImageMemoryBarrier
		(
			featureData->m_targetCubeImage,
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

	for (int i = X_POSITIVE_INDEX; i <= Z_NEGATIVE_INDEX; i++)
	{
		for (int mipLevel = 0; mipLevel < featureData->mipLevel; mipLevel++) {

			pushBlock.mvp = glm::perspective((float)(glm::pi<float>() / 2.0), 1.0f, 0.1f, 512.0f) * matrices[i];
			pushBlock.roughness = (float)mipLevel / float(featureData->mipLevel - 1);

			commandBuffer->beginRenderPass(m_renderPass, featureData->m_frameBuffers[i][mipLevel]);
			commandBuffer->pushConstant(featureData->m_generateMaterial, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushBlock);
			commandBuffer->drawMesh(featureData->m_boxMesh, featureData->m_generateMaterial);
			commandBuffer->endRenderPass();
		}
	}


	//Change layout
	{
		auto colorAttachmentBarrier = ImageMemoryBarrier
		(
			featureData->m_targetCubeImage,
			VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
		);
		commandBuffer->addPipelineImageBarrier(
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			{ &colorAttachmentBarrier }
		);
	}

	commandBuffer->endRecord();
}

void PrefilteredEnvironmentMap_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void PrefilteredEnvironmentMap_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
