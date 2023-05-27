#include "Geometry_RenderFeature.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Rendering/FrameBuffer.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Graphic/Command/ImageMemoryBarrier.h"
#include "Asset/Mesh.h"
#include "Utils/OrientedBoundingBox.h"
#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Logic/Component/Base/Renderer.h"

#include "Core/Graphic/Rendering/Material.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<Geometry_RenderFeature::Geometry_RenderPass>("Geometry_RenderFeature::Geometry_RenderPass")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<Geometry_RenderFeature::Geometry_RenderFeatureData>("Geometry_RenderFeature::Geometry_RenderFeatureData")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<Geometry_RenderFeature>("Geometry_RenderFeature")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

void Geometry_RenderFeature::Geometry_RenderPass::populateRenderPassSettings(RenderPassSettings& settings)
{
	settings.addColorAttachment(
		"Depth",
		VK_FORMAT_R32_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	settings.addColorAttachment(
		"vPosition",
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	settings.addColorAttachment(
		"vNormal",
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);


	settings.addDepthAttachment(
		"DepthAttachment",
		VK_FORMAT_D32_SFLOAT,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);
	settings.addSubpass(
		"DrawSubpass",
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		{ "Depth","vPosition", "vNormal" },
		"DepthAttachment"
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
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
	);
}
Geometry_RenderFeature::Geometry_RenderPass::Geometry_RenderPass()
	: RenderPassBase()
{
}

Geometry_RenderFeature::Geometry_RenderPass::~Geometry_RenderPass()
{
}

Geometry_RenderFeature::Geometry_RenderFeatureData::Geometry_RenderFeatureData()
	: RenderFeatureDataBase()
	, frameBuffer(nullptr)
	, depthTexture(nullptr)
	, normalTexture(nullptr)
{
}

Geometry_RenderFeature::Geometry_RenderFeatureData::~Geometry_RenderFeatureData()
{
}

Geometry_RenderFeature::Geometry_RenderFeature()
	: RenderFeatureBase()
	, m_geometryRenderPass(Instance::getRenderPassManager().loadRenderPass<Geometry_RenderPass>())
	, m_geometryRenderPassName(rttr::type::get<Geometry_RenderPass>().get_name().to_string())
{

}

Geometry_RenderFeature::~Geometry_RenderFeature()
{
	Instance::getRenderPassManager().unloadRenderPass<Geometry_RenderPass>();
}


RenderFeatureDataBase* Geometry_RenderFeature::createRenderFeatureData(CameraBase* camera)
{
	auto extent = camera->attachments["ColorAttachment"]->getExtent2D();
	auto featureData = new Geometry_RenderFeatureData();
	featureData->depthTexture = Image::Create2DImage(
		extent,
		VkFormat::VK_FORMAT_R32_SFLOAT,
		VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
	);
	featureData->normalTexture = Image::Create2DImage(
		extent,
		VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT,
		VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
	);
	featureData->positionTexture = Image::Create2DImage(
		extent,
		VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT,
		VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
	);
	featureData->frameBuffer = new FrameBuffer(
		m_geometryRenderPass,
		{
			{"Depth", featureData->depthTexture},
			{"vPosition", featureData->positionTexture},
			{"vNormal", featureData->normalTexture},
			{"DepthAttachment", camera->attachments["DepthAttachment"]},
		}
	);
	return featureData;
}

void Geometry_RenderFeature::resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)
{

}

void Geometry_RenderFeature::destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)
{
	auto featureData = static_cast<Geometry_RenderFeatureData*>(renderFeatureData);
	delete featureData->frameBuffer;
	delete featureData->depthTexture;
	delete featureData->normalTexture;
	delete featureData->positionTexture;
	delete featureData;
}

void Geometry_RenderFeature::prepare(RenderFeatureDataBase* renderFeatureData)
{
}

void Geometry_RenderFeature::excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	auto featureData = static_cast<Geometry_RenderFeatureData*>(renderFeatureData);

	commandBuffer->reset();
	commandBuffer->beginRecord(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	///Render
	{
		VkClearValue depthClearValue{};
		depthClearValue.depthStencil.depth = 1.0f;
		VkClearValue normalClearValue{};
		normalClearValue.color = { 0.5f, 0.5f, 1.0f, 0.0f };
		commandBuffer->beginRenderPass(
			m_geometryRenderPass,
			featureData->frameBuffer,
			{
				{"Depth", depthClearValue},
				{"vNormal", normalClearValue},
				{"vPosition", normalClearValue},
				{"DepthAttachment", depthClearValue}
			}
		);

		auto viewMatrix = camera->getViewMatrix();
		for (const auto& rendererComponent : *rendererComponents)
		{
			auto material = rendererComponent->getMaterial(m_geometryRenderPassName);
			if (material == nullptr) continue;

			auto obbVertexes = rendererComponent->mesh->getOrientedBoundingBox().BoundryVertexes();
			auto mvMatrix = viewMatrix * rendererComponent->getGameObject()->transform.getModelMatrix();
			if (rendererComponent->enableFrustumCulling && !camera->checkInFrustum(obbVertexes, mvMatrix))
			{
				continue;
			}

			material->setUniformBuffer("cameraInfo", camera->getCameraInfoBuffer());
			material->setUniformBuffer("meshObjectInfo", rendererComponent->getObjectInfoBuffer());

			commandBuffer->drawMesh(rendererComponent->mesh, material);
		}
		commandBuffer->endRenderPass();
	}

	commandBuffer->endRecord();
}

void Geometry_RenderFeature::submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->submit();
}

void Geometry_RenderFeature::finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)
{
	commandBuffer->waitForFinish();
}

