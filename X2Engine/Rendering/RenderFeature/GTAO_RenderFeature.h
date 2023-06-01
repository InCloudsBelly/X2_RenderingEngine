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

class GTAO_RenderFeature final : public RenderFeatureBase
{
public:
	class GTAO_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(GTAO_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class GTAO_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class GTAO_RenderFeature;
	private:
		Material* material;
		FrameBuffer* frameBuffer;
		Buffer* gtaoInfoBuffer;
		Image* noiseTexture;
		Buffer* noiseStagingBuffer;
	public:
		float sampleRadius;
		float sampleBiasAngle;
		int stepCount;
		int sliceCount;
		int noiseTextureWidth;
		Image* occlusionTexture;
		Image* depthTexture;
		Image* normalTexture;
		Image* positionTexture;

		CONSTRUCTOR(GTAO_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(GTAO_RenderFeature)

private:
	struct GTAOInfo
	{
		alignas(8) glm::vec2 attachmentize;
		alignas(8) glm::vec2 attachmentTexelSize;
		alignas(4) float sampleRadius;
		alignas(4) float sampleBiasAngle;
		alignas(4) int stepCount;
		alignas(4) int sliceCount;
		alignas(4) int noiseTextureWidth;
	};
	RenderPassBase* m_renderPass;
	Mesh* m_fullScreenMesh;
	Shader* m_gtaoShader;
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