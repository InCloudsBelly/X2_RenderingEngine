#include "Precompiled.h"
#include "Renderer.h"

#include "X2/Vulkan/VulkanShader.h"

#include <map>

#include "SceneRenderer.h"
#include "SceneEnvironment.h"
#include "Renderer2D.h"

#include "X2/Core/Timer.h"
#include "X2/Core/Debug/Profiler.h"
#include "X2/Core/Application.h"

#include "X2/Vulkan/VulkanRenderer.h"
#include "X2/Vulkan/VulkanContext.h"
#include "X2/Project/Project.h"
#include "X2/Utilities/SMAAAreaTex.h"
#include "X2/Utilities/SMAASearchTex.h"

#include <filesystem>

namespace std {
	template<>
	struct hash<X2::WeakRef<X2::VulkanShader>>
	{
		size_t operator()(const X2::WeakRef<X2::VulkanShader>& shader) const noexcept
		{
			return shader->GetHash();
		}
	};
}

namespace X2 {

	static std::unordered_map<size_t, Ref<VulkanPipeline>> s_PipelineCache;

	static VulkanRenderer* s_RendererAPI = nullptr;

	struct ShaderDependencies
	{
		std::vector<Ref<VulkanComputePipeline>> ComputePipelines;
		std::vector<Ref<VulkanPipeline>> Pipelines;
		std::vector<Ref<VulkanMaterial>> Materials;
	};
	static std::unordered_map<size_t, ShaderDependencies> s_ShaderDependencies;

	struct GlobalShaderInfo
	{
		// Macro name, set of shaders with that macro.
		std::unordered_map<std::string, std::unordered_map<size_t, WeakRef<VulkanShader>>> ShaderGlobalMacrosMap;
		// Shaders waiting to be reloaded.
		std::unordered_set<WeakRef<VulkanShader>> DirtyShaders;
	};
	static GlobalShaderInfo s_GlobalShaderInfo;

