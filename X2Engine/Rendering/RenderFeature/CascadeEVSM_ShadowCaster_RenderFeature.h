#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <array>
#include <glm/glm.hpp>

#define CASCADE_COUNT 4

class Mesh;

class Material;
class Shader;

class ImageSampler;

class CascadeEVSM_ShadowCaster_RenderFeature final : public RenderFeatureBase
{
public:
	class CascadeEVSM_ShadowCaster_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(CascadeEVSM_ShadowCaster_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};
	class CascadeEVSM_Blit_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(CascadeEVSM_Blit_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};
	class CascadeEVSM_Blur_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(CascadeEVSM_Blur_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class CascadeEVSM_ShadowCaster_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class CascadeEVSM_ShadowCaster_RenderFeature;
	private:
		RenderPassBase* shadowCasterRenderPass;
		RenderPassBase* blitRenderPass;
		RenderPassBase* blurRenderPass;
		ImageSampler* pointSampler;

		std::array<Image*, CASCADE_COUNT> depthAttachemnts;
		std::array<Image*, CASCADE_COUNT> shadowTextures;
		std::array<Image*, CASCADE_COUNT> temporaryShadowTextures;
		std::array<FrameBuffer*, CASCADE_COUNT> shadowCasterFrameBuffers;
		std::array<FrameBuffer*, CASCADE_COUNT> blitFrameBuffers;
		std::array<FrameBuffer*, CASCADE_COUNT * 2> blurFrameBuffers;
		std::array<Material*, CASCADE_COUNT> blitMaterials;
		std::array<Buffer*, CASCADE_COUNT> blitInfoBuffers;
		std::array<Material*, CASCADE_COUNT * 2> blurMaterials;
		std::array<Buffer*, CASCADE_COUNT * 2> blurInfoBuffers;

		Buffer* lightCameraInfoBuffer;
		Buffer* lightCameraInfoStagingBuffer;
		Buffer* cascadeEvsmShadowReceiverInfoBuffer;
	public:
		std::array<float, CASCADE_COUNT> frustumSegmentScales;
		std::array<float, CASCADE_COUNT> lightCameraCompensationDistances;
		std::array<uint32_t, CASCADE_COUNT> shadowImageResolutions;
		std::array<float, CASCADE_COUNT> blurOffsets;
		float overlapScale;
		float c1;
		float c2;
		float threshold;
		int iterateCount;

		void refresh();

		void setShadowReceiverMaterialParameters(Material* material);

		CONSTRUCTOR(CascadeEVSM_ShadowCaster_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	struct LightCameraInfo
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 viewProjection;
	};
	struct CascadeEvsmBlitInfo
	{
		alignas(8) glm::vec2 texelSize;
		alignas(4) float c1;
		alignas(4) float c2;
	};
	struct CascadeEvsmBlurInfo
	{
		alignas(8) glm::vec2 texelSize;
		alignas(8) glm::vec2 sampleOffset;
	};
	struct CascadeEvsmShadowReceiverInfo
	{
		alignas(16) glm::vec4 thresholdVZ[CASCADE_COUNT * 2];
		alignas(16) glm::vec3 wLightDirection;
		alignas(16) glm::vec3 vLightDirection;
		alignas(16) glm::mat4 matrixVC2PL[CASCADE_COUNT];
		alignas(16) glm::vec4 texelSize[CASCADE_COUNT];
		alignas(4) float c1;
		alignas(4) float c2;
		alignas(4) float threshold;
	};

	CONSTRUCTOR(CascadeEVSM_ShadowCaster_RenderFeature)

private:
	RenderPassBase* m_shadowCasterRenderPass;
	RenderPassBase* m_blitRenderPass;
	RenderPassBase* m_blurRenderPass;
	std::string m_shadowCasterRenderPassName;
	ImageSampler* m_pointSampler;
	Shader* m_blitShader;
	Shader* m_blurShader;
	Mesh* m_fullScreenMesh;

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};