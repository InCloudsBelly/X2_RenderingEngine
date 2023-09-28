#ifndef X2_DIRECTIONALSHADOW
#define X2_DIRECTIONALSHADOW

#include "X2/Renderer/Renderer.h"
#include "X2/Vulkan/VulkanPipeline.h"
#include "X2/Vulkan/VulkanRenderpass.h"
#include "X2/Vulkan/VulkanTexture.h"

#define CASCADED_COUNT 4


namespace X2
{
	struct DirectionalLightData {
		float CascadeSplits[CASCADED_COUNT];
		glm::mat4 ViewProjection[CASCADED_COUNT];
	};


	class DirectionalLightShadow
	{
	public:
		DirectionalLightShadow(uint32_t resolution);
		~DirectionalLightShadow() {}

		void Update( const glm::vec3 lightDirection, const SceneRendererCamera& camera, float splitLambda, float nearOffset, float farOffset);

		void RenderStaticShadow(Ref<VulkanRenderCommandBuffer> cb, Ref<VulkanUniformBufferSet> uniformBufferSet, std::map<MeshKey, StaticDrawCommand>& staticDrawList, std::map<MeshKey, TransformMapData>* curTransformMap, Ref<VulkanVertexBuffer> buffer);

		Ref<VulkanPipeline> GetPipeline(uint32_t index) { return m_ShadowPassPipelines[index]; }

		uint32_t GetResolution() { return m_resolution; }
		DirectionalLightData& getData() { return m_data; }

	private:
		DirectionalLightData m_data;

		uint32_t m_resolution;
		Ref<VulkanPipeline> m_ShadowPassPipelines[4];
		Ref<VulkanMaterial> m_ShadowPassMaterial;

	};

}
#endif