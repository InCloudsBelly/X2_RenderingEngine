#pragma once
#include "Core/Graphic/Rendering/RenderPipelineBase.h"


class AmbientOcclusionRenderPipeline final : public RenderPipelineBase
{
public:
	AmbientOcclusionRenderPipeline();
	virtual ~AmbientOcclusionRenderPipeline();
	AmbientOcclusionRenderPipeline(const AmbientOcclusionRenderPipeline&) = delete;
	AmbientOcclusionRenderPipeline& operator=(const AmbientOcclusionRenderPipeline&) = delete;
	AmbientOcclusionRenderPipeline(AmbientOcclusionRenderPipeline&&) = delete;
	AmbientOcclusionRenderPipeline& operator=(AmbientOcclusionRenderPipeline&&) = delete;

	RTTR_ENABLE(RenderPipelineBase)
};