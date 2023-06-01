#pragma once

#include "Core/Graphic/Rendering/RendererBase.h"

class Renderer;


class SSAOVisualizationRenderer final : public RendererBase
{
public:
	class SSAOVisualizationRendererData final : public RendererDataBase
	{
	public:
		CONSTRUCTOR(SSAOVisualizationRendererData)

			RTTR_ENABLE(RendererDataBase)
	};

	CONSTRUCTOR(SSAOVisualizationRenderer)

private:
	RendererDataBase* onCreateRendererData(CameraBase* camera)override;
	void onResolveRendererData(RendererDataBase* rendererData, CameraBase* camera)override;
	void onDestroyRendererData(RendererDataBase* rendererData)override;

	RTTR_ENABLE(RendererBase)
};