#pragma once

#include "Core/Graphic/Rendering/RendererBase.h"

class Renderer;


class ForwardRenderer final : public RendererBase
{
public:
	class ForwardRendererData final : public RendererDataBase
	{
	public:
		CONSTRUCTOR(ForwardRendererData)

			RTTR_ENABLE(RendererDataBase)
	};

	CONSTRUCTOR(ForwardRenderer)

private:
	RendererDataBase* onCreateRendererData(CameraBase* camera)override;
	void onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)override;
	void onDestroyRendererData(RendererDataBase* rendererData)override;

	//virtual void prepareRenderer(RendererDataBase* rendererData) override;
	//virtual void excuteRenderer(RendererDataBase* rendererData, CameraBase* camera, std::vector<Renderer*>const* rendererComponents) override;
	//virtual void submitRenderer(RendererDataBase* rendererData) override;
	//virtual void finishRenderer(RendererDataBase* rendererData) override;

	RTTR_ENABLE(RendererBase)
};