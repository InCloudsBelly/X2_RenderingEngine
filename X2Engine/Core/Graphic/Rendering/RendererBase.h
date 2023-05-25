#pragma once
#include "Core/Logic/Object/Object.h"
#include <map>
//#include <future>

class CameraBase;

class Renderer;

class CommandBuffer;

class RenderFeatureBase;
class RenderFeatureDataBase;
class RendererBase;

class RendererDataBase : public Object
{
	friend class RendererBase;
private:
	struct RenderFeatureWrapper
	{
		RenderFeatureDataBase* renderFeatureData;
		CommandBuffer* commandBuffer;
	};
	std::map<std::string, RenderFeatureWrapper> m_renderFeatureWrappers;
protected:
	RendererDataBase();
	virtual ~RendererDataBase();
	RendererDataBase(const RendererDataBase&) = delete;
	RendererDataBase& operator=(const RendererDataBase&) = delete;
	RendererDataBase(RendererDataBase&&) = delete;
	RendererDataBase& operator=(RendererDataBase&&) = delete;
public:
	RenderFeatureDataBase* getRenderFeatureData(std::string renderFeatureName);
	template<typename TRenderFeatureData>
	TRenderFeatureData* getRenderFeatureData(std::string renderFeatureName);

	RTTR_ENABLE(Object)
};

class RendererBase : public Object
{
	friend class RenderPipelineBase;
private:
	std::map<std::string, RenderFeatureBase*> m_renderFeatures;
	std::vector<std::string> m_renderFeatureUseOrder;
	
	bool m_preliminaryRenderFeaturesFinished;
	std::vector<std::string> m_preliminaryRenderFeatures;

protected:
	RendererBase();
	virtual ~RendererBase();
	RendererBase(const RendererBase&) = delete;
	RendererBase& operator=(const RendererBase&) = delete;
	RendererBase(RendererBase&&) = delete;
	RendererBase& operator=(RendererBase&&) = delete;

	virtual RendererDataBase* onCreateRendererData(CameraBase* camera) = 0;
	virtual void onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera) = 0;
	virtual void onDestroyRendererData(RendererDataBase* rendererData) = 0;

	void pushPreliminaryRenderFeature(std::string renderFeatureName, RenderFeatureBase* renderFeature);
	void pushRenderFeature(std::string renderFeatureName, RenderFeatureBase* renderFeature);
	void prepareRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData);
	void excuteRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData, CameraBase* camera, std::vector<Renderer*>const* rendererComponents);
	void submitRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData);
	void finishRenderFeature(std::string renderFeatureName, RendererDataBase* rendererData);

public:
	RendererDataBase* createRendererData(CameraBase* camera);
	void destroyRendererData(RendererDataBase* rendererData);

	virtual void prepareRenderer(RendererDataBase* rendererData);
	virtual void excuteRenderer(RendererDataBase* rendererData, CameraBase* camera, std::vector<Renderer*>const* rendererComponents);
	virtual void submitRenderer(RendererDataBase* rendererData);
	virtual void finishRenderer(RendererDataBase* rendererData);
	RenderFeatureBase* getRenderFeature(std::string renderFeatureName);

	RTTR_ENABLE(Object)
};