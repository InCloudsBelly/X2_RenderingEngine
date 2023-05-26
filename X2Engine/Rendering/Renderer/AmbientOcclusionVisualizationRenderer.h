#pragma once

#include "Core/Graphic/Rendering/RendererBase.h"

class Renderer;


class AmbientOcclusionVisualizationRenderer final : public RendererBase
{
public:
	class AmbientOcclusionVisualizationRendererData final : public RendererDataBase
	{
	public:
		CONSTRUCTOR(AmbientOcclusionVisualizationRendererData)

			RTTR_ENABLE(RendererDataBase)
	};

	CONSTRUCTOR(AmbientOcclusionVisualizationRenderer)

private:
	RendererDataBase* onCreateRendererData(CameraBase* camera)override;
	void onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)override;
	void onDestroyRendererData(RendererDataBase* rendererData)override;

	RTTR_ENABLE(RendererBase)
};