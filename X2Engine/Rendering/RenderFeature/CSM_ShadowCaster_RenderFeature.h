#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <array>
#include <glm/glm.hpp>

#define CASCADE_COUNT 4

class Material;
class ImageSampler;
class Renderer;


class CSM_ShadowCaster_RenderFeature final : public RenderFeatureBase
{
public:
	class CSM_ShadowCaster_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(CSM_ShadowCaster_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class CSM_ShadowCaster_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class CSM_ShadowCaster_RenderFeature;
	private:
		RenderPassBase* shadowMapRenderPass;
		ImageSampler* sampler;

		Image* shadowImageArray;
		std::array<FrameBuffer*, CASCADE_COUNT> shadowFrameBuffers;
		Buffer* lightCameraInfoBuffer;
		Buffer* lightCameraInfoHostStagingBuffer;
		Buffer* csmShadowReceiverInfoBuffer;
	public:
		std::array<float, CASCADE_COUNT> frustumSegmentScales;
		std::array<float, CASCADE_COUNT> lightCameraCompensationDistances;
		uint32_t shadowImageResolutions;
		std::array <glm::vec2, CASCADE_COUNT> bias;
		float overlapScale;
		int sampleHalfWidth;

		void setShadowReceiverMaterialParameters(Material* material);

		CONSTRUCTOR(CSM_ShadowCaster_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	struct LightCameraInfo
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 viewProjection;
	};
	struct CsmShadowReceiverInfo
	{
		alignas(16) glm::vec4 thresholdVZ[CASCADE_COUNT * 2];
		alignas(16) glm::vec3 wLightDirection;
		alignas(16) glm::vec3 vLightDirection;
		alignas(16) glm::vec4 bias[CASCADE_COUNT];
		alignas(16) glm::mat4 matrixVC2PL[CASCADE_COUNT];
		alignas(16) glm::vec4 texelSize[CASCADE_COUNT];
		alignas(4) int sampleHalfWidth;
	};

	CONSTRUCTOR(CSM_ShadowCaster_RenderFeature)

private:
	RenderPassBase* m_shadowMapRenderPass;
	std::string m_shadowMapRenderPassName;
	ImageSampler* m_shadowImageSampler;

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};
