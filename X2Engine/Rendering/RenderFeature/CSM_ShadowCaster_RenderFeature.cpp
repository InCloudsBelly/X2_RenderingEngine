#include "Rendering/RenderFeature/CSM_ShadowCaster_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Instance/Image.h"

#include "Asset/Mesh.h"
#include "Utils/OrientedBoundingBox.h"

#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Logic/Component/Base/LightBase.h"
#include "Core/Graphic/Rendering/Material.h"
#include "Core/Graphic/Manager/LightManager.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"

#include "Light/DirectionalLight.h"

#include <set>
#include "Utils/IntersectionChecker.h"
#include <array>
#include <algorithm>

RTTR_REGISTRATION
{
	rttr::registration::class_<CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderPass>("CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderPass").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeatureData>("CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeatureData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<CSM_ShadowCaster_RenderFeature>("CSM_ShadowCaster_RenderFeature").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

void CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderPass::populateRenderPassSettings(RenderPassSettings& creator)
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
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0
	);
}

CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderPass::CSM_ShadowCaster_RenderPass()
	: RenderPassBase()
{
}

CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderPass::~CSM_ShadowCaster_RenderPass()
{
}

CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeatureData::CSM_ShadowCaster_RenderFeatureData()
	: RenderFeatureDataBase()
	, shadowImageArray(nullptr)
	, shadowFrameBuffers()
	, lightCameraInfoBuffer(nullptr)
	, lightCameraInfoHostStagingBuffer(nullptr)
	, frustumSegmentScales()
	, lightCameraCompensationDistances()
	, shadowImageResolutions(1024)
{
	frustumSegmentScales.fill(1.0 / CASCADE_COUNT);
	lightCameraCompensationDistances.fill(15);
	bias.fill({ 0.00005 , 0.0065 });
	overlapScale = 0.3;
	sampleHalfWidth = 1;
}

CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeatureData::~CSM_ShadowCaster_RenderFeatureData()
{
}

CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeature()
	: RenderFeatureBase()
	, m_shadowMapRenderPass(Instance::getRenderPassManager().loadRenderPass<CSM_ShadowCaster_RenderPass>())
	, m_shadowMapRenderPassName(rttr::type::get<CSM_ShadowCaster_RenderPass>().get_name().to_string())
	, m_shadowImageSampler(
		new ImageSampler(
			VkFilter::VK_FILTER_NEAREST,
			VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			0.0f,
			VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
		)
	)
{

}

CSM_ShadowCaster_RenderFeature::~CSM_ShadowCaster_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<CSM_ShadowCaster_RenderPass>();
	delete m_shadowImageSampler;
}


void CSM_ShadowCaster_RenderFeature::CSM_ShadowCaster_RenderFeatureData::setShadowReceiverMaterialParameters(Material* material)
{
	material->setUniformBuffer("csmShadowReceiverInfo", csmShadowReceiverInfoBuffer);
	material->setSampledImage2D("shadowTextureArray", shadowImageArray, sampler);
}

RenderFeatureDataBase* CSM_ShadowCaster_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto featureData = new CSM_ShadowCaster_RenderFeatureData();
	featureData->shadowMapRenderPass = m_shadowMapRenderPass;
	featureData->sampler = m_shadowImageSampler;

	featureData->lightCameraInfoBuffer = new Buffer(
		sizeof(LightCameraInfo),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);
	featureData->lightCameraInfoHostStagingBuffer = new Buffer(
		sizeof(LightCameraInfo) * CASCADE_COUNT,
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	featureData->csmShadowReceiverInfoBuffer = new Buffer(
		sizeof(CsmShadowReceiverInfo),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	return featureData;
}

void CSM_ShadowCaster_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{
	auto featureData = static_cast<CSM_ShadowCaster_RenderFeatureData*>(renderFeatureData);
	
	VkExtent2D extent = { featureData->shadowImageResolutions, featureData->shadowImageResolutions };
	delete featureData->shadowImageArray;

	featureData->shadowImageArray = Image::Create2DImageArray(
		extent,
		VkFormat::VK_FORMAT_D32_SFLOAT,
		VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
		CASCADE_COUNT
	);

	for (auto i = 0; i < CASCADE_COUNT; i++)
	{
		featureData->shadowImageArray->AddImageView(
			"SingleImageView_" + std::to_string(i),
			VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
			VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
			i,
			1
		);
	}

	for (auto i = 0; i < CASCADE_COUNT; i++)
	{
		delete featureData->shadowFrameBuffers[i];
		featureData->shadowFrameBuffers[i] = new FrameBuffer(
			featureData->shadowMapRenderPass,
			{
				{"DepthAttachment", featureData->shadowImageArray}
			},
			{
				{"DepthAttachment", "SingleImageView_" + std::to_string(i)}
			}
			);
	}
}

void CSM_ShadowCaster_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<CSM_ShadowCaster_RenderFeatureData*>(renderFeatureData);
	delete featureData->lightCameraInfoBuffer;
	delete featureData->lightCameraInfoHostStagingBuffer;
	delete featureData->csmShadowReceiverInfoBuffer;
	delete featureData->shadowImageArray;
	for (auto i = 0; i < CASCADE_COUNT; i++)
	{
		delete featureData->shadowFrameBuffers[i];
	}
	delete featureData;
}

