#include "RenderPipelineBase.h"
#include "Core/Graphic/Rendering/RendererBase.h"
//#include "Utils/Log.h"

RTTR_REGISTRATION
{
	rttr::registration::class_<RenderPipelineBase>("RenderPipelineBase");
}

RenderPipelineBase::RenderPipelineBase()
{
}

RenderPipelineBase::~RenderPipelineBase()
{
}

void RenderPipelineBase::useRenderer(std::string rendererName, RendererBase* renderer)
{
	if (g_renderers.count(rendererName) != 0)
		throw(std::runtime_error("Already contains renderer named: " + rendererName + "."));

	g_renderers[rendererName] = renderer;
}

RendererBase* RenderPipelineBase::getRenderer(std::string rendererName)
{
	return g_renderers[rendererName];
}


