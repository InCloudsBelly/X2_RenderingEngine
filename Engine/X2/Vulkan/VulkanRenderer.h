#pragma once

#include "VulkanComputePipeline.h"

#include "VulkanMaterial.h"
#include "VulkanUniformBuffer.h"
#include "VulkanStorageBuffer.h"
#include "VulkanUniformBufferSet.h"
#include "VulkanStorageBufferSet.h"

#include "X2/Scene/Scene.h"
#include "X2/Renderer/RendererCapabilities.h"

#include "vulkan/vulkan.h"

namespace X2 {
	
	enum class PrimitiveType
	{
		None = 0, Triangles, Lines
	};


	class VulkanRenderer 
	{
	public:
		virtual void Init() ;
		virtual void Shutdown() ;

		virtual RendererCapabilities& GetCapabilities() ;

		virtual void BeginFrame() ;
		virtual void EndFrame() ;

		virtual void RT_InsertGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {}) ;
		virtual void RT_BeginGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {}) ;
		virtual void RT_EndGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer) ;

		virtual void BeginRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const Ref<VulkanRenderPass>& renderPass, bool explicitClear = false) ;
		virtual void EndRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer) ;
		virtual void SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material) ;
		virtual void SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material) ;
		virtual void SubmitFullscreenQuadWithOverrides(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides) ;

		virtual void SetSceneEnvironment(SceneRenderer* sceneRenderer, Ref<Environment> environment, Ref<VulkanImage2D> shadow, Ref<VulkanImage2D> spotShadow, Ref<VulkanImage2D> pointShadow) ;

		virtual Ref<Environment> CreateEnvironmentMap(const std::string& filepath) ;
		virtual Ref<VulkanTextureCube> CreatePreethamSky(float turbidity, float azimuth, float inclination) ;

		virtual void RenderStaticMesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount) ;
		//virtual void RenderSubmesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t index, Ref<MaterialTable> materialTable, const glm::mat4& transform) ;
		virtual void RenderSubmeshInstanced(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t index, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformUBs, uint32_t boneTransformsOffset, uint32_t instanceCount) ;
		virtual void RenderMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformUBs, uint32_t boneTransformsOffset, uint32_t instanceCount, Buffer additionalUniforms = Buffer()) ;
		virtual void RenderStaticMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount, Buffer additionalUniforms = Buffer()) ;
		virtual void RenderQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::mat4& transform) ;
		virtual void LightCulling(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> pipelineCompute, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups) ;
		virtual void RenderGeometry(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> vertexBuffer, Ref<VulkanIndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount = 0) ;
		virtual void DispatchComputeShader(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups, Buffer additionalUniforms) ;
		virtual void ClearImage(Ref<VulkanRenderCommandBuffer> commandBuffer, Ref<VulkanImage2D> image) ;
		virtual void CopyImage(Ref<VulkanRenderCommandBuffer> commandBuffer, Ref<VulkanImage2D> sourceImage, Ref<VulkanImage2D> destinationImage) ;

		static void RT_UpdateMaterialForRendering(Ref<VulkanMaterial> material, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet);
		static VkSampler GetClampSampler();
		static VkSampler GetPointSampler();

		static uint32_t GetDescriptorAllocationCount(uint32_t frameIndex = 0);

		static int32_t& GetSelectedDrawCall();

		static void ReleaseResoueces();
	public:
		static VkDescriptorSet RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);
	};

	namespace Utils {

		void InsertImageMemoryBarrier(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange);

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageAspectFlags aspectMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	}

}