void CSM_ShadowCaster_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{

}

void CSM_ShadowCaster_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<CSM_ShadowCaster_RenderFeatureData*>(renderFeatureData);

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
	CsmShadowReceiverInfo csmShadowReceiverInfo{};
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

		for (int i = 0; i < CASCADE_COUNT; i++)
		{
			//glm::mat4 matrixVC2VL = glm::lookAt({ 0, 0, 0 }, lightVView, lightVUp);
			//float unit = sphereRadius[i] * 2 / featureData->shadowImageResolutions[i];

			//glm::vec3 virtualCenterLVPosition = matrixVC2VL * glm::vec4(sphereCenterVPositions[i], 1);
			//virtualCenterLVPosition.x -= std::fmod(virtualCenterLVPosition.x, unit);
			//virtualCenterLVPosition.y -= std::fmod(virtualCenterLVPosition.y, unit);

			//sphereCenterVPositions[i] = glm::inverse(matrixVC2VL) * glm::vec4(virtualCenterLVPosition, 1);

			lightVPositions[i] = sphereCenterVPositions[i] - lightVView * (sphereRadius[i] + featureData->lightCameraCompensationDistances[i]);
		}
		csmShadowReceiverInfo.wLightDirection = lightWView;
		csmShadowReceiverInfo.vLightDirection = lightVView;
		csmShadowReceiverInfo.sampleHalfWidth = featureData->sampleHalfWidth;
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

			csmShadowReceiverInfo.matrixVC2PL[i] = lightCameraInfos[i].projection * matrixVC2VL;

			csmShadowReceiverInfo.thresholdVZ[i * 2 + 0].x = subAngularPointVPositions[i][0].z;
			csmShadowReceiverInfo.thresholdVZ[i * 2 + 1].x = subAngularPointVPositions[i][4].z;

			csmShadowReceiverInfo.texelSize[i] = { 1.0f / featureData->shadowImageResolutions, 1.0f / featureData->shadowImageResolutions, 0, 0 };

			csmShadowReceiverInfo.bias[i] = glm::vec4(featureData->bias[i], 0, 0);
		}

		std::sort(csmShadowReceiverInfo.thresholdVZ, csmShadowReceiverInfo.thresholdVZ + CASCADE_COUNT * 2, [](glm::vec4 a, glm::vec4 b)->bool {return a.x > b.x; });

		featureData->lightCameraInfoHostStagingBuffer->WriteData(&lightCameraInfos, sizeof(LightCameraInfo) * CASCADE_COUNT);
		featureData->csmShadowReceiverInfoBuffer->WriteData(&csmShadowReceiverInfo, sizeof(CsmShadowReceiverInfo));
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
		auto material = rendererComponent->getMaterial(m_shadowMapRenderPassName);
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
		commandBuffer->copyBuffer(featureData->lightCameraInfoHostStagingBuffer, sizeof(LightCameraInfo) * i, featureData->lightCameraInfoBuffer, 0, sizeof(LightCameraInfo));

		VkClearValue depthClearValue{};
		depthClearValue.depthStencil.depth = 1.0f;
		commandBuffer->beginRenderPass(
			m_shadowMapRenderPass,
			featureData->shadowFrameBuffers[i],
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

	///Wait shadow texture ready
	{
		auto shadowTextureBarrier = ImageMemoryBarrier
		(
			featureData->shadowImageArray,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT
		);
		commandBuffer->addPipelineImageBarrier(
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			{ &shadowTextureBarrier }
		);
	}

	commandBuffer->endRecord();
}

void CSM_ShadowCaster_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void CSM_ShadowCaster_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}
