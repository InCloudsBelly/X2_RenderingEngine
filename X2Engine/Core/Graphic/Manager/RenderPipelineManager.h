#pragma once
#include <map>
#include <mutex>

class CameraBase;


class RenderPipelineBase;
class RendererBase;
class RendererDataBase;

class RenderPipelineManager final
{
public:
	RenderPipelineManager();
	~RenderPipelineManager();
	RenderPipelineManager(const RenderPipelineManager&) = delete;
	RenderPipelineManager& operator=(const RenderPipelineManager&) = delete;
	RenderPipelineManager(RenderPipelineManager&&) = delete;
	RenderPipelineManager& operator=(RenderPipelineManager&&) = delete;

	RenderPipelineBase* switchRenderPipeline(RenderPipelineBase* renderPipeline);
	void createRendererData(CameraBase* camera);
	void refreshRendererData(CameraBase* camera);
	void destroyRendererData(CameraBase* camera);
	RenderPipelineBase* getRenderPipeline();
	RendererBase* getRenderer(std::string rendererName);
	RendererDataBase* getRendererData(CameraBase* camera);

private:
	//std::mutex _managerMutex;
	RenderPipelineBase* m_renderPipeline;
	std::map<CameraBase*, RendererDataBase*> m_rendererDatas;
};