	void Renderer::RegisterShaderDependency(Ref<VulkanShader> shader, Ref<VulkanComputePipeline> computePipeline)
	{
		s_ShaderDependencies[shader->GetHash()].ComputePipelines.push_back(computePipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<VulkanShader> shader, Ref<VulkanPipeline> pipeline)
	{
		s_ShaderDependencies[shader->GetHash()].Pipelines.push_back(pipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<VulkanShader> shader, Ref<VulkanMaterial> material)
	{
		s_ShaderDependencies[shader->GetHash()].Materials.push_back(material);
	}

	void Renderer::OnShaderReloaded(size_t hash)
	{
		if (s_ShaderDependencies.find(hash) != s_ShaderDependencies.end())
		{
			auto& dependencies = s_ShaderDependencies.at(hash);
			for (auto& pipeline : dependencies.Pipelines)
			{
				pipeline->Invalidate();
			}

			for (auto& computePipeline : dependencies.ComputePipelines)
			{
				computePipeline.As<VulkanComputePipeline>()->CreatePipeline();
			}

			for (auto& material : dependencies.Materials)
			{
				material->OnShaderReloaded();
			}
		}
	}

	uint32_t Renderer::RT_GetCurrentFrameIndex()
	{
		// Swapchain owns the Render Thread frame index
		return Application::Get().GetWindow().GetSwapChain().GetCurrentBufferIndex();
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return Application::Get().GetCurrentFrameIndex();
	}

	struct RendererData
	{
		Ref<ShaderLibrary> m_ShaderLibrary;

		Ref<VulkanTexture2D> WhiteTexture;
		Ref<VulkanTexture2D> BlackTexture;
		Ref<VulkanTexture2D> BRDFLutTexture;
		Ref<VulkanTexture2D> HilbertLut;
		Ref<VulkanTexture2D> SMAASearchLut;
		Ref<VulkanTexture2D> SMAAAreaLut;
		Ref<VulkanTextureCube> BlackCubeTexture;

		Ref<Environment> EmptyEnvironment;

		std::unordered_map<std::string, std::string> GlobalShaderMacros;
	};

	static RendererConfig s_Config;
	static RendererData* s_Data = nullptr;
	constexpr static uint32_t s_RenderCommandQueueCount = 2;
	static RenderCommandQueue* s_CommandQueue[s_RenderCommandQueueCount];
	static std::atomic<uint32_t> s_RenderCommandQueueSubmissionIndex = 0;
	static RenderCommandQueue s_ResourceFreeQueue[3];




	void Renderer::Init()
	{
		s_Data = hnew RendererData();
		s_CommandQueue[0] = hnew RenderCommandQueue();
		s_CommandQueue[1] = hnew RenderCommandQueue();

		// Make sure we don't have more frames in flight than swapchain images
		s_Config.FramesInFlight = glm::min<uint32_t>(s_Config.FramesInFlight, Application::Get().GetWindow().GetSwapChain().GetImageCount());

		s_RendererAPI = hnew VulkanRenderer();

		s_Data->m_ShaderLibrary = Ref<ShaderLibrary>::Create();

		if (!s_Config.ShaderPackPath.empty())
			Renderer::GetShaderLibrary()->LoadShaderPack(s_Config.ShaderPackPath);

		// NOTE: some shaders (compute) need to have optimization disabled because of a shaderc internal error
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBR_Static.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBR_Transparent.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBR_Anim.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Grid.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Wireframe.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Wireframe_Anim.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Skybox.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/DirShadowMap.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/DirShadowMap_Anim.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SpotShadowMap.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SpotShadowMap_Anim.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/HZB.glsl");

		// HBAO
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Deinterleaving.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Reinterleaving.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/HBAOBlur.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/HBAO.glsl");

		// GTAO
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/GTAO.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/GTAO-Denoise.glsl");

		// AO
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/AO-Composite.glsl");

		//SSR
		Renderer::GetShaderLibrary()->Load("Resources/shaders/Pre-Integration.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/Pre-Convolution.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/SSR.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/SSR-Composite.glsl");

		// Environment compute shaders
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentMipFilter.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EquirectangularToCubeMap.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentIrradiance.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreethamSky.glsl");

		// Post-processing
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/Bloom.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/DOF.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/EdgeDetection.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/SceneComposite.glsl");

		// Light-culling
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreDepth.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreDepth_Anim.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/LightCulling.glsl");

		// Renderer2D Shaders
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Renderer2D.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Renderer2D_Line.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Renderer2D_Circle.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Renderer2D_Text.glsl");

		// Jump Flood Shaders
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/JumpFlood_Init.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/JumpFlood_Pass.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/JumpFlood_Composite.glsl");

		// Misc
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SelectedGeometry.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SelectedGeometry_Anim.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/TexturePass.glsl");

		//SMAA
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/SMAAEdgeDetect.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/SMAABlendWeight.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PostProcessing/SMAANeighborBlend.glsl");

		// Compile shaders
		Application::Get().GetRenderThread().Pump();

		uint32_t whiteTextureData = 0xffffffff;
		TextureSpecification spec;
		spec.Format = ImageFormat::RGBA;
		spec.Width = 1;
		spec.Height = 1;
		s_Data->WhiteTexture = Ref<VulkanTexture2D>::Create(spec, Buffer(&whiteTextureData, sizeof(uint32_t)));

		constexpr uint32_t blackTextureData = 0xff000000;
		s_Data->BlackTexture = Ref<VulkanTexture2D>::Create(spec, Buffer(&blackTextureData, sizeof(uint32_t)));

		{
			TextureSpecification spec;
			spec.SamplerWrap = TextureWrap::Clamp;
			s_Data->BRDFLutTexture = Ref<VulkanTexture2D>::Create(spec, std::filesystem::path(std::string(PROJECT_ROOT)+"Resources/Renderer/BRDF_LUT.tga"));

			s_Data->SMAAAreaLut = Ref<VulkanTexture2D>::Create(spec, std::filesystem::path(std::string(PROJECT_ROOT) + "Resources/Renderer/AreaTex.png") ,true);

			s_Data->SMAASearchLut = Ref<VulkanTexture2D>::Create(spec, std::filesystem::path(std::string(PROJECT_ROOT) + "Resources/Renderer/SearchTex.png"), true);
		}

		constexpr uint32_t blackCubeTextureData[6] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
		s_Data->BlackCubeTexture = Ref<VulkanTextureCube>::Create(spec, Buffer(&blackTextureData, sizeof(blackCubeTextureData)));

		s_Data->EmptyEnvironment = Ref<Environment>::Create(s_Data->BlackCubeTexture,s_Data->BlackCubeTexture, s_Data->BlackCubeTexture);

		// Hilbert look-up texture! It's a 64 x 64 uint16 texture
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::RED16UI;
			spec.Width = 64;
			spec.Height = 64;
			spec.SamplerWrap = TextureWrap::Clamp;
			spec.SamplerFilter = TextureFilter::Nearest;

			constexpr auto HilbertIndex = [](uint32_t posX, uint32_t posY)
			{
				uint16_t index = 0u;
				for (uint16_t curLevel = 64 / 2u; curLevel > 0u; curLevel /= 2u)
				{
					const uint16_t regionX = (posX & curLevel) > 0u;
					const uint16_t regionY = (posY & curLevel) > 0u;
					index += curLevel * curLevel * ((3u * regionX) ^ regionY);
					if (regionY == 0u)
					{
						if (regionX == 1u)
						{
							posX = uint16_t((64 - 1u)) - posX;
							posY = uint16_t((64 - 1u)) - posY;
						}

						std::swap(posX, posY);
					}
				}
				return index;
			};

			uint16_t* data = new uint16_t[(size_t)(64 * 64)];
			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
					const uint16_t r2index = HilbertIndex(x, y);
					X2_CORE_ASSERT(r2index < 65536);
					data[x + 64 * y] = r2index;
				}
			}
			s_Data->HilbertLut = Ref<VulkanTexture2D>::Create(spec, Buffer(data, 1));
			delete[] data;

		}

