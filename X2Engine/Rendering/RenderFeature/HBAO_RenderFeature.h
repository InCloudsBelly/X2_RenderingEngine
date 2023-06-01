#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/glm.hpp>

class Mesh;
class Shader;
class Material;
class ImageSampler;
class CameraBase;

class HBAO_RenderFeature final : public RenderFeatureBase
{
public:
	class HBAO_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(HBAO_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class HBAO_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class HBAO_RenderFeature;
	private:
		Material* material;
		FrameBuffer* frameBuffer;
		Buffer* hbaoInfoBuffer;
		Image* noiseTexture;
		Buffer* noiseStagingBuffer;
	public:
		float sampleRadius;
		float sampleBiasAngle;
		int stepCount;
		int directionCount;
		int noiseTextureWidth;
		Image* occlusionTexture;
		Image* depthTexture;
		Image* normalTexture;
		Image* positionTexture;

		CONSTRUCTOR(HBAO_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(HBAO_RenderFeature)

private:
	struct HbaoInfo
	{
		alignas(8) glm::vec2 attachmentSize;
		alignas(8) glm::vec2 attachmentTexelSize;
		alignas(4) float sampleRadius;
		alignas(4) float sampleBiasAngle;
		alignas(4) int stepCount;
		alignas(4) int directionCount;
		alignas(4) int noiseTextureWidth;
	};
	RenderPassBase* m_renderPass;
	Mesh* m_fullScreenMesh;
	Shader* m_hbaoShader;
	ImageSampler* m_textureSampler;

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*>const* rendererComponents)override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};