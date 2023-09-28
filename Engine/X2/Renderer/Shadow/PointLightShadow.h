#ifndef X2_POINTLIGHTSHADOW
#define X2_POINTLIGHTSHADOW

#include "X2/Renderer/Renderer.h"
#include "X2/Vulkan/VulkanPipeline.h"
#include "X2/Vulkan/VulkanRenderpass.h"
#include "X2/Vulkan/VulkanTexture.h"

#define MAX_POINT_LIGHT_SHADOW_COUNT 16


namespace X2
{
	struct PointLightShadowData {
		glm::mat4 ViewProjection[MAX_POINT_LIGHT_SHADOW_COUNT * 6];
	};

	class PointLightShadow
	{
	public:
		PointLightShadow(uint32_t resolution);
		~PointLightShadow() {}

		void Update(const std::vector<PointLightInfo>& pointLightInfos);

		void RenderStaticShadow(Ref<VulkanRenderCommandBuffer> cb, Ref<VulkanUniformBufferSet> uniformBufferSet, std::map<MeshKey, StaticDrawCommand>& staticDrawList, std::map<MeshKey, TransformMapData>* curTransformMap, Ref<VulkanVertexBuffer> buffer);

		void FillTexturesToAtlas(Ref<VulkanRenderCommandBuffer> cb);

		Ref<VulkanPipeline> GetPipeline(uint32_t index) { return m_ShadowPassPipelines[index]; }

		Ref<VulkanImage2D> GetShadowCubemapAtlas() { return m_ShadowCubemapAtlas; }

		uint32_t GetResolution() { return m_resolution; }
		PointLightShadowData& getData() { return m_data; }


	private:
		PointLightShadowData m_data;

		uint32_t m_resolution;
		uint32_t m_activePointLightCount = 0;
		
		Ref<VulkanImage2D> m_ShadowCubemapAtlas;
		Ref<VulkanImage2D> m_ShaodwArrays[MAX_POINT_LIGHT_SHADOW_COUNT];
		Ref<VulkanPipeline> m_ShadowPassPipelines[MAX_POINT_LIGHT_SHADOW_COUNT * 6];

		Ref<VulkanMaterial> m_ShadowPassMaterial;


	};

}
#endif