#pragma once

#include "Core/Graphic/Rendering/RendererBase.h"

class Renderer;


class GTAOVisualizationRenderer final : public RendererBase
{
public:
	class GTAOVisualizationRendererData final : public RendererDataBase
	{
	public:
		CONSTRUCTOR(GTAOVisualizationRendererData)

			RTTR_ENABLE(RendererDataBase)
	};

	CONSTRUCTOR(GTAOVisualizationRenderer)

private:
	RendererDataBase* onCreateRendererData(CameraBase* camera)override;
	void onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)override;
	void onDestroyRendererData(RendererDataBase* rendererData)override;

	RTTR_ENABLE(RendererBase)
};