#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"

class Geometry_RenderFeature final : public RenderFeatureBase
{
public:
	class Geometry_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(Geometry_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class Geometry_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class Geometry_RenderFeature;
	private:
		FrameBuffer* frameBuffer;
	public:
		Image* depthTexture;
		Image* normalTexture;

		CONSTRUCTOR(Geometry_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(Geometry_RenderFeature)

private:
	RenderPassBase* m_geometryRenderPass;
	std::string m_geometryRenderPassName;

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};