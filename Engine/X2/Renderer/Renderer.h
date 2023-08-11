
#pragma once

#include "X2/Core/RenderThread.h"

#include "X2/Vulkan/VulkanContext.h"
#include "X2/Vulkan/VulkanRenderPass.h"
#include "X2/Vulkan/VulkanRenderCommandBuffer.h"
#include "X2/Vulkan/VulkanUniformBufferSet.h"
#include "X2/Vulkan/VulkanStorageBufferSet.h"
#include "X2/Vulkan/VulkanMaterial.h"
#include "X2/Vulkan/VulkanComputePipeline.h"
#include "X2/Vulkan/VulkanImage.h"
#include "X2/Vulkan/VulkanTexture.h"

#include "RenderCommandQueue.h"
#include "RendererCapabilities.h"
#include "RendererConfig.h"

#include "Mesh.h"

#include "X2/Core/Application.h"
#include "X2/Core/RenderThread.h"

#include "RendererConfig.h"

#include "X2/Scene/Scene.h"

namespace X2 {

	class ShaderLibrary;
	class VulkanMaterial;


	class Renderer
	{
	public:
		typedef void(*RenderCommandFn)(void*);

		static Ref<VulkanContext> GetContext()
		{
			return Application::Get().GetWindow().GetRenderContext();
		}

		static void Init();
		static void Shutdown();

		static RendererCapabilities& GetCapabilities();

		static Ref<ShaderLibrary> GetShaderLibrary();

		template<typename FuncT>
		static void Submit(FuncT&& func)
		{
			auto renderCmd = [](void* ptr) {
				auto pFunc = (FuncT*)ptr;
				(*pFunc)();

				// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
				// however some items like uniforms which contain std::strings still exist for now
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};
			auto storageBuffer = GetRenderCommandQueue().Allocate(renderCmd, sizeof(func));
			new (storageBuffer) FuncT(std::forward<FuncT>(func));
		}

		template<typename FuncT>
		static void SubmitResourceFree(FuncT&& func)
		{
			auto renderCmd = [](void* ptr) {
				auto pFunc = (FuncT*)ptr;
				(*pFunc)();

				// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
				// however some items like uniforms which contain std::strings still exist for now
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			Submit([renderCmd, func]()
				{
					const uint32_t index = Renderer::RT_GetCurrentFrameIndex();
					auto storageBuffer = GetRenderResourceReleaseQueue(index).Allocate(renderCmd, sizeof(func));
					new (storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
				});
		}

		/*static void* Submit(RenderCommandFn fn, unsigned int size)
		{
			return s_Instance->m_CommandQueue.Allocate(fn, size);
		}*/

		static void WaitAndRender(RenderThread* renderThread);
		static void SwapQueues();

		static void RenderThreadFunc(RenderThread* renderThread);
		static uint32_t GetRenderQueueIndex();
		static uint32_t GetRenderQueueSubmissionIndex();

		// ~Actual~ Renderer here... TODO: remove confusion later
		static void BeginRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanRenderPass> renderPass, bool explicitClear = false);
		static void EndRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer);

		static void RT_BeginGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {});
		static void RT_InsertGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {});
		static void RT_EndGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer);

		static void BeginFrame();
		static void EndFrame();

		static void SetSceneEnvironment(Ref<SceneRenderer> sceneRenderer, Ref<Environment> environment, Ref<VulkanImage2D> shadow, Ref<VulkanImage2D> spotShadow);
		static Ref<Environment> CreateEnvironmentMap(const std::string& filepath);
		static Ref<VulkanTextureCube> CreatePreethamSky(float turbidity, float azimuth, float inclination);

		static void RenderStaticMesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount);
		//static void RenderSubmesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform);
		static void RenderSubmeshInstanced(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformUBs, uint32_t boneTransformsOffset, uint32_t instanceCount);
		static void RenderMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformUBs, uint32_t boneTransformsOffset, uint32_t instanceCount, Ref<VulkanMaterial> material, Buffer additionalUniforms = Buffer());
		static void RenderStaticMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount, Ref<VulkanMaterial> material, Buffer additionalUniforms = Buffer());
		static void RenderQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::mat4& transform);
		static void SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material);
		static void SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material);
		static void SubmitFullscreenQuadWithOverrides(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides);
		static void LightCulling(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> computePipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups);
		static void DispatchComputeShader(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> computePipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups, Buffer additionalUniforms = {});
		static void RenderGeometry(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> vertexBuffer, Ref<VulkanIndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount = 0);
		static void SubmitQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanMaterial> material, const glm::mat4& transform = glm::mat4(1.0f));
		static void ClearImage(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanImage2D> image);
		static void CopyImage(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanImage2D> sourceImage, Ref<VulkanImage2D> destinationImage);

		static Ref<VulkanTexture2D> GetWhiteTexture();
		static Ref<VulkanTexture2D> GetBlackTexture();
		static Ref<VulkanTexture2D> GetHilbertLut();
		static Ref<VulkanTexture2D> GetBRDFLutTexture();
		static Ref<VulkanTextureCube> GetBlackCubeTexture();
		static Ref<Environment> GetEmptyEnvironment();

		static void RegisterShaderDependency(Ref<VulkanShader> shader, Ref<VulkanComputePipeline> computePipeline);
		static void RegisterShaderDependency(Ref<VulkanShader> shader, Ref<VulkanPipeline> pipeline);
		static void RegisterShaderDependency(Ref<VulkanShader> shader, Ref<VulkanMaterial> material);
		static void OnShaderReloaded(size_t hash);

		static uint32_t GetCurrentFrameIndex();
		static uint32_t RT_GetCurrentFrameIndex();

		static RendererConfig& GetConfig();
		static void SetConfig(const RendererConfig& config);

		static RenderCommandQueue& GetRenderResourceReleaseQueue(uint32_t index);

		// Add known macro from shader.
		static const std::unordered_map<std::string, std::string>& GetGlobalShaderMacros();
		static void AcknowledgeParsedGlobalMacros(const std::unordered_set<std::string>& macros, Ref<VulkanShader> shader);
		static void SetMacroInShader(Ref<VulkanShader> shader, const std::string& name, const std::string& value = "");
		static void SetGlobalMacroInShaders(const std::string& name, const std::string& value = "");
		// Returns true if any shader is actually updated.
		static bool UpdateDirtyShaders();

	private:
		static RenderCommandQueue& GetRenderCommandQueue();
	};

	namespace Utils {

		inline void DumpGPUInfo()
		{
			auto& caps = Renderer::GetCapabilities();
			X2_CORE_TRACE_TAG("Renderer", "GPU Info:");
			X2_CORE_TRACE_TAG("Renderer", "  Vendor: {0}", caps.Vendor);
			X2_CORE_TRACE_TAG("Renderer", "  Device: {0}", caps.Device);
			X2_CORE_TRACE_TAG("Renderer", "  Version: {0}", caps.Version);
		}

	}

}
