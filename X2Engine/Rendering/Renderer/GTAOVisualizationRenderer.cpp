#include "GTAOVisualizationRenderer.h"

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
	rttr::registration::class_<GTAOVisualizationRenderer>("GTAOVisualizationRenderer").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<GTAOVisualizationRenderer::GTAOVisualizationRendererData>("GTAOVisualizationRenderer::GTAOVisualizationRendererData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

GTAOVisualizationRenderer::GTAOVisualizationRenderer()
	: RendererBase()
{

	pushRenderFeature("Geometry_RenderFeature", new Geometry_RenderFeature());
	pushRenderFeature("ClearColorAttachment_RenderFeature", new ClearColorAttachment_RenderFeature());


	pushRenderFeature("GTAO_RenderFeature", new GTAO_RenderFeature());
	pushRenderFeature("GTAO_Blur_RenderFeature", new AO_Blur_RenderFeature());
	pushRenderFeature("GTAO_Cover_RenderFeature", new AO_Cover_RenderFeature());

	pushRenderFeature("Present_RenderFeature", new Present_RenderFeature());

}

GTAOVisualizationRenderer::~GTAOVisualizationRenderer()
{
	delete static_cast<GTAO_RenderFeature*>(getRenderFeature("GTAO_RenderFeature"));
	delete static_cast<AO_Blur_RenderFeature*>(getRenderFeature("GTAO_Blur_RenderFeature"));
	delete static_cast<AO_Cover_RenderFeature*>(getRenderFeature("GTAO_Cover_RenderFeature"));

	delete static_cast<Geometry_RenderFeature*>(getRenderFeature("Geometry_RenderFeature"));
	delete static_cast<Present_RenderFeature*>(getRenderFeature("Present_RenderFeature"));
}

GTAOVisualizationRenderer::GTAOVisualizationRendererData::GTAOVisualizationRendererData()
	: RendererDataBase()
{
}

GTAOVisualizationRenderer::GTAOVisualizationRendererData::~GTAOVisualizationRendererData()
{
}

RendererDataBase* GTAOVisualizationRenderer::onCreateRendererData(CameraBase* camera)
{
	return new GTAOVisualizationRendererData();
}

void GTAOVisualizationRenderer::onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)
{
	auto geometryFeatureData = static_cast<Geometry_RenderFeature::Geometry_RenderFeatureData*>(rendererData->getRenderFeatureData("Geometry_RenderFeature"));

	// GTAO
	{
		auto gtaoFeatureData = static_cast<GTAO_RenderFeature::GTAO_RenderFeatureData*>(rendererData->getRenderFeatureData("GTAO_RenderFeature"));
		gtaoFeatureData->depthTexture = geometryFeatureData->depthTexture;
		gtaoFeatureData->normalTexture = geometryFeatureData->normalTexture;
		gtaoFeatureData->positionTexture = geometryFeatureData->positionTexture;

		auto aoBlurFeatureData = static_cast<AO_Blur_RenderFeature::AO_Blur_RenderFeatureData*>(rendererData->getRenderFeatureData("GTAO_Blur_RenderFeature"));
		aoBlurFeatureData->normalTexture = geometryFeatureData->normalTexture;
		aoBlurFeatureData->occlusionTexture = gtaoFeatureData->occlusionTexture;
		aoBlurFeatureData->iterateCount = 2;

		auto aoCoverFeatureData = static_cast<AO_Cover_RenderFeature::AO_Cover_RenderFeatureData*>(rendererData->getRenderFeatureData("GTAO_Cover_RenderFeature"));
		aoCoverFeatureData->occlusionTexture = gtaoFeatureData->occlusionTexture;
		aoCoverFeatureData->intensity = 2.0f;
	}
}

void GTAOVisualizationRenderer::onDestroyRendererData(RendererDataBase* rendererData)
{
	delete static_cast<GTAOVisualizationRendererData*>(rendererData);
}
