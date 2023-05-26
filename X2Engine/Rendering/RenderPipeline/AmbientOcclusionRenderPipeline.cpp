#include "AmbientOcclusionRenderPipeline.h"
#include "Rendering/Renderer/AmbientOcclusionVisualizationRenderer.h"


RTTR_REGISTRATION
{
	rttr::registration::class_<AmbientOcclusionRenderPipeline>("AmbientOcclusionRenderPipeline")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

AmbientOcclusionRenderPipeline::AmbientOcclusionRenderPipeline()
	: RenderPipelineBase()
{
	useRenderer("AmbientOcclusionVisualizationRenderer", new AmbientOcclusionVisualizationRenderer());
}

AmbientOcclusionRenderPipeline::~AmbientOcclusionRenderPipeline()
{
	delete static_cast<AmbientOcclusionVisualizationRenderer*>(getRenderer("AmbientOcclusionVisualizationRenderer"));
}
	
