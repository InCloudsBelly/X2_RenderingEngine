#pragma once
#include "Core/Logic/Object/Object.h"
#include <vector>

class Renderer;

class CameraBase;

class CommandBuffer;

class FrameBuffer;
class RendererBase;
class RendererDataBase;
class RenderFeatureDataBase : public Object
{
protected:
	RenderFeatureDataBase();
	virtual ~RenderFeatureDataBase();
	RenderFeatureDataBase(const RenderFeatureDataBase&) = delete;
	RenderFeatureDataBase& operator=(const RenderFeatureDataBase&) = delete;
	RenderFeatureDataBase(RenderFeatureDataBase&&) = delete;
	RenderFeatureDataBase& operator=(RenderFeatureDataBase&&) = delete;

	RTTR_ENABLE(Object)
};

class RenderFeatureBase : public Object
{
	friend class RendererBase;
protected:
	RenderFeatureBase();
	virtual ~RenderFeatureBase();
	RenderFeatureBase(const RenderFeatureBase&) = delete;
	RenderFeatureBase& operator=(const RenderFeatureBase&) = delete;
	RenderFeatureBase(RenderFeatureBase&&) = delete;
	RenderFeatureBase& operator=(RenderFeatureBase&&) = delete;

	virtual RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera) = 0;
	virtual void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData,CameraBase* camera) = 0;
	virtual void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData) = 0;

	virtual void prepare(RenderFeatureDataBase* renderFeatureData) = 0;
	virtual void excute(RenderFeatureDataBase* renderFeatureData,CommandBuffer* commandBuffer,CameraBase* camera, std::vector<Renderer*>const* rendererComponents) = 0;
	virtual void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer) = 0;
	virtual void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer) = 0;

	RTTR_ENABLE(Object)
};