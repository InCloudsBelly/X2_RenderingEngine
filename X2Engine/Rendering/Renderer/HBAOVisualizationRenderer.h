#pragma once

#include "Core/Graphic/Rendering/RendererBase.h"

class Renderer;


class HBAOVisualizationRenderer final : public RendererBase
{
public:
	class HBAOVisualizationRendererData final : public RendererDataBase
	{
	public:
		CONSTRUCTOR(HBAOVisualizationRendererData)

			RTTR_ENABLE(RendererDataBase)
	};

	CONSTRUCTOR(HBAOVisualizationRenderer)

private:
	RendererDataBase* onCreateRendererData(CameraBase* camera)override;
	void onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)override;
	void onDestroyRendererData(RendererDataBase* rendererData)override;

	RTTR_ENABLE(RendererBase)
};