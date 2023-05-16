#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/vec2.hpp>

class Semaphore;
class ImageSampler;
class Renderer;

class Present_RenderFeature final : public RenderFeatureBase
{
public:
	class Present_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(Present_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class Present_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class Present_RenderFeature;
	private:
		std::vector<FrameBuffer*> frameBuffers;
		Buffer* attachmentSizeInfoBuffer;

		std::vector<Semaphore*>            m_imageAvailableSemaphores;
		std::vector<Semaphore*>            m_renderFinishedSemaphores;

		ImageSampler* m_sampler;

	public:
		CONSTRUCTOR(Present_RenderFeatureData)
			
	private:
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(Present_RenderFeature)

private:
	struct AttachmentSizeInfo
	{
		alignas(8) glm::vec2 size;
		alignas(8) glm::vec2 texelSize;
	};
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