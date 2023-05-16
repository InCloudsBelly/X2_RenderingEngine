#pragma once
#include "Core/Graphic/Rendering/RenderFeatureBase.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/Image.h"
#include <glm/vec2.hpp>
#include <array>


class Mesh;

class Image;
class ImageSampler;
		
class Material;
class Shader;
class FrameBuffer;
		
	
class PrefilteredIrradiance_RenderFeature final : public RenderFeatureBase
{
public:
	class PrefilteredIrradiance_RenderPass final : public RenderPassBase
	{
	private:
		void populateRenderPassSettings(RenderPassSettings& settings)override;
	public:
		CONSTRUCTOR(PrefilteredIrradiance_RenderPass)
			RTTR_ENABLE(RenderPassBase)
	};

	class PrefilteredIrradiance_RenderFeatureData final : public RenderFeatureDataBase
	{
		friend class PrefilteredIrradiance_RenderFeature;
	private:
		std::array< std::vector<FrameBuffer*>, 6> m_frameBuffers;
		Shader* m_generateShader;
		Material* m_generateMaterial;
		
		ImageSampler* m_environmentImageSampler;
		Mesh* m_boxMesh;
		uint32_t m_sliceIndex;
	public:
		VkExtent2D resolution;
		uint32_t  mipLevel ;
		uint32_t sliceCount;
		std::string environmentImagePath;
		Image* m_targetCubeImage;

		CONSTRUCTOR(PrefilteredIrradiance_RenderFeatureData)
			RTTR_ENABLE(RenderFeatureDataBase)
	};

	CONSTRUCTOR(PrefilteredIrradiance_RenderFeature)

private:
	RenderPassBase* m_renderPass;

	RenderFeatureDataBase* createRenderFeatureData(CameraBase* camera)override;
	void resolveRenderFeatureData(RenderFeatureDataBase* renderFeatureData, CameraBase* camera)override;
	void destroyRenderFeatureData(RenderFeatureDataBase* renderFeatureData)override;

	void prepare(RenderFeatureDataBase* renderFeatureData)override;
	void excute(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer, CameraBase* camera, std::vector<Renderer*> const* rendererComponents) override;
	void submit(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;
	void finish(RenderFeatureDataBase* renderFeatureData, CommandBuffer* commandBuffer)override;

	RTTR_ENABLE(RenderFeatureBase)
};
	