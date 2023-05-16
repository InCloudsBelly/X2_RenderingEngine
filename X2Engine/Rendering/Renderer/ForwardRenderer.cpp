#include "ForwardRenderer.h"

#include "Rendering/RenderFeature/Background_RenderFeature.h"
#include "Rendering/RenderFeature/Present_RenderFeature.h"
#include "Rendering/RenderFeature/PrefilteredIrradiance_RenderFeature.h"
#include "Rendering/RenderFeature/PrefilteredEnvironmentMap_RenderFeature.h"


#include "Core/Logic/Component/Base/CameraBase.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"

#include <glm/glm.hpp>

RTTR_REGISTRATION
{
	rttr::registration::class_<ForwardRenderer>("ForwardRenderer").constructor<>()(rttr::policy::ctor::as_raw_ptr);
	rttr::registration::class_<ForwardRenderer::ForwardRendererData>("ForwardRenderer::ForwardRendererData").constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

ForwardRenderer::ForwardRenderer()
	: RendererBase()
{
	pushPreliminaryRenderFeature("PrefilteredIrradiance_RenderFeature", new PrefilteredIrradiance_RenderFeature());
	pushPreliminaryRenderFeature("PrefilteredEnvironmentMap_RenderFeature", new PrefilteredEnvironmentMap_RenderFeature());


	pushRenderFeature("Background_RenderFeature", new Background_RenderFeature());
	pushRenderFeature("Present_RenderFeature", new Present_RenderFeature());
}

ForwardRenderer::~ForwardRenderer()
{
	delete static_cast<PrefilteredIrradiance_RenderFeature*>(getRenderFeature("PrefilteredIrradiance_RenderFeature"));
	delete static_cast<PrefilteredEnvironmentMap_RenderFeature*>(getRenderFeature("PrefilteredEnvironmentMap_RenderFeature"));

	delete static_cast<Background_RenderFeature*>(getRenderFeature("Background_RenderFeature"));
	delete static_cast<Present_RenderFeature*>(getRenderFeature("Present_RenderFeature"));
}

ForwardRenderer::ForwardRendererData::ForwardRendererData()
	: RendererDataBase()
{
}

ForwardRenderer::ForwardRendererData::~ForwardRendererData()
{
}

RendererDataBase* ForwardRenderer::onCreateRendererData(CameraBase* camera)
{
	return new ForwardRendererData();
}

void ForwardRenderer::onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)
{
	auto prefilteredIrradianceData = static_cast<PrefilteredIrradiance_RenderFeature::PrefilteredIrradiance_RenderFeatureData*>(rendererData->getRenderFeatureData("PrefilteredIrradiance_RenderFeature"));
	auto prefilteredEnvData = static_cast<PrefilteredEnvironmentMap_RenderFeature::PrefilteredEnvironmentMap_RenderFeatureData*>(rendererData->getRenderFeatureData("PrefilteredEnvironmentMap_RenderFeature"));

	auto backgroundData = static_cast<Background_RenderFeature::Background_RenderFeatureData*>(rendererData->getRenderFeatureData("Background_RenderFeature"));
	backgroundData->needClearColorAttachment = true;
	//backgroundData->backgroundImage = Instance::g_backgroundImage;
	//backgroundData->backgroundImage = prefilteredIrradianceData->m_targetCubeImage;
	//backgroundData->backgroundImage = prefilteredEnvData->m_targetCubeImage;



}

void ForwardRenderer::onDestroyRendererData(RendererDataBase* rendererData)
{
	delete static_cast<ForwardRendererData*>(rendererData);
}
