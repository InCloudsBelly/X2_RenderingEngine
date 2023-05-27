#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/glm.hpp>

#define NOISE_COUNT 4096
#define SAMPLE_POINT_COUNT 32

class Mesh;
class Shader;
class Material;
class ImageSampler;

class SSAO_RenderFeature final : public RenderFeatureBase
{
public:
	class SSAO_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(SSAO_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class SSAO_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class SSAO_RenderFeature;
	private:
		Material* material;
		FrameBuffer* frameBuffer;
		Buffer* ssaoInfoBuffer;
		Buffer* samplePointInfoBuffer;
		Image* noiseTexture;
		Buffer* noiseStagingBuffer;
	public:
		float samplePointRadius;
		float samplePointBiasAngle;
		int noiseTextureWidth;
		Image* occlusionTexture;
		Image* depthTexture;
		Image* normalTexture;
		Image* positionTexture;

		CONSTRUCTOR(SSAO_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(SSAO_RenderFeature)

private:
	struct SSAOInfo
	{
		alignas(8) glm::vec2 attachmentSize;
		alignas(8) glm::vec2 attachmentTexelSize;
		alignas(4) int noiseTextureWidth;
	};
	RenderPassBase* m_renderPass;
	Mesh* m_fullScreenMesh;
	Shader* m_ssaoShader;
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