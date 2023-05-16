#include "ForwardRenderPipeline.h"
#include "Rendering/Renderer/ForwardRenderer.h"


RTTR_REGISTRATION
{
	rttr::registration::class_<ForwardRenderPipeline>("ForwardRenderPipeline")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

ForwardRenderPipeline::ForwardRenderPipeline()
	: RenderPipelineBase()
{
	useRenderer("ForwardRenderer", new ForwardRenderer());
	/*UseRenderer("TBForwardRenderer", new TBForwardRenderer());
	UseRenderer("AmbientOcclusionRenderer", new AmbientOcclusionRenderer());
	UseRenderer("ShadowVisualizationRenderer", new ShadowVisualizationRenderer());
	UseRenderer("BuildAssetRenderer", new BuildAssetRenderer());*/
}

ForwardRenderPipeline::~ForwardRenderPipeline()
{
	delete static_cast<ForwardRenderer*>(getRenderer("ForwardRenderer"));
	/*delete static_cast<TBForwardRenderer*>(Renderer("TBForwardRenderer"));
	delete static_cast<ShadowVisualizationRenderer*>(Renderer("ShadowVisualizationRenderer"));
	delete static_cast<BuildAssetRenderer*>(Renderer("BuildAssetRenderer"));*/
}
