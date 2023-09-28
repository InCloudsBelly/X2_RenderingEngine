#ifndef X2_SPOTLIGHTSHADOW
#define X2_SPOTLIGHTSHADOW

#include "X2/Renderer/Renderer.h"
#include "X2/Vulkan/VulkanPipeline.h"
#include "X2/Vulkan/VulkanRenderpass.h"
#include "X2/Vulkan/VulkanTexture.h"


#define MAX_SPOT_LIGHT_SHADOW_COUNT 16


namespace X2
{
	struct SpotLightShadowData {
		glm::mat4 ViewProjection[MAX_SPOT_LIGHT_SHADOW_COUNT];
	};


	class SpotLightShadow
	{
	public:
		SpotLightShadow(uint32_t resolution);
		~SpotLightShadow() {}

		void Update(const std::vector<SpotLightInfo>& spotLightInfos);

		void RenderStaticShadow(Ref<VulkanRenderCommandBuffer> cb, Ref<VulkanUniformBufferSet> uniformBufferSet, std::map<MeshKey, StaticDrawCommand>& staticDrawList, std::map<MeshKey, TransformMapData>* curTransformMap, Ref<VulkanVertexBuffer> buffer);

		Ref<VulkanPipeline> GetPipeline(uint32_t index) { return m_ShadowPassPipelines[index]; }

		uint32_t GetResolution() { return m_resolution; }
		SpotLightShadowData& getData() { return m_data; }

	private:
		SpotLightShadowData m_data;

		uint32_t m_resolution;
		uint32_t m_activeSpotLightCount = 0;
		Ref<VulkanPipeline> m_ShadowPassPipelines[MAX_SPOT_LIGHT_SHADOW_COUNT];
		Ref<VulkanMaterial> m_ShadowPassMaterial;

	};

}


#endif