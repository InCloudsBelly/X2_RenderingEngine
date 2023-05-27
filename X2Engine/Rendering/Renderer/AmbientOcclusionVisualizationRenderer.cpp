#include "AmbientOcclusionVisualizationRenderer.h"

#include "Rendering/RenderFeature/Background_RenderFeature.h"
#include "Rendering/RenderFeature/Present_RenderFeature.h"
#include "Rendering/RenderFeature/Geometry_RenderFeature.h"
#include "Rendering/RenderFeature/ClearColorAttachment_RenderFeature.h"
#include "Rendering/RenderFeature/SSAO_RenderFeature.h"
#include "Rendering/RenderFeature/AO_Blur_RenderFeature.h"
#include "Rendering/RenderFeature/AO_Cover_RenderFeature.h"

#include "Core/Logic/Component/Base/CameraBase.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"

#include <glm/glm.hpp>            

RTTR_REGISTRATION
{
	rttr::registration::class_<AmbientOcclusionVisualizationRenderer>("AmbientOcclusionVisualizationRenderer").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<AmbientOcclusionVisualizationRenderer::AmbientOcclusionVisualizationRendererData>("AmbientOcclusionVisualizationRenderer::AmbientOcclusionVisualizationRendererData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

AmbientOcclusionVisualizationRenderer::AmbientOcclusionVisualizationRenderer()
	: RendererBase()
{

	pushRenderFeature("Geometry_RenderFeature", new Geometry_RenderFeature());
	pushRenderFeature("ClearColorAttachment_RenderFeature", new ClearColorAttachment_RenderFeature());
	
	pushRenderFeature("SSAO_RenderFeature", new SSAO_RenderFeature());
	pushRenderFeature("SSAO_Blur_RenderFeature", new AO_Blur_RenderFeature());
	pushRenderFeature("SSAO_Cover_RenderFeature", new AO_Cover_RenderFeature());

	//pushRenderFeature("Background_RenderFeature", new Background_RenderFeature());

	pushRenderFeature("Present_RenderFeature", new Present_RenderFeature());

}

AmbientOcclusionVisualizationRenderer::~AmbientOcclusionVisualizationRenderer()
{
	//delete static_cast<Background_RenderFeature*>(getRenderFeature("Background_RenderFeature"));
	delete static_cast< SSAO_RenderFeature*>(getRenderFeature("SSAO_RenderFeature"));
	delete static_cast<AO_Blur_RenderFeature*>(getRenderFeature("SSAO_Blur_RenderFeature"));
	delete static_cast<AO_Cover_RenderFeature*>(getRenderFeature("SSAO_Cover_RenderFeature"));
	delete static_cast<Geometry_RenderFeature*>(getRenderFeature("Geometry_RenderFeature"));
	delete static_cast<Present_RenderFeature*>(getRenderFeature("Present_RenderFeature"));
}

AmbientOcclusionVisualizationRenderer::AmbientOcclusionVisualizationRendererData::AmbientOcclusionVisualizationRendererData()
	: RendererDataBase()
{
}

AmbientOcclusionVisualizationRenderer::AmbientOcclusionVisualizationRendererData::~AmbientOcclusionVisualizationRendererData()
{
}

RendererDataBase* AmbientOcclusionVisualizationRenderer::onCreateRendererData(CameraBase* camera)
{
	return new AmbientOcclusionVisualizationRendererData();
}

void AmbientOcclusionVisualizationRenderer::onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)
{
	/*auto backgroundData = static_cast<Background_RenderFeature::Background_RenderFeatureData*>(rendererData->getRenderFeatureData("Background_RenderFeature"));
	backgroundData->needClearColorAttachment = false;
	backgroundData->backgroundImage = Instance::g_backgroundImage;*/
	

	auto geometryFeatureData = static_cast<Geometry_RenderFeature::Geometry_RenderFeatureData*>( rendererData->getRenderFeatureData("Geometry_RenderFeature"));

	auto ssaoFeatureData = static_cast<SSAO_RenderFeature::SSAO_RenderFeatureData*>(rendererData->getRenderFeatureData("SSAO_RenderFeature"));
	ssaoFeatureData->depthTexture = geometryFeatureData->depthTexture;
	ssaoFeatureData->normalTexture = geometryFeatureData->normalTexture;
	ssaoFeatureData->positionTexture = geometryFeatureData->positionTexture;
	ssaoFeatureData->samplePointRadius = 0.2;

	auto aoBlurFeatureData = static_cast<AO_Blur_RenderFeature::AO_Blur_RenderFeatureData*>(rendererData->getRenderFeatureData("SSAO_Blur_RenderFeature"));
	aoBlurFeatureData->normalTexture = geometryFeatureData->normalTexture;
	aoBlurFeatureData->occlusionTexture = ssaoFeatureData->occlusionTexture;
	aoBlurFeatureData->iterateCount = 2;

	auto aoCoverFeatureData = static_cast<AO_Cover_RenderFeature::AO_Cover_RenderFeatureData*>(rendererData->getRenderFeatureData("SSAO_Cover_RenderFeature"));
	aoCoverFeatureData->occlusionTexture = ssaoFeatureData->occlusionTexture;
	aoCoverFeatureData->intensity = 1.5f;

}

void AmbientOcclusionVisualizationRenderer::onDestroyRendererData(RendererDataBase* rendererData)
{
	delete static_cast<AmbientOcclusionVisualizationRendererData*>(rendererData);
}