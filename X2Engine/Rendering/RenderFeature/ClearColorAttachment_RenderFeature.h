#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/vec2.hpp>

class ClearColorAttachment_RenderFeature final : public RenderFeatureBase
{
public:
	class ClearColorAttachment_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class ClearColorAttachment_RenderFeature;
	public:
		VkClearColorValue clearValue;
		CONSTRUCTOR(ClearColorAttachment_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(ClearColorAttachment_RenderFeature)

private:

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};