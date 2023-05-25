#include "Rendering/RenderFeature/CascadeEVSM_ShadowCaster_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"


#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"

#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Logic/Component/Base/LightBase.h"

#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Manager/LightManager.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Instance/Image.h"
#include <set>

#include "Utils/IntersectionChecker.h"
#include "Utils/OrientedBoundingBox.h"

#include "Asset/Mesh.h"
#include "Light/DirectionalLight.h"

#include <array>
#include <algorithm>
#include "Core/IO/Manager/AssetManager.h"
#include "Core/IO/CoreObject/IOInstance.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderPass>("CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blit_RenderPass>("CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blit_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blur_RenderPass>("CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blur_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData>("CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<CascadeEVSM_ShadowCaster_RenderFeature>("CascadeEVSM_ShadowCaster_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderPass::CascadeEVSM_ShadowCaster_RenderPass()
	: RenderPassBase()
{
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderPass::~CascadeEVSM_ShadowCaster_RenderPass()
{
}

void CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderPass::populateRenderPassSettings(RenderPassSettings& creator)
{
	creator.addDepthAttachment(
		"DepthAttachment",
		VK_FORMAT_D32_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	creator.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{},
		"DepthAttachment"
	);
	creator.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
	creator.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
}
CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blit_RenderPass::CascadeEVSM_Blit_RenderPass()
	: RenderPassBase()
{
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blit_RenderPass::~CascadeEVSM_Blit_RenderPass()
{
}

void CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blit_RenderPass::populateRenderPassSettings(RenderPassSettings& creator)
{
	creator.addColorAttachment(
		"ColorAttachment",
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	creator.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "ColorAttachment" }
	);
	creator.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
	creator.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
}
CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blur_RenderPass::CascadeEVSM_Blur_RenderPass()
	: RenderPassBase()
{
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blur_RenderPass::~CascadeEVSM_Blur_RenderPass()
{
}

void CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_Blur_RenderPass::populateRenderPassSettings(RenderPassSettings& creator)
{
	creator.addColorAttachment(
		"ShadowTexture",
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	creator.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "ShadowTexture" }
	);
	creator.addDependency(
		"VK_SUBPASS_EXTERNAL",
		"DrawSubpass",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
	creator.addDependency(
		"DrawSubpass",
		"VK_SUBPASS_EXTERNAL",
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
	);
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData::CascadeEVSM_ShadowCaster_RenderFeatureData()
	: RenderFeatureDataBase()
	, depthAttachemnts()
	, shadowCasterFrameBuffers()
	, lightCameraInfoBuffer(nullptr)
	, lightCameraInfoStagingBuffer(nullptr)
	, frustumSegmentScales()
	, lightCameraCompensationDistances()
	, shadowImageResolutions()
	, cascadeEvsmShadowReceiverInfoBuffer(nullptr)
	, blitInfoBuffers()
	, shadowTextures()
	, blitFrameBuffers()
	, blitRenderPass(nullptr)
	, blitMaterials()
	, blurInfoBuffers()
	, blurMaterials()
	, blurFrameBuffers()
	, temporaryShadowTextures()
{
	frustumSegmentScales.fill(1.0 / CASCADE_COUNT);
	lightCameraCompensationDistances.fill(20);
	shadowImageResolutions.fill(1024);
	overlapScale = 0.3;
	c1 = 40;
	c2 = 5;
	threshold = 0.0;
	iterateCount = 1;
	blurOffsets.fill(1);
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData::~CascadeEVSM_ShadowCaster_RenderFeatureData()
{
}

CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeature()
	: RenderFeatureBase()
	, m_shadowCasterRenderPass(Instance::getRenderPassManager().loadRenderPass<CascadeEVSM_ShadowCaster_RenderPass>())
	, m_blitRenderPass(Instance::getRenderPassManager().loadRenderPass<CascadeEVSM_Blit_RenderPass>())
	, m_blurRenderPass(Instance::getRenderPassManager().loadRenderPass<CascadeEVSM_Blur_RenderPass>())
	, m_shadowCasterRenderPassName(rttr::type::get<CascadeEVSM_ShadowCaster_RenderPass>().get_name().to_string())
	, m_pointSampler(
		new ImageSampler(
			VkFilter::VK_FILTER_NEAREST,
			VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			0.0f,
			VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
		)
	)
	, m_blitShader(Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "CascadeEVSM_Blit_Shader.shader"))
	, m_blurShader(Instance::getAssetManager()->load<Shader>(std::string(SHADER_DIR) + "CascadeEVSM_Blur_Shader.shader"))
	, m_fullScreenMesh(Instance::getAssetManager()->load<Mesh>(std::string(MODEL_DIR) + "default/BackgroundMesh.ply"))
{

}

CascadeEVSM_ShadowCaster_RenderFeature::~CascadeEVSM_ShadowCaster_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass(m_shadowCasterRenderPass);
	Instance::getRenderPassManager().unloadRenderPass(m_blitRenderPass);
	Instance::getRenderPassManager().unloadRenderPass(m_blurRenderPass);

	Instance::getAssetManager()->unload(m_fullScreenMesh);

	delete m_pointSampler;
}

void CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData::refresh()
{
	for (auto i = 0; i < CASCADE_COUNT; i++)
	{
		VkExtent2D extent = { shadowImageResolutions[i], shadowImageResolutions[i] };
		delete depthAttachemnts[i];
		delete shadowCasterFrameBuffers[i];
		depthAttachemnts[i] = Image::Create2DImage(
			extent,
			VkFormat::VK_FORMAT_D32_SFLOAT,
			VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
		);
		shadowCasterFrameBuffers[i] = new FrameBuffer(
			shadowCasterRenderPass,
			{
				{"DepthAttachment", depthAttachemnts[i]},
			}
		);


		// Blit
		delete shadowTextures[i];
		shadowTextures[i] = Image::Create2DImage(
			extent,
			VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
			VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
		);
		delete blitFrameBuffers[i];
		blitFrameBuffers[i] = new FrameBuffer(
			blitRenderPass,
			{
				{"ColorAttachment", shadowTextures[i]},
			}
		);
		blitMaterials[i]->setSampledImage2D("depthTexture", depthAttachemnts[i], pointSampler);


		//Blur
		delete temporaryShadowTextures[i];
		temporaryShadowTextures[i] = Image::Create2DImage(
			extent,
			VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
			VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
		);
		delete blurFrameBuffers[i];
		blurFrameBuffers[i] = new FrameBuffer(
			blurRenderPass,
			{
				{"ShadowTexture", temporaryShadowTextures[i]},
			}
		);
		delete blurFrameBuffers[i + CASCADE_COUNT];
		blurFrameBuffers[i + CASCADE_COUNT] = new FrameBuffer(
			blurRenderPass,
			{
				{"ShadowTexture", shadowTextures[i]},
			}
		);
		blurMaterials[i]->setSampledImage2D("shadowTexture", shadowTextures[i], pointSampler);
		blurMaterials[i + CASCADE_COUNT]->setSampledImage2D("shadowTexture", temporaryShadowTextures[i], pointSampler);
	}
}

void CascadeEVSM_ShadowCaster_RenderFeature::CascadeEVSM_ShadowCaster_RenderFeatureData::setShadowReceiverMaterialParameters(Material* material)
{
	material->setUniformBuffer("cascadeEvsmShadowReceiverInfo", cascadeEvsmShadowReceiverInfoBuffer);
	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		material->setSampledImage2D("shadowTexture_" + std::to_string(i), shadowTextures[i], pointSampler);
	}
}

RenderFeatureDataBase* CascadeEVSM_ShadowCaster_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new CascadeEVSM_ShadowCaster_RenderFeatureData();
	featureData->shadowCasterRenderPass = m_shadowCasterRenderPass;
	featureData->blitRenderPass = m_blitRenderPass;
	featureData->blurRenderPass = m_blurRenderPass;
	featureData->pointSampler = m_pointSampler;

	for (auto i = 0; i < CASCADE_COUNT; i++)
	{
		featureData->blitInfoBuffers[i] = new Buffer(
			sizeof(CascadeEvsmBlitInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		);

		featureData->blitMaterials[i] = new Material(m_blitShader);
		featureData->blitMaterials[i]->setUniformBuffer("cascadeEvsmBlitInfo", featureData->blitInfoBuffers[i]);

		featureData->blurInfoBuffers[i] = new Buffer(
			sizeof(CascadeEvsmBlurInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		);
		featureData->blurInfoBuffers[i + CASCADE_COUNT] = new Buffer(
			sizeof(CascadeEvsmBlurInfo),
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY
		);
		featureData->blurMaterials[i] = new Material(m_blurShader);
		featureData->blurMaterials[i + CASCADE_COUNT] = new Material(m_blurShader);
		featureData->blurMaterials[i]->setUniformBuffer("cascadeEvsmBlurInfo", featureData->blurInfoBuffers[i]);
		featureData->blurMaterials[i + CASCADE_COUNT]->setUniformBuffer("cascadeEvsmBlurInfo", featureData->blurInfoBuffers[i + CASCADE_COUNT]);
	}

	featureData->lightCameraInfoBuffer = new Buffer(
		sizeof(LightCameraInfo),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);
	featureData->lightCameraInfoStagingBuffer = new Buffer(
		sizeof(LightCameraInfo) * CASCADE_COUNT,
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	featureData->cascadeEvsmShadowReceiverInfoBuffer = new Buffer(
		sizeof(CascadeEvsmShadowReceiverInfo),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	return featureData;
}

void CascadeEVSM_ShadowCaster_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
	auto featureData = static_cast<CascadeEVSM_ShadowCaster_RenderFeatureData*>(renderFeatureData);
	featureData->refresh();
}

void CascadeEVSM_ShadowCaster_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<CascadeEVSM_ShadowCaster_RenderFeatureData*>(renderFeatureData);
	delete featureData->lightCameraInfoBuffer;
	delete featureData->lightCameraInfoStagingBuffer;
	delete featureData->cascadeEvsmShadowReceiverInfoBuffer;
	for (auto i = 0; i < CASCADE_COUNT; i++)
	{
		delete featureData->depthAttachemnts[i];
		delete featureData->shadowCasterFrameBuffers[i];

		delete featureData->shadowTextures[i];
		delete featureData->blitInfoBuffers[i];
		delete featureData->blitFrameBuffers[i];
		delete featureData->blitMaterials[i];

		delete featureData->temporaryShadowTextures[i];
		delete featureData->blurMaterials[i];
		delete featureData->blurMaterials[i + CASCADE_COUNT];
		delete featureData->blurInfoBuffers[i];
		delete featureData->blurInfoBuffers[i + CASCADE_COUNT];
		delete featureData->blurFrameBuffers[i];
		delete featureData->blurFrameBuffers[i + CASCADE_COUNT];

	}
	delete featureData;
}

void CascadeEVSM_ShadowCaster_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{

}

void CascadeEVSM_ShadowCaster_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<CascadeEVSM_ShadowCaster_RenderFeatureData*>(renderFeatureData);

	glm::vec3 angularPointVPositions[8]{};
	camera->getAngularPointVPosition(angularPointVPositions);

	glm::vec3 subAngularPointVPositions[CASCADE_COUNT][8]{};
	{
		std::array<float, CASCADE_COUNT> frustumSplitRatio = featureData->frustumSegmentScales;
		for (int i = 1; i < CASCADE_COUNT; i++)
		{
			frustumSplitRatio[i] = frustumSplitRatio[i - 1] + featureData->frustumSegmentScales[i];
		}

		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			if (i == 0)
			{
				subAngularPointVPositions[i][0] = angularPointVPositions[0];
				subAngularPointVPositions[i][1] = angularPointVPositions[1];
				subAngularPointVPositions[i][2] = angularPointVPositions[2];
				subAngularPointVPositions[i][3] = angularPointVPositions[3];
			}
			else
			{
				subAngularPointVPositions[i][0] = (1 - frustumSplitRatio[i - 1]) * angularPointVPositions[0] + frustumSplitRatio[i - 1] * angularPointVPositions[4];
				subAngularPointVPositions[i][1] = (1 - frustumSplitRatio[i - 1]) * angularPointVPositions[1] + frustumSplitRatio[i - 1] * angularPointVPositions[5];
				subAngularPointVPositions[i][2] = (1 - frustumSplitRatio[i - 1]) * angularPointVPositions[2] + frustumSplitRatio[i - 1] * angularPointVPositions[6];
				subAngularPointVPositions[i][3] = (1 - frustumSplitRatio[i - 1]) * angularPointVPositions[3] + frustumSplitRatio[i - 1] * angularPointVPositions[7];
			}

			if (i == CASCADE_COUNT - 1)
			{
				subAngularPointVPositions[i][4] = angularPointVPositions[4];
				subAngularPointVPositions[i][5] = angularPointVPositions[5];
				subAngularPointVPositions[i][6] = angularPointVPositions[6];
				subAngularPointVPositions[i][7] = angularPointVPositions[7];
			}
			else
			{
				subAngularPointVPositions[i][4] = (1 - frustumSplitRatio[i]) * angularPointVPositions[0] + frustumSplitRatio[i] * angularPointVPositions[4];
				subAngularPointVPositions[i][5] = (1 - frustumSplitRatio[i]) * angularPointVPositions[1] + frustumSplitRatio[i] * angularPointVPositions[5];
				subAngularPointVPositions[i][6] = (1 - frustumSplitRatio[i]) * angularPointVPositions[2] + frustumSplitRatio[i] * angularPointVPositions[6];
				subAngularPointVPositions[i][7] = (1 - frustumSplitRatio[i]) * angularPointVPositions[3] + frustumSplitRatio[i] * angularPointVPositions[7];
			}
		}

		for (int i = CASCADE_COUNT - 1; i > 0; i--)
		{
			subAngularPointVPositions[i][0] = featureData->overlapScale * subAngularPointVPositions[i - 1][0] + (1 - featureData->overlapScale) * subAngularPointVPositions[i - 1][4];
			subAngularPointVPositions[i][1] = featureData->overlapScale * subAngularPointVPositions[i - 1][1] + (1 - featureData->overlapScale) * subAngularPointVPositions[i - 1][5];
			subAngularPointVPositions[i][2] = featureData->overlapScale * subAngularPointVPositions[i - 1][2] + (1 - featureData->overlapScale) * subAngularPointVPositions[i - 1][6];
			subAngularPointVPositions[i][3] = featureData->overlapScale * subAngularPointVPositions[i - 1][3] + (1 - featureData->overlapScale) * subAngularPointVPositions[i - 1][7];
		}
	}

	glm::vec3 sphereCenterVPositions[CASCADE_COUNT]{};
	float sphereRadius[CASCADE_COUNT]{};
	{
		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			float len = std::abs(subAngularPointVPositions[i][0].z - subAngularPointVPositions[i][4].z);
			float a2 = glm::dot(subAngularPointVPositions[i][0] - subAngularPointVPositions[i][2], subAngularPointVPositions[i][0] - subAngularPointVPositions[i][2]);
			float b2 = glm::dot(subAngularPointVPositions[i][4] - subAngularPointVPositions[i][6], subAngularPointVPositions[i][4] - subAngularPointVPositions[i][6]);
			float x = len / 2 - (a2 - b2) / 8 / len;

			sphereCenterVPositions[i] = glm::vec3(0, 0, subAngularPointVPositions[i][0].z - x);
			sphereRadius[i] = std::sqrt(a2 / 4 + x * x);
		}
	}

	glm::vec3 lightVPositions[CASCADE_COUNT]{};
	LightCameraInfo lightCameraInfos[CASCADE_COUNT]{};
	CascadeEvsmShadowReceiverInfo cascadeEvsmShadowReceiverInfo{};
	CascadeEvsmBlitInfo cascadeEvsmBlitInfo{};
	{
		auto light = Instance::getLightManager().getMainLight();
		glm::mat4 cameraV = camera->getViewMatrix();
		glm::mat4 lightM = light->getGameObject()->transform.getModelMatrix();
		glm::vec3 lightWPosition = lightM * glm::vec4(0, 0, 0, 1);
		glm::vec3 lightWView = glm::normalize(glm::vec3(lightM * glm::vec4(0, 0, -1, 1)) - lightWPosition);
		glm::vec3 lightVPosition = cameraV * lightM * glm::vec4(0, 0, 0, 1);
		glm::vec3 lightVUp = glm::normalize(glm::vec3(cameraV * lightM * glm::vec4(0, 1, 0, 1)) - lightVPosition);
		glm::vec3 lightVRight = glm::normalize(glm::vec3(cameraV * lightM * glm::vec4(1, 0, 0, 1)) - lightVPosition);
		glm::vec3 lightVView = glm::normalize(glm::vec3(cameraV * lightM * glm::vec4(0, 0, -1, 1)) - lightVPosition);

		cascadeEvsmBlitInfo.c1 = featureData->c1;
		cascadeEvsmBlitInfo.c2 = featureData->c2;
		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			lightVPositions[i] = sphereCenterVPositions[i] - lightVView * (sphereRadius[i] + featureData->lightCameraCompensationDistances[i]);
		}
		cascadeEvsmShadowReceiverInfo.wLightDirection = lightWView;
		cascadeEvsmShadowReceiverInfo.vLightDirection = lightVView;
		cascadeEvsmShadowReceiverInfo.c1 = featureData->c1;
		cascadeEvsmShadowReceiverInfo.c2 = featureData->c2;
		cascadeEvsmShadowReceiverInfo.threshold = featureData->threshold;
		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			float halfWidth = sphereRadius[i];
			float flatDistence = sphereRadius[i] + sphereRadius[i] + featureData->lightCameraCompensationDistances[i];
			lightCameraInfos[i].projection = glm::mat4(
				1.0 / halfWidth, 0, 0, 0,
				0, 1.0 / halfWidth, 0, 0,
				0, 0, -1.0 / flatDistence, 0,
				0, 0, 0, 1
			);

			auto matrixVC2VL = glm::lookAt(lightVPositions[i], sphereCenterVPositions[i], lightVUp);
			lightCameraInfos[i].view = matrixVC2VL * cameraV;

			lightCameraInfos[i].viewProjection = lightCameraInfos[i].projection * lightCameraInfos[i].view;

			cascadeEvsmShadowReceiverInfo.matrixVC2PL[i] = lightCameraInfos[i].projection * matrixVC2VL;

			cascadeEvsmShadowReceiverInfo.thresholdVZ[i * 2 + 0].x = subAngularPointVPositions[i][0].z;
			cascadeEvsmShadowReceiverInfo.thresholdVZ[i * 2 + 1].x = subAngularPointVPositions[i][4].z;

			cascadeEvsmShadowReceiverInfo.texelSize[i] = { 1.0f / featureData->shadowImageResolutions[i], 1.0f / featureData->shadowImageResolutions[i], 0, 0};

			cascadeEvsmBlitInfo.texelSize = cascadeEvsmShadowReceiverInfo.texelSize[i];
			featureData->blitInfoBuffers[i]->WriteData(&cascadeEvsmBlitInfo, sizeof(CascadeEvsmBlitInfo));

			CascadeEvsmBlurInfo cascadeEvsmBlurInfo{};
			cascadeEvsmBlurInfo.texelSize = cascadeEvsmShadowReceiverInfo.texelSize[i];
			cascadeEvsmBlurInfo.sampleOffset = { featureData->blurOffsets[i], 0 };
			featureData->blurInfoBuffers[i]->WriteData(&cascadeEvsmBlurInfo, sizeof(CascadeEvsmBlurInfo));
			cascadeEvsmBlurInfo.sampleOffset = { 0, featureData->blurOffsets[i] };
			featureData->blurInfoBuffers[i + CASCADE_COUNT]->WriteData(&cascadeEvsmBlurInfo, sizeof(CascadeEvsmBlurInfo));
		}

		std::sort(cascadeEvsmShadowReceiverInfo.thresholdVZ, cascadeEvsmShadowReceiverInfo.thresholdVZ + CASCADE_COUNT * 2, [](glm::vec4 a, glm::vec4 b)->bool {return a.x > b.x; });

		featureData->lightCameraInfoStagingBuffer->WriteData(&lightCameraInfos, sizeof(LightCameraInfo) * CASCADE_COUNT);
		featureData->cascadeEvsmShadowReceiverInfoBuffer->WriteData(&cascadeEvsmShadowReceiverInfo, sizeof(CascadeEvsmShadowReceiverInfo));
	}

	IntersectionChecker checkers[CASCADE_COUNT];
	{
		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			glm::vec4 planes[6] =
			{
				glm::vec4(-1, 0, 0, sphereRadius[i]),
				glm::vec4(1, 0, 0, sphereRadius[i]),
				glm::vec4(0, -1, 0, sphereRadius[i]),
				glm::vec4(0, 1, 0, sphereRadius[i]),
				glm::vec4(0, 0, -1, 0),
				glm::vec4(0, 0, 1, sphereRadius[i] * 2 + featureData->lightCameraCompensationDistances[i])
			};
			checkers[i].setIntersectPlanes(planes, 6);
		}
	}

	struct RendererWrapper
	{
		Material* material;
		Mesh* mesh;
	};
	std::vector< RendererWrapper> rendererWrappers[CASCADE_COUNT];
	for (const auto& rendererComponent : *rendererComponents)
	{
		auto material = rendererComponent->getMaterial(m_shadowCasterRenderPassName);
		if (material == nullptr) continue;

		material->setUniformBuffer("lightCameraInfo", featureData->lightCameraInfoBuffer);
		material->setUniformBuffer("meshObjectInfo", rendererComponent->getObjectInfoBuffer());

		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			auto obbVertexes = rendererComponent->mesh->getOrientedBoundingBox().BoundryVertexes();
			auto mvMatrix = lightCameraInfos[i].view * rendererComponent->getGameObject()->transform.getModelMatrix();
			if (rendererComponent->enableFrustumCulling && !checkers[i].check(obbVertexes.data(), obbVertexes.size(), mvMatrix))
			{
				continue;
			}
			rendererWrappers[i].push_back({ material , rendererComponent->mesh });
		}
	}

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		commandBuffer->copyBuffer(featureData->lightCameraInfoStagingBuffer, sizeof(LightCameraInfo) * i, featureData->lightCameraInfoBuffer, 0, sizeof(LightCameraInfo));

		VkClearValue depthClearValue{};
		depthClearValue.depthStencil.depth = 1.0f;
		commandBuffer->beginRenderPass(
			m_shadowCasterRenderPass,
			featureData->shadowCasterFrameBuffers[i],
			{
				{"DepthAttachment", depthClearValue}
			}
		);

		for (const auto& wrapper : rendererWrappers[i])
		{
			commandBuffer->drawMesh(wrapper.mesh, wrapper.material);
		}

		commandBuffer->endRenderPass();
	}

	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		commandBuffer->beginRenderPass(m_blitRenderPass, featureData->blitFrameBuffers[i]);

		commandBuffer->drawMesh(m_fullScreenMesh, featureData->blitMaterials[i]);

		commandBuffer->endRenderPass();
	}

	for (int ii = 0; ii < featureData->iterateCount; ii++)
	{
		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			commandBuffer->beginRenderPass(m_blurRenderPass, featureData->blurFrameBuffers[i]);
			commandBuffer->drawMesh(m_fullScreenMesh, featureData->blurMaterials[i]);
			commandBuffer->endRenderPass();

			commandBuffer->beginRenderPass(m_blurRenderPass, featureData->blurFrameBuffers[i + CASCADE_COUNT]);
			commandBuffer->drawMesh(m_fullScreenMesh, featureData->blurMaterials[i + CASCADE_COUNT]);
			commandBuffer->endRenderPass();
		}
	}

	commandBuffer->endRecord();
}

void CascadeEVSM_ShadowCaster_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void CascadeEVSM_ShadowCaster_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
