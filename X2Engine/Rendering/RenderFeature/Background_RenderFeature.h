#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/vec2.hpp>


class Background_RenderFeature final : public RenderFeatureBase
{
public:
	class Background_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(Background_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class Background_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class Background_RenderFeature;
	private:
		FrameBuffer* frameBuffer;
		Buffer* attachmentSizeInfoBuffer;

	public:
		bool needClearColorAttachment;
		Image* backgroundImage;

		CONSTRUCTOR(Background_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(Background_RenderFeature)

private:
	struct AttachmentSizeInfo
	{
		alignas(8) glm::vec2 size;
		alignas(8) glm::vec2 texelSize;
	};
	RenderPassBase* m_renderPass;
	std::string m_renderPassName;


	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};