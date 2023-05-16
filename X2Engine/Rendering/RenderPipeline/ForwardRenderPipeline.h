#pragma once
#include "Core/Graphic/Rendering/RenderPipelineBase.h"


class ForwardRenderPipeline final : public RenderPipelineBase
{
public:
	ForwardRenderPipeline();
	virtual ~ForwardRenderPipeline();
	ForwardRenderPipeline(const ForwardRenderPipeline&) = delete;
	ForwardRenderPipeline& operator=(const ForwardRenderPipeline&) = delete;
	ForwardRenderPipeline(ForwardRenderPipeline&&) = delete;
	ForwardRenderPipeline& operator=(ForwardRenderPipeline&&) = delete;

	RTTR_ENABLE(RenderPipelineBase)
};