#include "EnumRenderPipeline.h"
#include "Rendering/Renderer/ForwardRenderer.h"
#include "Rendering/Renderer/SSAOVisualizationRenderer.h"
#include "Rendering/Renderer/GTAOVisualizationRenderer.h"
#include "Rendering/Renderer/HBAOVisualizationRenderer.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<EnumRenderPipeline>("EnumRenderPipeline")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}

EnumRenderPipeline::EnumRenderPipeline()
	: RenderPipelineBase()
{
	useRenderer("ForwardRenderer", new ForwardRenderer());

	useRenderer("SSAOVisualizationRenderer", new SSAOVisualizationRenderer());
	useRenderer("GTAOVisualizationRenderer", new GTAOVisualizationRenderer());
	useRenderer("HBAOVisualizationRenderer", new HBAOVisualizationRenderer());

	/*UseRenderer("TBForwardRenderer", new TBForwardRenderer());
	UseRenderer("AmbientOcclusionRenderer", new AmbientOcclusionRenderer());
	UseRenderer("ShadowVisualizationRenderer", new ShadowVisualizationRenderer());
	UseRenderer("BuildAssetRenderer", new BuildAssetRenderer());*/
}

EnumRenderPipeline::~EnumRenderPipeline()
{
	delete static_cast<ForwardRenderer*>(getRenderer("ForwardRenderer"));
	delete static_cast<SSAOVisualizationRenderer*>(getRenderer("SSAOVisualizationRenderer"));
	delete static_cast<GTAOVisualizationRenderer*>(getRenderer("GTAOVisualizationRenderer"));
	delete static_cast<HBAOVisualizationRenderer*>(getRenderer("HBAOVisualizationRenderer"));

	/*delete static_cast<TBForwardRenderer*>(Renderer("TBForwardRenderer"));
	delete static_cast<ShadowVisualizationRenderer*>(Renderer("ShadowVisualizationRenderer"));
	delete static_cast<BuildAssetRenderer*>(Renderer("BuildAssetRenderer"));*/
}
