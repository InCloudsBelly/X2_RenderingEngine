#include "RenderPipelineManager.h"
//#include "Utils/Log.h"
#include "Core/Graphic/Rendering/RendererBase.h"
#include "Core/Graphic/Rendering/RenderPipelineBase.h"
#include "Core/Graphic/Rendering/RendererBase.h"
#include "Core/Logic/Component/Base/CameraBase.h"

RenderPipelineManager::RenderPipelineManager()
	: m_renderPipeline(nullptr)
	, m_rendererDatas()
	//, _managerMutex()
{
}

RenderPipelineManager::~RenderPipelineManager()
{
	delete m_renderPipeline;
}

RenderPipelineBase* RenderPipelineManager::switchRenderPipeline(RenderPipelineBase* renderPipeline)
{
	//std::lock_guard<std::mutex> locker(_managerMutex);

	for (auto& rendererDataPair : m_rendererDatas)
	{
		m_renderPipeline->getRenderer(rendererDataPair.first->getRendererName())->destroyRendererData(rendererDataPair.second);
	}

	auto oldRenderPipeline = m_renderPipeline;
	m_renderPipeline = renderPipeline;

	for (auto& rendererDataPair : m_rendererDatas)
	{
		rendererDataPair.second = m_renderPipeline->getRenderer(rendererDataPair.first->getRendererName())->createRendererData(rendererDataPair.first);
	}

	return oldRenderPipeline;
}

void RenderPipelineManager::createRendererData(CameraBase* camera)
{
	if (camera == nullptr) return;

	//std::lock_guard<std::mutex> locker(_managerMutex);

	if (m_rendererDatas.find(camera) != std::end(m_rendererDatas))
		throw(std::runtime_error("Camera already has a renderer data."));

	auto rendererData = m_renderPipeline->getRenderer(camera->getRendererName())->createRendererData(camera);

	m_rendererDatas[camera] = rendererData;
}

void RenderPipelineManager::refreshRendererData(CameraBase* camera)
{
	if (camera == nullptr) return;

	//std::lock_guard<std::mutex> locker(_managerMutex);

	if (m_rendererDatas.find(camera) == std::end(m_rendererDatas))
		throw(std::runtime_error("Camera does not have a renderer data."));

	m_renderPipeline->getRenderer(camera->getRendererName())->destroyRendererData(m_rendererDatas[camera]);
	m_rendererDatas[camera] = m_renderPipeline->getRenderer(camera->getRendererName())->createRendererData(camera);
}

void RenderPipelineManager::destroyRendererData(CameraBase* camera)
{
	if (camera == nullptr) return;

	//std::lock_guard<std::mutex> locker(_managerMutex);

	if (m_rendererDatas.find(camera) == std::end(m_rendererDatas))
		throw(std::runtime_error("Camera does not have a renderer data."));

	m_renderPipeline->getRenderer(camera->getRendererName())->destroyRendererData(m_rendererDatas[camera]);
	m_rendererDatas.erase(camera);
}

void RenderPipelineManager::destroyRenderer(CameraBase* camera)
{
}

RenderPipelineBase* RenderPipelineManager::getRenderPipeline()
{
	//std::lock_guard<std::mutex> locker(_managerMutex);

	return m_renderPipeline;
}

RendererBase* RenderPipelineManager::getRenderer(std::string rendererName)
{
	//std::lock_guard<std::mutex> locker(_managerMutex);

	return m_renderPipeline->getRenderer(rendererName);
}

RendererDataBase* RenderPipelineManager::getRendererData(CameraBase* camera)
{
	if (camera == nullptr) return nullptr;

	//std::lock_guard<std::mutex> locker(_managerMutex);

	if (m_rendererDatas.find(camera) == std::end(m_rendererDatas))
		throw(std::runtime_error("Camera does not have a renderer data."));

	return m_rendererDatas[camera];
}