		s_RendererAPI->Init();
	}

	void Renderer::Shutdown()
	{
		s_ShaderDependencies.clear();
		s_RendererAPI->Shutdown();

		delete s_Data;

		// Resource release queue
		for (uint32_t i = 0; i < s_Config.FramesInFlight; i++)
		{
			auto& queue = Renderer::GetRenderResourceReleaseQueue(i);
			queue.Execute();
		}

		delete s_CommandQueue[0];
		delete s_CommandQueue[1];
	}

	RendererCapabilities& Renderer::GetCapabilities()
	{
		return s_RendererAPI->GetCapabilities();
	}

	Ref<ShaderLibrary> Renderer::GetShaderLibrary()
	{
		return s_Data->m_ShaderLibrary;
	}

	void Renderer::RenderThreadFunc(RenderThread* renderThread)
	{
		X2_PROFILE_THREAD("Render Thread");

		while (renderThread->IsRunning())
		{
			WaitAndRender(renderThread);
		}
	}

	void Renderer::WaitAndRender(RenderThread* renderThread)
	{
		X2_PROFILE_FUNC();
		auto& performanceTimers = Application::Get().m_PerformanceTimers;

		// Wait for kick, then set render thread to busy
		{
			X2_PROFILE_FUNC("Wait");
			Timer waitTimer;
			renderThread->WaitAndSet(RenderThread::State::Kick, RenderThread::State::Busy);
			performanceTimers.RenderThreadWaitTime = waitTimer.ElapsedMillis();
		}

		Timer workTimer;
		s_CommandQueue[GetRenderQueueIndex()]->Execute();
		// ExecuteRenderCommandQueue();

		// Rendering has completed, set state to idle
		renderThread->Set(RenderThread::State::Idle);

		performanceTimers.RenderThreadWorkTime = workTimer.ElapsedMillis();
	}

	void Renderer::SwapQueues()
	{
		s_RenderCommandQueueSubmissionIndex = (s_RenderCommandQueueSubmissionIndex + 1) % s_RenderCommandQueueCount;
	}

	uint32_t Renderer::GetRenderQueueIndex()
	{
		return (s_RenderCommandQueueSubmissionIndex + 1) % s_RenderCommandQueueCount;
	}

	uint32_t Renderer::GetRenderQueueSubmissionIndex()
	{
		return s_RenderCommandQueueSubmissionIndex;
	}

	void Renderer::BeginRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanRenderPass> renderPass, bool explicitClear)
	{
		X2_CORE_ASSERT(renderPass, "Render pass cannot be null!");

		s_RendererAPI->BeginRenderPass(renderCommandBuffer, renderPass, explicitClear);
	}

	void Renderer::EndRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->EndRenderPass(renderCommandBuffer);
	}

	void Renderer::RT_InsertGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& color)
	{
		s_RendererAPI->RT_InsertGPUPerfMarker(renderCommandBuffer, label, color);
	}

	void Renderer::RT_BeginGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor)
	{
		s_RendererAPI->RT_BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
	}

	void Renderer::RT_EndGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->RT_EndGPUPerfMarker(renderCommandBuffer);
	}

	void Renderer::BeginFrame()
	{
		s_RendererAPI->BeginFrame();
	}

	void Renderer::EndFrame()
	{
		s_RendererAPI->EndFrame();
	}

	void Renderer::SetSceneEnvironment(Ref<SceneRenderer> sceneRenderer, Ref<Environment> environment, Ref<VulkanImage2D> shadow, Ref<VulkanImage2D> spotShadow)
	{
		s_RendererAPI->SetSceneEnvironment(sceneRenderer, environment, shadow, spotShadow);
	}

	Ref<Environment> Renderer::CreateEnvironmentMap(const std::string& filepath)
	{
		return s_RendererAPI->CreateEnvironmentMap(filepath);
	}

	void Renderer::LightCulling(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> computePipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups)
	{
		s_RendererAPI->LightCulling(renderCommandBuffer, computePipeline, uniformBufferSet, storageBufferSet, material, workGroups);
	}

	void Renderer::DispatchComputeShader(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> computePipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups, const Buffer additionalUniforms)
	{
		s_RendererAPI->DispatchComputeShader(renderCommandBuffer, computePipeline, uniformBufferSet, storageBufferSet, material, workGroups, additionalUniforms);
	}

	Ref<VulkanTextureCube> Renderer::CreatePreethamSky(float turbidity, float azimuth, float inclination)
	{
		return s_RendererAPI->CreatePreethamSky(turbidity, azimuth, inclination);
	}

	void Renderer::RenderStaticMesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount)
	{
		s_RendererAPI->RenderStaticMesh(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, materialTable, transformBuffer, transformOffset, instanceCount);
	}

