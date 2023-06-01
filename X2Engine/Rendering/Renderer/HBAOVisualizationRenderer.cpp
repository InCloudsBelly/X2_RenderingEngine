#include "HBAOVisualizationRenderer.h"

#include "Rendering/RenderFeature/Background_RenderFeature.h"
#include "Rendering/RenderFeature/Present_RenderFeature.h"
#include "Rendering/RenderFeature/Geometry_RenderFeature.h"
#include "Rendering/RenderFeature/ClearColorAttachment_RenderFeature.h"
#include "Rendering/RenderFeature/SSAO_RenderFeature.h"
#include "Rendering/RenderFeature/AO_Blur_RenderFeature.h"
#include "Rendering/RenderFeature/AO_Cover_RenderFeature.h"
#include "Rendering/RenderFeature/HBAO_RenderFeature.h"
#include "Rendering/RenderFeature/GTAO_RenderFeature.h"

#include "Core/Logic/Component/Base/CameraBase.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"

#include <glm/glm.hpp>            

RTTR_REGISTRATION
{
	rttr::registration::class_<HBAOVisualizationRenderer>("HBAOVisualizationRenderer").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<HBAOVisualizationRenderer::HBAOVisualizationRendererData>("HBAOVisualizationRenderer::HBAOVisualizationRendererData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

HBAOVisualizationRenderer::HBAOVisualizationRenderer()
	: RendererBase()
{

	pushRenderFeature("Geometry_RenderFeature", new Geometry_RenderFeature());
	pushRenderFeature("ClearColorAttachment_RenderFeature", new ClearColorAttachment_RenderFeature());

	pushRenderFeature("HBAO_RenderFeature", new HBAO_RenderFeature());
	pushRenderFeature("HBAO_Blur_RenderFeature", new AO_Blur_RenderFeature());
	pushRenderFeature("HBAO_Cover_RenderFeature", new AO_Cover_RenderFeature());

	pushRenderFeature("Present_RenderFeature", new Present_RenderFeature());

}

HBAOVisualizationRenderer::~HBAOVisualizationRenderer()
{
	delete static_cast<HBAO_RenderFeature*>(getRenderFeature("HBAO_RenderFeature"));
	delete static_cast<AO_Blur_RenderFeature*>(getRenderFeature("HBAO_Blur_RenderFeature"));
	delete static_cast<AO_Cover_RenderFeature*>(getRenderFeature("HBAO_Cover_RenderFeature"));

	delete static_cast<Geometry_RenderFeature*>(getRenderFeature("Geometry_RenderFeature"));
	delete static_cast<Present_RenderFeature*>(getRenderFeature("Present_RenderFeature"));
}

HBAOVisualizationRenderer::HBAOVisualizationRendererData::HBAOVisualizationRendererData()
	: RendererDataBase()
{
}

HBAOVisualizationRenderer::HBAOVisualizationRendererData::~HBAOVisualizationRendererData()
{
}

RendererDataBase* HBAOVisualizationRenderer::onCreateRendererData(CameraBase* camera)
{
	return new HBAOVisualizationRendererData();
}

void HBAOVisualizationRenderer::onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)
{
	auto geometryFeatureData = static_cast<Geometry_RenderFeature::Geometry_RenderFeatureData*>(rendererData->getRenderFeatureData("Geometry_RenderFeature"));

	///HBAO
	{
		auto hbaoFeatureData = static_cast<HBAO_RenderFeature::HBAO_RenderFeatureData*>(rendererData->getRenderFeatureData("HBAO_RenderFeature"));
		hbaoFeatureData->depthTexture = geometryFeatureData->depthTexture;
		hbaoFeatureData->normalTexture = geometryFeatureData->normalTexture;
		hbaoFeatureData->positionTexture = geometryFeatureData->positionTexture;

		auto aoBlurFeatureData = static_cast<AO_Blur_RenderFeature::AO_Blur_RenderFeatureData*>(rendererData->getRenderFeatureData("HBAO_Blur_RenderFeature"));
		aoBlurFeatureData->normalTexture = geometryFeatureData->normalTexture;
		aoBlurFeatureData->occlusionTexture = hbaoFeatureData->occlusionTexture;
		aoBlurFeatureData->iterateCount = 2;

		auto aoCoverFeatureData = static_cast<AO_Cover_RenderFeature::AO_Cover_RenderFeatureData*>(rendererData->getRenderFeatureData("HBAO_Cover_RenderFeature"));
		aoCoverFeatureData->occlusionTexture = hbaoFeatureData->occlusionTexture;
		aoCoverFeatureData->intensity = 2.5f;
	}

}

void HBAOVisualizationRenderer::onDestroyRendererData(RendererDataBase* rendererData)
{
	delete static_cast<HBAOVisualizationRendererData*>(rendererData);
}
