#pragma once
#include "Core/Graphic/Rendering/RenderPipelineBase.h"


class EnumRenderPipeline final : public RenderPipelineBase
{
public:
	EnumRenderPipeline();
	virtual ~EnumRenderPipeline();
	EnumRenderPipeline(const EnumRenderPipeline&) = delete;
	EnumRenderPipeline& operator=(const EnumRenderPipeline&) = delete;
	EnumRenderPipeline(EnumRenderPipeline&&) = delete;
	EnumRenderPipeline& operator=(EnumRenderPipeline&&) = delete;

	RTTR_ENABLE(RenderPipelineBase)
};