#if 0
	void Renderer::RenderSubmesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform)
	{
		s_RendererAPI->RenderSubmesh(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, materialTable, transform);
	}
#endif

	void Renderer::RenderSubmeshInstanced(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformUBs, uint32_t boneTransformsOffset, uint32_t instanceCount)
	{
		s_RendererAPI->RenderSubmeshInstanced(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, materialTable, transformBuffer, transformOffset, boneTransformUBs, boneTransformsOffset, instanceCount);
	}

	void Renderer::RenderMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformUBs, uint32_t boneTransformsOffset, uint32_t instanceCount, Ref<VulkanMaterial> material, Buffer additionalUniforms)
	{
		s_RendererAPI->RenderMeshWithMaterial(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, material, transformBuffer, transformOffset, boneTransformUBs, boneTransformsOffset, instanceCount, additionalUniforms);
	}

	void Renderer::RenderStaticMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount, Ref<VulkanMaterial> material, Buffer additionalUniforms)
	{
		s_RendererAPI->RenderStaticMeshWithMaterial(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, material, transformBuffer, transformOffset, instanceCount, additionalUniforms);
	}

	void Renderer::RenderQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::mat4& transform)
	{
		s_RendererAPI->RenderQuad(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material, transform);
	}

	void Renderer::RenderGeometry(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> vertexBuffer, Ref<VulkanIndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount /*= 0*/)
	{
		s_RendererAPI->RenderGeometry(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material, vertexBuffer, indexBuffer, transform, indexCount);
	}

	void Renderer::SubmitQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanMaterial> material, const glm::mat4& transform)
	{
		X2_CORE_ASSERT(false, "Not Implemented");
		/*bool depthTest = true;
		if (material)
		{
				material->Bind();
				depthTest = material->GetFlag(MaterialFlag::DepthTest);
				cullFace = !material->GetFlag(MaterialFlag::TwoSided);

				auto shader = material->GetShader();
				shader->SetUniformBuffer("Transform", &transform, sizeof(glm::mat4));
		}

		s_Data->m_FullscreenQuadVertexBuffer->Bind();
		s_Data->m_FullscreenQuadPipeline->Bind();
		s_Data->m_FullscreenQuadIndexBuffer->Bind();
		Renderer::DrawIndexed(6, PrimitiveType::Triangles, depthTest);*/
	}

	void Renderer::ClearImage(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanImage2D> image)
	{
		s_RendererAPI->ClearImage(renderCommandBuffer, image);
	}

	void Renderer::CopyImage(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanImage2D> sourceImage, Ref<VulkanImage2D> destinationImage)
	{
		s_RendererAPI->CopyImage(renderCommandBuffer, sourceImage, destinationImage);
	}

	void Renderer::SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material)
	{
		s_RendererAPI->SubmitFullscreenQuad(renderCommandBuffer, pipeline, uniformBufferSet, material);
	}

	void Renderer::SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material)
	{
		s_RendererAPI->SubmitFullscreenQuad(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material);
	}

	void Renderer::SubmitFullscreenQuadWithOverrides(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides)
	{
		s_RendererAPI->SubmitFullscreenQuadWithOverrides(renderCommandBuffer, pipeline, uniformBufferSet, material, vertexShaderOverrides, fragmentShaderOverrides);
	}

