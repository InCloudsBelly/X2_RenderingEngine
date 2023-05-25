#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"

#include <glm/vec2.hpp>

class Semaphore;
class ImageSampler;
class Renderer;
class Buffer;

enum class ShadowType
{
	CSM = 0,
	CASCADED_EVSM,
};

class SimpleForward_RenderFeature final : public RenderFeatureBase
{
public:
	class SimpleForward_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(SimpleForward_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class SimpleForward_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class SimpleForward_RenderFeature;

	private:
		FrameBuffer* frameBuffer;
		ImageSampler* m_sampler;
	public:
		bool needClearDepthAttachment;
		Image* defaultAlbedoTexture;

		ShadowType shadowType = ShadowType::CASCADED_EVSM;

		RenderFeatureDataBase* shadowFeatureData;

		CONSTRUCTOR(SimpleForward_RenderFeatureData)
		RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(SimpleForward_RenderFeature)

private:
	RenderPassBase* m_renderPass;
	std::string m_renderPassName;
	

	static int currentFrame;
	static int swapchainFrameIndex;

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};