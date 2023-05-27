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


class AO_Cover_RenderFeature final : public RenderFeatureBase
{
public:
	class AO_Cover_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(AO_Cover_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class AO_Cover_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class AO_Cover_RenderFeature;
	private:
		Material* material;
		FrameBuffer* frameBuffer;
		Buffer* coverInfoBuffer;
	public:
		float intensity;
		Image* occlusionTexture;

		CONSTRUCTOR(AO_Cover_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(AO_Cover_RenderFeature)

private:
	struct CoverInfo
	{
		alignas(8) glm::vec2 size;
		alignas(8) glm::vec2 texelSize;
		alignas(4) float intensity;
	};
	RenderPassBase* m_renderPass;
	Mesh* m_fullScreenMesh;
	Shader* m_coverShader;
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