#if 0
	void Renderer::SubmitFullscreenQuad(Ref<VulkanMaterial> material)
	{
		// Retrieve pipeline from cache
		auto& shader = material->GetShader();
		auto hash = shader->GetHash();
		if (s_PipelineCache.find(hash) == s_PipelineCache.end())
		{
			// Create pipeline
			PipelineSpecification spec = s_Data->m_FullscreenQuadPipelineSpec;
			spec.VulkanShader = shader;
			spec.DebugName = "Renderer-FullscreenQuad-" + shader->GetName();
			s_PipelineCache[hash] = VulkanPipeline::Create(spec);
		}

		auto& pipeline = s_PipelineCache[hash];

		bool depthTest = true;
		bool cullFace = true;
		if (material)
		{
			// material->Bind();
			depthTest = material->GetFlag(MaterialFlag::DepthTest);
			cullFace = !material->GetFlag(MaterialFlag::TwoSided);
		}

		s_Data->FullscreenQuadVertexBuffer->Bind();
		pipeline->Bind();
		s_Data->FullscreenQuadIndexBuffer->Bind();
		Renderer::DrawIndexed(6, PrimitiveType::Triangles, depthTest);
	}
#endif

	Ref<VulkanTexture2D> Renderer::GetWhiteTexture()
	{
		return s_Data->WhiteTexture;
	}

	Ref<VulkanTexture2D> Renderer::GetBlackTexture()
	{
		return s_Data->BlackTexture;
	}

	Ref<VulkanTexture2D> Renderer::GetSMAAAreaLut()
	{
		return s_Data->SMAAAreaLut;
	}
	Ref<VulkanTexture2D> Renderer::GetSMAASearchLut()
	{
		return s_Data->SMAASearchLut;
	}

	Ref<VulkanTexture2D> Renderer::GetHilbertLut()
	{
		return s_Data->HilbertLut;
	}

	Ref<VulkanTexture2D> Renderer::GetBRDFLutTexture()
	{
		return s_Data->BRDFLutTexture;
	}

	Ref<VulkanTextureCube> Renderer::GetBlackCubeTexture()
	{
		return s_Data->BlackCubeTexture;
	}


	Ref<Environment> Renderer::GetEmptyEnvironment()
	{
		return s_Data->EmptyEnvironment;
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return *s_CommandQueue[s_RenderCommandQueueSubmissionIndex];
	}

	RenderCommandQueue& Renderer::GetRenderResourceReleaseQueue(uint32_t index)
	{
		return s_ResourceFreeQueue[index];
	}


	const std::unordered_map<std::string, std::string>& Renderer::GetGlobalShaderMacros()
	{
		return s_Data->GlobalShaderMacros;
	}

	RendererConfig& Renderer::GetConfig()
	{
		return s_Config;
	}

	void Renderer::SetConfig(const RendererConfig& config)
	{
		s_Config = config;
	}

	void Renderer::AcknowledgeParsedGlobalMacros(const std::unordered_set<std::string>& macros, Ref<VulkanShader> shader)
	{
		for (const std::string& macro : macros)
		{
			s_GlobalShaderInfo.ShaderGlobalMacrosMap[macro][shader->GetHash()] = shader;
		}
	}

	void Renderer::SetMacroInShader(Ref<VulkanShader> shader, const std::string& name, const std::string& value)
	{
		shader->SetMacro(name, value);
		s_GlobalShaderInfo.DirtyShaders.emplace(shader.Raw());
	}

	void Renderer::SetGlobalMacroInShaders(const std::string& name, const std::string& value)
	{
		if (s_Data->GlobalShaderMacros.find(name) != s_Data->GlobalShaderMacros.end())
		{
			if (s_Data->GlobalShaderMacros.at(name) == value)
				return;
		}

		s_Data->GlobalShaderMacros[name] = value;

		if (s_GlobalShaderInfo.ShaderGlobalMacrosMap.find(name) == s_GlobalShaderInfo.ShaderGlobalMacrosMap.end())
		{
			X2_CORE_WARN("No shaders with {} macro found", name);
			return;
		}

		X2_CORE_ASSERT(s_GlobalShaderInfo.ShaderGlobalMacrosMap.find(name) != s_GlobalShaderInfo.ShaderGlobalMacrosMap.end(), "Macro has not been passed from any shader!");
		for (auto& [hash, shader] : s_GlobalShaderInfo.ShaderGlobalMacrosMap.at(name))
		{
			X2_CORE_ASSERT(shader.IsValid(), "VulkanShader is deleted!");
			s_GlobalShaderInfo.DirtyShaders.emplace(shader);
		}
	}

	bool Renderer::UpdateDirtyShaders()
	{
		// TODO(Yan): how is this going to work for dist?
		const bool updatedAnyShaders = s_GlobalShaderInfo.DirtyShaders.size();
		for (WeakRef<VulkanShader> shader : s_GlobalShaderInfo.DirtyShaders)
		{
			X2_CORE_ASSERT(shader.IsValid(), "VulkanShader is deleted!");
			shader->RT_Reload(true);
		}
		s_GlobalShaderInfo.DirtyShaders.clear();

		return updatedAnyShaders;
	}

}
