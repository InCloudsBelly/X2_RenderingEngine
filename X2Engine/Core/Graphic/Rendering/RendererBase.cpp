#include "RendererBase.h"
//#include "Utils/Log.h"
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
//#include "Core/Graphic/CoreObject/Thread.h"
#include "Core/Graphic/Command/CommandPool.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Logic/Component/Base/Renderer.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<RendererBase>("RendererBase");
	rttr::registration::class_<RendererDataBase>("RendererDataBase");
}

RendererDataBase::RendererDataBase()
	: m_renderFeatureWrappers()
{
}

RendererDataBase::~RendererDataBase()
{
}

RenderFeatureDataBase* RendererDataBase::getRenderFeatureData(std::string renderFeatureName)
{
	return m_renderFeatureWrappers[renderFeatureName].renderFeatureData;
}

RendererBase::RendererBase()
	: m_renderFeatures()
	, m_preliminaryRenderFeaturesFinished(false)
{
}

RendererBase::~RendererBase()
{
}

void RendererBase::pushPreliminaryRenderFeature(std::string renderFeatureName, RenderFeatureBase* renderFeature)
{
	if (m_renderFeatures.count(renderFeatureName) != 0)
		throw(std::runtime_error("Already contains render feature named: " + renderFeatureName + "."));

	m_renderFeatures[renderFeatureName] = renderFeature;
	m_preliminaryRenderFeatures.push_back(renderFeatureName);
}

void RendererBase::pushRenderFeature(std::string renderFeatureName, RenderFeatureBase* renderFeature)
{
	if (m_renderFeatures.count(renderFeatureName) != 0)
		throw(std::runtime_error("Already contains render feature named: " + renderFeatureName + "."));

	m_renderFeatures[renderFeatureName] = renderFeature;
	m_renderFeatureUseOrder.push_back(renderFeatureName);
}

void RendererBase::prepareRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData)
{
	m_renderFeatures[renderFeatureName]->prepare(rendererData->getRenderFeatureData(renderFeatureName));
}

void RendererBase::excuteRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	RendererDataBase::RenderFeatureWrapper* wrapper = &rendererData->m_renderFeatureWrappers[renderFeatureName];
	auto renderFeature = m_renderFeatures[renderFeatureName];


	{
		wrapper->commandBuffer = Instance::getGraphicCommandPool()->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		renderFeature->excute(wrapper->renderFeatureData, wrapper->commandBuffer, camera, rendererComponents);
	}

	//wrapper->excuteTask.get();
}

void RendererBase::submitRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData)
{
	RendererDataBase::RenderFeatureWrapper* wrapper = &rendererData->m_renderFeatureWrappers[renderFeatureName];
	m_renderFeatures[renderFeatureName]->submit(wrapper->renderFeatureData, wrapper->commandBuffer);
}

void RendererBase::finishRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData)
{
	RendererDataBase::RenderFeatureWrapper* wrapper = &rendererData->m_renderFeatureWrappers[renderFeatureName];
	m_renderFeatures[renderFeatureName]->finish(wrapper->renderFeatureData, wrapper->commandBuffer);
	wrapper->commandBuffer->ParentCommandPool()->destoryCommandBuffer(wrapper->commandBuffer);
}

RendererDataBase* RendererBase::createRendererData(CameraBase* camera)
{
	RendererDataBase* rendererData = onCreateRendererData(camera);
	for (const auto& renderFeatureName : m_renderFeatureUseOrder)
		rendererData->m_renderFeatureWrappers[renderFeatureName].renderFeatureData = m_renderFeatures[renderFeatureName]->createRenderFeatureData(camera);
	for (const auto& renderFeatureName : m_preliminaryRenderFeatures)
		rendererData->m_renderFeatureWrappers[renderFeatureName].renderFeatureData = m_renderFeatures[renderFeatureName]->createRenderFeatureData(camera);

	onResolveRendererData(rendererData, camera);

	for (const auto& renderFeatureName : m_renderFeatureUseOrder)
		m_renderFeatures[renderFeatureName]->resolveRenderFeatureData(rendererData->m_renderFeatureWrappers[renderFeatureName].renderFeatureData, camera);
	for (const auto& renderFeatureName : m_preliminaryRenderFeatures)
		m_renderFeatures[renderFeatureName]->resolveRenderFeatureData(rendererData->m_renderFeatureWrappers[renderFeatureName].renderFeatureData, camera);

	return rendererData;
}

void RendererBase::destroyRendererData(RendererDataBase* rendererData)
{
	for (const auto& renderFeaturePair : m_renderFeatures)
	{
		renderFeaturePair.second->destroyRenderFeatureData(rendererData->m_renderFeatureWrappers[renderFeaturePair.first].renderFeatureData);
	}
	onDestroyRendererData(rendererData);
}

void RendererBase::prepareRenderer(RendererDataBase* rendererData)
{
	if (!m_preliminaryRenderFeaturesFinished)
	{
		for (auto featureName : m_preliminaryRenderFeatures)
			prepareRenderFeature(featureName, rendererData);
	}

	for (auto featureName : m_renderFeatureUseOrder)
		prepareRenderFeature(featureName, rendererData);
}

void RendererBase::excuteRenderer(RendererDataBase* rendererData, CameraBase* camera, std::vector<Renderer*> const* rendererComponents)
{
	if (!m_preliminaryRenderFeaturesFinished)
	{
		for (auto featureName : m_preliminaryRenderFeatures)
			excuteRenderFeature(featureName, rendererData, camera, rendererComponents);
	}

	for (auto featureName : m_renderFeatureUseOrder)
		excuteRenderFeature(featureName, rendererData, camera, rendererComponents);
}

void RendererBase::submitRenderer(RendererDataBase* rendererData)
{
	if (!m_preliminaryRenderFeaturesFinished)
	{
		for (auto featureName : m_preliminaryRenderFeatures)
			submitRenderFeature(featureName, rendererData);
	}

	for (auto featureName : m_renderFeatureUseOrder)
		submitRenderFeature(featureName, rendererData);
}

void RendererBase::finishRenderer(RendererDataBase* rendererData)
{
	if (!m_preliminaryRenderFeaturesFinished)
	{
		for (auto featureName : m_preliminaryRenderFeatures)
			finishRenderFeature(featureName, rendererData);

		m_preliminaryRenderFeaturesFinished = true;
	}

	for (auto featureName : m_renderFeatureUseOrder)
		finishRenderFeature(featureName, rendererData);
}

RenderFeatureBase* RendererBase::getRenderFeature(std::string renderFeatureName)
{
	return m_renderFeatures[renderFeatureName];
}
