#pragma once
#include "Core/Logic/Object/Object.h"
#include <map>

class RendererBase;
class RenderPipelineBase : public Object
{
	friend class RenderPipelineManager;
private:
	std::map<std::string, RendererBase*> g_renderers;
protected:
	RenderPipelineBase();
	virtual ~RenderPipelineBase();
	RenderPipelineBase(const RenderPipelineBase&) = delete;
	RenderPipelineBase& operator=(const RenderPipelineBase&) = delete;
	RenderPipelineBase(RenderPipelineBase&&) = delete;
	RenderPipelineBase& operator=(RenderPipelineBase&&) = delete;

	void useRenderer(std::string rendererName, RendererBase* renderer);
public:
	RendererBase* getRenderer(std::string rendererName);
	void destroyRenderer(std::string rendererName);

	RTTR_ENABLE(Object)
};