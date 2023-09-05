#include "Precompiled.h"
#include "SceneRenderer.h"

#include "Renderer.h"
#include "SceneEnvironment.h"

#include <imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/compatibility.hpp>

#include "Renderer2D.h"


#include "X2/Utilities/FileSystem.h"

#include "X2/ImGui/ImGui.h"
#include "X2/Core/Debug/Profiler.h"
#include "X2/Math/Math.h"
#include "X2/Math/Noise.h"

#include "X2/Vulkan/VulkanComputePipeline.h"
#include "X2/Vulkan/VulkanMaterial.h"
#include "X2/Vulkan/VulkanRenderer.h"
#include "X2/Vulkan/VulkanUniformBuffer.h"


namespace X2 {

	// MUST AGREE WITH WHAT IS IN THE SHADERS
	// (conversely, shader bindings must agree with this...)
	enum Binding : uint32_t
	{
		CameraData = 0,
		ShadowData = 1,
		SceneData = 2,
		RendererData = 3,
		PointLightData = 4,

		VisiblePointLightIndicesBuffer = 14,
		VisibleSpotLightIndicesBuffer = 23,

		ScreenData = 17,
		HBAOData = 18,
		SMAAData = 24,
		TAAData = 25,
		RayMarchingData = 26 
	};

	static std::vector<std::thread> s_ThreadPool;

	SceneRenderer::SceneRenderer(Ref<Scene> scene, SceneRendererSpecification specification)
		: m_Scene(scene), m_Specification(specification)
	{
		Init();
	}

	SceneRenderer::~SceneRenderer()
	{
		Shutdown();
	}

	void SceneRenderer::Init()
	{
		X2_SCOPE_TIMER("SceneRenderer::Init");

		m_ShadowCascadeSplits[0] = 0.1f;
		m_ShadowCascadeSplits[1] = 0.2f;
		m_ShadowCascadeSplits[2] = 0.3f;
		m_ShadowCascadeSplits[3] = 1.0f;

		// Tiering
		{
			using namespace Tiering::Renderer;

			const auto& tiering = m_Specification.Tiering;

			RendererDataUB.SoftShadows = tiering.ShadowQuality == ShadowQualitySetting::High;

			m_Options.EnableGTAO = false;
			m_Options.EnableHBAO = false;

			if (tiering.EnableAO)
			{
				switch (tiering.AOType)
				{
				case Tiering::Renderer::AmbientOcclusionTypeSetting::HBAO:
					m_Options.EnableHBAO = true;
					break;
				case Tiering::Renderer::AmbientOcclusionTypeSetting::GTAO:
					m_Options.EnableGTAO = true;
					break;
				}
			}
		}

		m_CommandBuffer =Ref<VulkanRenderCommandBuffer>::Create(0, "SceneRenderer");

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		m_UniformBufferSet = Ref<VulkanUniformBufferSet>::Create(framesInFlight);
		m_UniformBufferSet->Create(sizeof(UBCamera), 0);
		m_UniformBufferSet->Create(sizeof(UBShadow), 1);
		m_UniformBufferSet->Create(sizeof(UBScene), 2);
		m_UniformBufferSet->Create(sizeof(UBRendererData), 3);
		m_UniformBufferSet->Create(sizeof(UBPointLights), 4);
		m_UniformBufferSet->Create(sizeof(UBScreenData), 17);
		m_UniformBufferSet->Create(sizeof(UBSpotLights), 19);
		m_UniformBufferSet->Create(sizeof(UBHBAOData), 18);
		m_UniformBufferSet->Create(sizeof(UBSpotShadowData), 20);
		m_UniformBufferSet->Create(sizeof(UBSMAAData), 24);
		m_UniformBufferSet->Create(sizeof(UBTAAData), 25);
		m_UniformBufferSet->Create(sizeof(UBRayMarchingData), 26);



		m_Renderer2D = Ref<Renderer2D>::Create();
		m_DebugRenderer = Ref<DebugRenderer>::Create();

		m_CompositeShader = Renderer::GetShaderLibrary()->Get("SceneComposite");
		m_CompositeMaterial = Ref<VulkanMaterial>::Create(m_CompositeShader);

		// Light culling compute pipeline
		{
			m_StorageBufferSet = Ref<VulkanStorageBufferSet>::Create(framesInFlight);
			m_StorageBufferSet->Create(1, Binding::VisiblePointLightIndicesBuffer); //Can't allocate 0 bytes.. Resized later
			m_StorageBufferSet->Create(1, Binding::VisibleSpotLightIndicesBuffer); //Can't allocate 0 bytes.. Resized later

			m_LightCullingMaterial = Ref<VulkanMaterial>::Create(Renderer::GetShaderLibrary()->Get("LightCulling"), "LightCulling");
			Ref<VulkanShader> lightCullingShader = Renderer::GetShaderLibrary()->Get("LightCulling");
			m_LightCullingPipeline = Ref<VulkanComputePipeline>::Create(lightCullingShader);
		}

		VertexBufferLayout vertexLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Binormal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		VertexBufferLayout instanceLayout = {
			{ ShaderDataType::Float4, "a_MRow0" },
			{ ShaderDataType::Float4, "a_MRow1" },
			{ ShaderDataType::Float4, "a_MRow2" },

			{ ShaderDataType::Float4, "a_MRowPrev0" },
			{ ShaderDataType::Float4, "a_MRowPrev1" },
			{ ShaderDataType::Float4, "a_MRowPrev2" },
		};

		VertexBufferLayout boneInfluenceLayout = {
			{ ShaderDataType::Int4,   "a_BoneIDs" },
			{ ShaderDataType::Float4, "a_BoneWeights" },
		};

		uint32_t shadowMapResolution = 4096;
		switch (m_Specification.Tiering.ShadowResolution)
		{
		case Tiering::Renderer::ShadowResolutionSetting::Low:
			shadowMapResolution = 1024;
			break;
		case Tiering::Renderer::ShadowResolutionSetting::Medium:
			shadowMapResolution = 2048;
			break;
		case Tiering::Renderer::ShadowResolutionSetting::High:
			shadowMapResolution = 4096;
			break;
		}

		// Directional Shadow pass
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::DEPTH32F;
			spec.Usage = ImageUsage::Attachment;
			spec.Width = shadowMapResolution;
			spec.Height = shadowMapResolution;
			spec.Layers = 4; // 4 cascades
			spec.DebugName = "ShadowCascades";
			Ref<VulkanImage2D> cascadedDepthImage = Ref<VulkanImage2D>::Create(spec);
			cascadedDepthImage->Invalidate();
			cascadedDepthImage->CreatePerLayerImageViews();

			FramebufferSpecification shadowMapFramebufferSpec;
			shadowMapFramebufferSpec.DebugName = "Dir Shadow Map";
			shadowMapFramebufferSpec.Width = shadowMapResolution;
			shadowMapFramebufferSpec.Height = shadowMapResolution;
			shadowMapFramebufferSpec.Attachments = { ImageFormat::DEPTH32F };
			shadowMapFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			shadowMapFramebufferSpec.DepthClearValue = 1.0f;
			shadowMapFramebufferSpec.NoResize = true;
			shadowMapFramebufferSpec.ExistingImage = cascadedDepthImage;

			auto shadowPassShader = Renderer::GetShaderLibrary()->Get("DirShadowMap");
			auto shadowPassShaderAnim = Renderer::GetShaderLibrary()->Get("DirShadowMap_Anim");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "DirShadowPass";
			pipelineSpec.Shader = shadowPassShader;
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;
			pipelineSpec.Layout = vertexLayout;
			pipelineSpec.InstanceLayout = instanceLayout;

			PipelineSpecification pipelineSpecAnim = pipelineSpec;
			pipelineSpecAnim.DebugName = "DirShadowPass-Anim";
			pipelineSpecAnim.Shader = shadowPassShaderAnim;
			pipelineSpecAnim.BoneInfluenceLayout = boneInfluenceLayout;

			// 4 cascades
			for (int i = 0; i < 4; i++)
			{
				shadowMapFramebufferSpec.ExistingImageLayers.clear();
				shadowMapFramebufferSpec.ExistingImageLayers.emplace_back(i);

				RenderPassSpecification shadowMapRenderPassSpec;
				shadowMapRenderPassSpec.DebugName = shadowMapFramebufferSpec.DebugName;
				shadowMapRenderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(shadowMapFramebufferSpec);
				auto renderpass = Ref<VulkanRenderPass>::Create(shadowMapRenderPassSpec);

				pipelineSpec.RenderPass = renderpass;
				m_ShadowPassPipelines[i] = Ref<VulkanPipeline>::Create(pipelineSpec);

				pipelineSpecAnim.RenderPass = renderpass;
				m_ShadowPassPipelinesAnim[i] = Ref<VulkanPipeline>::Create(pipelineSpecAnim);
			}
			m_ShadowPassMaterial = Ref<VulkanMaterial>::Create(shadowPassShader, "DirShadowPass");
		}

		// Non-directional shadow mapping pass
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Width = shadowMapResolution; // TODO(Yan): could probably halve these
			framebufferSpec.Height = shadowMapResolution;
			framebufferSpec.Attachments = { ImageFormat::DEPTH32F };
			framebufferSpec.DepthClearValue = 1.0f;
			framebufferSpec.NoResize = true;
			framebufferSpec.DebugName = "Spot Shadow Map";

			auto shadowPassShader = Renderer::GetShaderLibrary()->Get("SpotShadowMap");
			auto shadowPassShaderAnim = Renderer::GetShaderLibrary()->Get("SpotShadowMap_Anim");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "SpotShadowPass";
			pipelineSpec.Shader = shadowPassShader;
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;

			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float3, "a_Normal" },
				{ ShaderDataType::Float3, "a_Tangent" },
				{ ShaderDataType::Float3, "a_Binormal" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.InstanceLayout = {
				{ ShaderDataType::Float4, "a_MRow0" },
				{ ShaderDataType::Float4, "a_MRow1" },
				{ ShaderDataType::Float4, "a_MRow2" },

				{ ShaderDataType::Float4, "a_MRowPrev0" },
				{ ShaderDataType::Float4, "a_MRowPrev1" },
				{ ShaderDataType::Float4, "a_MRowPrev2" },
			};

			RenderPassSpecification shadowMapRenderPassSpec;
			shadowMapRenderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			shadowMapRenderPassSpec.DebugName = "SpotShadowMap";
			pipelineSpec.RenderPass = Ref<VulkanRenderPass>::Create(shadowMapRenderPassSpec);

			PipelineSpecification pipelineSpecAnim = pipelineSpec;
			pipelineSpecAnim.DebugName = "SpotShadowPass-Anim";
			pipelineSpecAnim.Shader = shadowPassShaderAnim;
			pipelineSpecAnim.BoneInfluenceLayout = boneInfluenceLayout;

			m_SpotShadowPassPipeline = Ref<VulkanPipeline>::Create(pipelineSpec);
			m_SpotShadowPassAnimPipeline = Ref<VulkanPipeline>::Create(pipelineSpecAnim);

			m_SpotShadowPassMaterial = Ref<VulkanMaterial>::Create(shadowPassShader, "SpotShadowPass");
		}

		// PreDepth
		{
			FramebufferSpecification preDepthFramebufferSpec;
			preDepthFramebufferSpec.DebugName = "PreDepth-Opaque";
			//Linear depth, reversed device depth
			preDepthFramebufferSpec.Attachments = { /*ImageFormat::RED32F, */ ImageFormat::DEPTH32FSTENCIL8UINT };
			preDepthFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			preDepthFramebufferSpec.DepthClearValue = 1.0f;

			RenderPassSpecification preDepthRenderPassSpec;
			preDepthRenderPassSpec.DebugName = preDepthFramebufferSpec.DebugName;
			preDepthRenderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(preDepthFramebufferSpec);

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = preDepthFramebufferSpec.DebugName;
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;

			pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth");
			pipelineSpec.Layout = vertexLayout;
			pipelineSpec.InstanceLayout = instanceLayout;
			pipelineSpec.RenderPass = Ref<VulkanRenderPass>::Create(preDepthRenderPassSpec);
			m_PreDepthPipeline = Ref<VulkanPipeline>::Create(pipelineSpec);
			m_PreDepthMaterial = Ref<VulkanMaterial>::Create(pipelineSpec.Shader, pipelineSpec.DebugName);

			// TAA PreDepth;
			pipelineSpec.DebugName = "PreDepth-TAA";
			pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth_TAA");
			m_PreDepthTAAPipeline = Ref<VulkanPipeline>::Create(pipelineSpec);
			m_PreDepthTAAMaterial = Ref<VulkanMaterial>::Create(pipelineSpec.Shader, pipelineSpec.DebugName);

			pipelineSpec.DebugName = "PreDepth-Anim";
			pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth_Anim");
			pipelineSpec.BoneInfluenceLayout = boneInfluenceLayout;
			m_PreDepthPipelineAnim = Ref<VulkanPipeline>::Create(pipelineSpec);  // same renderpass as Predepth-Opaque

			pipelineSpec.DebugName = "PreDepth-Transparent";
			pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth");
			preDepthFramebufferSpec.DebugName = pipelineSpec.DebugName;
			preDepthRenderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(preDepthFramebufferSpec);
			preDepthRenderPassSpec.DebugName = pipelineSpec.DebugName;
			pipelineSpec.RenderPass = Ref<VulkanRenderPass>::Create(preDepthRenderPassSpec);
			m_PreDepthTransparentPipeline = Ref<VulkanPipeline>::Create(pipelineSpec);

			// TODO(0x): Need PreDepth-Transparent-Animated pipeline

		}

		// Hierarchical Z buffer
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::RED32F;
			spec.Width = 1;
			spec.Height = 1;
			spec.SamplerWrap = TextureWrap::Clamp;
			spec.SamplerFilter = TextureFilter::Nearest;
			spec.DebugName = "HierarchicalZ";

			m_HierarchicalDepthTexture = Ref<VulkanTexture2D>::Create(spec);

			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("HZB");
			m_HierarchicalDepthPipeline = Ref<VulkanComputePipeline>::Create(shader);
			m_HierarchicalDepthMaterial = Ref<VulkanMaterial>::Create(shader, "HZB");
		}

		// Pre-Integration
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::RED8UN;
			spec.Width = 1;
			spec.Height = 1;
			spec.DebugName = "Pre-Integration";

			m_VisibilityTexture = Ref<VulkanTexture2D>::Create(spec);

			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("Pre-Integration");
			m_PreIntegrationPipeline = Ref<VulkanComputePipeline>::Create(shader);
			m_PreIntegrationMaterial = Ref<VulkanMaterial>::Create(shader, "Pre-Integration");
		}

		//TAA Velocity Texture
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RG16F;
			imageSpec.Layers = 1;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.DebugName = "TAA-Velocity";
			m_TAAVelocityImage = Ref<VulkanImage2D>::Create(imageSpec);
			m_TAAVelocityImage->Invalidate();
			//m_TAAVelocityImage->CreatePerLayerImageViews();
		}

		// Geometry
		{
			FramebufferSpecification geoFramebufferSpec;

			geoFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::RGBA16F, ImageFormat::RGBA,ImageFormat::RG16F, ImageFormat::DEPTH32FSTENCIL8UINT };
			geoFramebufferSpec.ExistingImages[3] = m_TAAVelocityImage;
			geoFramebufferSpec.ExistingImages[4] = m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage();

			// Don't blend with luminance in the alpha channel.
			geoFramebufferSpec.Attachments.Attachments[1].Blend = false;
			geoFramebufferSpec.Attachments.Attachments[3].Blend = false; //Velocity need not blend
			geoFramebufferSpec.Samples = 1;
			geoFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			geoFramebufferSpec.DebugName = "Geometry";
			geoFramebufferSpec.ClearDepthOnLoad = false;
			Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(geoFramebufferSpec);

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = geoFramebufferSpec.DebugName;
			renderPassSpec.TargetFramebuffer = framebuffer;
			Ref<VulkanRenderPass> renderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "PBR-Static";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Static");
			pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.Layout = vertexLayout;
			pipelineSpecification.InstanceLayout = instanceLayout;
			pipelineSpecification.LineWidth = m_LineWidth;
			pipelineSpecification.RenderPass = renderPass;
			m_GeometryPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			//
			// TAA Geometry
			//
			pipelineSpecification.DebugName = "PBR-STATIC-TAA";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Static_TAA");
			pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
			m_GeometryTAAPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);


			//
			// Transparent Geometry
			//
			pipelineSpecification.DebugName = "PBR-Transparent";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Transparent");
			pipelineSpecification.DepthOperator = DepthCompareOperator::GreaterOrEqual;
			m_TransparentGeometryPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			//
			// Animated Geometry
			//
			pipelineSpecification.DebugName = "PBR-Anim";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Anim");
			pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
			pipelineSpecification.BoneInfluenceLayout = boneInfluenceLayout;
			m_GeometryPipelineAnim = Ref<VulkanPipeline>::Create(pipelineSpecification);

			// TODO(0x): Need Transparent-Animated geometry pipeline

		}
		

		// Selected Geometry isolation (for outline pass)
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.DebugName = "SelectedGeometry";
			framebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			framebufferSpec.Samples = 1;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			framebufferSpec.DepthClearValue = 1.0f;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			Ref<VulkanRenderPass> renderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("SelectedGeometry");
			pipelineSpecification.Layout = vertexLayout;
			pipelineSpecification.InstanceLayout = instanceLayout;
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.DepthOperator = DepthCompareOperator::LessOrEqual;
			m_SelectedGeometryPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_SelectedGeometryMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);

			pipelineSpecification.DebugName = "SelectedGeometry-Anim";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("SelectedGeometry_Anim");
			pipelineSpecification.BoneInfluenceLayout = boneInfluenceLayout;
			m_SelectedGeometryPipelineAnim = Ref<VulkanPipeline>::Create(pipelineSpecification); // Note: same framebuffer and renderpass as m_SelectedGeometryPipeline
		}

		// Pre-convolution Compute
		{
			auto shader = Renderer::GetShaderLibrary()->Get("Pre-Convolution");
			m_GaussianBlurPipeline = Ref<VulkanComputePipeline>::Create(shader);
			TextureSpecification spec;
			spec.Format = ImageFormat::RGBA32F;
			spec.Width = 1;
			spec.Height = 1;
			spec.SamplerWrap = TextureWrap::Clamp;
			spec.DebugName = "Pre-Convoluted";
			m_PreConvolutedTexture = Ref<VulkanTexture2D>::Create(spec);

			m_GaussianBlurMaterial = Ref<VulkanMaterial>::Create(shader);
		}

		// Bloom Compute
		{
			auto shader = Renderer::GetShaderLibrary()->Get("Bloom");
			m_BloomComputePipeline = Ref<VulkanComputePipeline>::Create(shader);
			TextureSpecification spec;
			spec.Format = ImageFormat::RGBA32F;
			spec.Width = 1;
			spec.Height = 1;
			spec.SamplerWrap = TextureWrap::Clamp;
			spec.Storage = true;
			spec.GenerateMips = true;
			spec.DebugName = "BloomCompute-0";
			m_BloomComputeTextures[0] = Ref<VulkanTexture2D>::Create(spec);
			spec.DebugName = "BloomCompute-1";
			m_BloomComputeTextures[1] = Ref<VulkanTexture2D>::Create(spec);
			spec.DebugName = "BloomCompute-2";
			m_BloomComputeTextures[2] = Ref<VulkanTexture2D>::Create(spec);
			m_BloomComputeMaterial = Ref<VulkanMaterial>::Create(shader);

			m_BloomDirtTexture = Renderer::GetBlackTexture();
		}

		// Deinterleaving
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RED32F;
			imageSpec.Layers = 16;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.DebugName = "Deinterleaved";
			Ref<VulkanImage2D> image = Ref<VulkanImage2D>::Create(imageSpec);
			image->Invalidate();
			image->CreatePerLayerImageViews();

			FramebufferSpecification deinterleavingFramebufferSpec;
			deinterleavingFramebufferSpec.Attachments.Attachments = { 8, FramebufferTextureSpecification{ ImageFormat::RED32F } };
			deinterleavingFramebufferSpec.ClearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
			deinterleavingFramebufferSpec.DebugName = "Deinterleaving";
			deinterleavingFramebufferSpec.ExistingImage = image;

			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("Deinterleaving");
			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Deinterleaving";
			pipelineSpec.DepthWrite = false;
			pipelineSpec.DepthTest = false;

			pipelineSpec.Shader = shader;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};

			// 2 frame buffers, 2 render passes .. 8 attachments each
			for (int rp = 0; rp < 2; rp++)
			{
				deinterleavingFramebufferSpec.ExistingImageLayers.clear();
				for (int layer = 0; layer < 8; layer++)
					deinterleavingFramebufferSpec.ExistingImageLayers.emplace_back(rp * 8 + layer);

				Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(deinterleavingFramebufferSpec);

				RenderPassSpecification deinterleavingRenderPassSpec;
				deinterleavingRenderPassSpec.TargetFramebuffer = framebuffer;
				deinterleavingRenderPassSpec.DebugName = "Deinterleaving";
				pipelineSpec.RenderPass = Ref<VulkanRenderPass>::Create(deinterleavingRenderPassSpec);

				m_DeinterleavingPipelines[rp] = Ref<VulkanPipeline>::Create(pipelineSpec);
			}
			m_DeinterleavingMaterial = Ref<VulkanMaterial>::Create(pipelineSpec.Shader, pipelineSpec.DebugName);

		}

		// HBAO
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RG16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Layers = 16;
			imageSpec.DebugName = "HBAO-Output";
			Ref<VulkanImage2D> image = Ref<VulkanImage2D>::Create(imageSpec);
			image->Invalidate();
			image->CreatePerLayerImageViews();

			m_HBAOOutputImage = image;

			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("HBAO");

			m_HBAOPipeline = Ref<VulkanComputePipeline>::Create(shader);
			m_HBAOMaterial = Ref<VulkanMaterial>::Create(shader, "HBAO");

			for (int i = 0; i < 16; i++)
				HBAODataUB.Float2Offsets[i] = glm::vec4((float)(i % 4) + 0.5f, (float)(i / 4) + 0.5f, 0.0f, 1.f);
			std::memcpy(HBAODataUB.Jitters, Noise::HBAOJitter().data(), sizeof glm::vec4 * 16);
		}

		// GTAO
		{
			{
				ImageSpecification imageSpec;
				if (m_Options.GTAOBentNormals)
					imageSpec.Format = ImageFormat::RED32UI;
				else
					imageSpec.Format = ImageFormat::RED8UI;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.Layers = 1;
				imageSpec.DebugName = "GTAO";
				m_GTAOOutputImage = Ref<VulkanImage2D>::Create(imageSpec);
				m_GTAOOutputImage->Invalidate();
			}

			// GTAO-Edges
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::RED8UN;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.DebugName = "GTAO-Edges";
				m_GTAOEdgesOutputImage = Ref<VulkanImage2D>::Create(imageSpec);
				m_GTAOEdgesOutputImage->Invalidate();
			}


			// GTAO-debug
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::RGBA32F;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.DebugName = "GTAO-Debug";
				m_GTAODebugOutputImage = Ref<VulkanImage2D>::Create(imageSpec);
				m_GTAODebugOutputImage->Invalidate();
			}

			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("GTAO");

			m_GTAOPipeline = Ref<VulkanComputePipeline>::Create(shader);
			m_GTAOMaterial = Ref<VulkanMaterial>::Create(shader, "GTAO");
		}

		// GTAO Denoise
		{
			{
				ImageSpecification imageSpec;
				if (m_Options.GTAOBentNormals)
					imageSpec.Format = ImageFormat::RED32UI;
				else
					imageSpec.Format = ImageFormat::RED8UI;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.Layers = 1;
				imageSpec.DebugName = "GTAO-Denoise";
				m_GTAODenoiseImage = Ref<VulkanImage2D>::Create(imageSpec);
				m_GTAODenoiseImage->Invalidate();

				Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("GTAO-Denoise");
				m_GTAODenoiseMaterial[0] = Ref<VulkanMaterial>::Create(shader, "GTAO-Denoise-Ping");
				m_GTAODenoiseMaterial[1] = Ref<VulkanMaterial>::Create(shader, "GTAO-Denoise-Pong");

				m_GTAODenoisePipeline = Ref<VulkanComputePipeline>::Create(shader);
			}

			// GTAO Composite
			{
				PipelineSpecification aoCompositePipelineSpec;
				aoCompositePipelineSpec.DebugName = "AO-Composite";
				RenderPassSpecification renderPassSpec;
				renderPassSpec.DebugName = "AO-Composite";
				FramebufferSpecification framebufferSpec;
				framebufferSpec.DebugName = "AO-Composite";
				framebufferSpec.Attachments = { ImageFormat::RGBA32F };
				framebufferSpec.ExistingImages[0] = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
				framebufferSpec.Blend = true;
				framebufferSpec.ClearColorOnLoad = false;
				framebufferSpec.BlendMode = FramebufferBlendMode::Zero_SrcColor;

				renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
				aoCompositePipelineSpec.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
				m_AOCompositeRenderPass = aoCompositePipelineSpec.RenderPass;
				aoCompositePipelineSpec.DepthTest = false;
				aoCompositePipelineSpec.Layout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" },
				};
				aoCompositePipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("AO-Composite");
				m_AOCompositePipeline = Ref<VulkanPipeline>::Create(aoCompositePipelineSpec);
				m_AOCompositeMaterial = Ref<VulkanMaterial>::Create(aoCompositePipelineSpec.Shader, "GTAO-Composite");
			}
		}

		//Reinterleaving
		{
			FramebufferSpecification reinterleavingFramebufferSpec;
			reinterleavingFramebufferSpec.Attachments = { ImageFormat::RG16F, };
			reinterleavingFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			reinterleavingFramebufferSpec.DebugName = "Reinterleaving";

			Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(reinterleavingFramebufferSpec);
			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("Reinterleaving");
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = shader;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = framebuffer;
			renderPassSpec.DebugName = "Reinterleaving";
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
			pipelineSpecification.DebugName = "Reinterleaving";
			pipelineSpecification.DepthWrite = false;
			m_ReinterleavingPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			m_ReinterleavingMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
		}

		//HBAO Blur
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;

			auto shader = Renderer::GetShaderLibrary()->Get("HBAOBlur");

			FramebufferSpecification hbaoBlurFramebufferSpec;
			hbaoBlurFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			hbaoBlurFramebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::RG16F);

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = hbaoBlurFramebufferSpec.DebugName;

			pipelineSpecification.Shader = shader;
			pipelineSpecification.DebugName = "HBAOBlur";
			// First blur pass
			{
				pipelineSpecification.DebugName = "HBAOBlur1";
				hbaoBlurFramebufferSpec.DebugName = "HBAOBlur1";
				renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(hbaoBlurFramebufferSpec);
				pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
				m_HBAOBlurPipelines[0] = Ref<VulkanPipeline>::Create(pipelineSpecification);
			}
			// Second blur pass
			{
				pipelineSpecification.DebugName = "HBAOBlur2";
				hbaoBlurFramebufferSpec.DebugName = "HBAOBlur2";
				renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(hbaoBlurFramebufferSpec);
				pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
				m_HBAOBlurPipelines[1] = Ref<VulkanPipeline>::Create(pipelineSpecification);
			}
			m_HBAOBlurMaterials[0] = Ref<VulkanMaterial>::Create(shader, "HBAOBlur");
			m_HBAOBlurMaterials[1] = Ref<VulkanMaterial>::Create(shader, "HBAOBlur2");

		}

		//SSR
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.DebugName = "SSR";
			Ref<VulkanImage2D> image = Ref<VulkanImage2D>::Create(imageSpec);
			image->Invalidate();

			m_SSRImage = image;

			Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("SSR");

			m_SSRPipeline = Ref<VulkanComputePipeline>::Create(shader);
			m_SSRMaterial = Ref<VulkanMaterial>::Create(shader, "SSR");
		}

		//SSR Composite
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "SSR-Composite";
			auto shader = Renderer::GetShaderLibrary()->Get("SSR-Composite");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::RGBA32F);
			framebufferSpec.ExistingImages[0] = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
			framebufferSpec.DebugName = "SSR-Composite";
			framebufferSpec.BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
			framebufferSpec.ClearColorOnLoad = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_SSRCompositePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_SSRCompositeMaterial = Ref<VulkanMaterial>::Create(shader, "SSR-Composite");
		}

		// Composite
		{
			FramebufferSpecification compFramebufferSpec;
			compFramebufferSpec.DebugName = "SceneComposite";
			compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			compFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

			Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(compFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("SceneComposite");

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = framebuffer;
			renderPassSpec.DebugName = "SceneComposite";
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
			pipelineSpecification.DebugName = "SceneComposite";
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DepthTest = false;
			m_CompositePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
		}



		//SMAA EdgeDetection
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "SMAA-EdgeDetection";
			auto shader = Renderer::GetShaderLibrary()->Get("SMAAEdgeDetect");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments = { ImageFormat::RGBA32F }; // recode edge
			framebufferSpec.DebugName = "SMAA-EdgeDetection";
			framebufferSpec.BlendMode = FramebufferBlendMode::OneZero;
			framebufferSpec.ClearColorOnLoad = true;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_SMAAEdgeDetectionPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_SMAAEdgeDetectionMaterial = Ref<VulkanMaterial>::Create(shader, "SMAA-EdgeDetection");
		}

		//SMAA Weight
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "SMAA-BlendWeight";
			auto shader = Renderer::GetShaderLibrary()->Get("SMAABlendWeight");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::RGBA32F}; // &Geometry Color & Blend weight
			framebufferSpec.DebugName = "SMAA-BlendWeight";
			framebufferSpec.BlendMode = FramebufferBlendMode::OneZero;
			framebufferSpec.ClearColorOnLoad = true;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_SMAABlendWeightPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_SMAABlendWeightMaterial = Ref<VulkanMaterial>::Create(shader, "SMAA-BlendWeight");
		}

		//SMAA Blend
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "SMAA-NeighborBlend";
			auto shader = Renderer::GetShaderLibrary()->Get("SMAANeighborBlend");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::RGBA32F);
			framebufferSpec.ExistingImages[0] = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
			framebufferSpec.DebugName = "SMAA-NeighborBlend";
			framebufferSpec.Attachments.Attachments[0].Blend = false;
			framebufferSpec.ClearColorOnLoad = true;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_SMAANeighborBlendPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_SMAANeighborBlendMaterial = Ref<VulkanMaterial>::Create(shader, "SMAA-NeighborBlend");
		}

		//TAA
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RGBA32F;
			imageSpec.DebugName = "TAA_Color_Image0";
			Ref<VulkanImage2D> image0 = Ref<VulkanImage2D>::Create(imageSpec);
			image0->Invalidate();
			m_TAAToneMappedPreColorImage = image0;

			//imageSpec.DebugName = "TAA_Color_Image1";
			//Ref<VulkanImage2D> image1 = Ref<VulkanImage2D>::Create(imageSpec);
			//image1->Invalidate();
			////m_TAACurColorImage = image1;


			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "TAA";
			auto shader = Renderer::GetShaderLibrary()->Get("TAA");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::RGBA32F);
			//framebufferSpec.ExistingImages[0] = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
			framebufferSpec.DebugName = "TAA";
			framebufferSpec.Attachments.Attachments[0].Blend = false;
			framebufferSpec.ClearColorOnLoad = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_TAAPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_TAAMaterial = Ref<VulkanMaterial>::Create(shader, "TAA");
		}

		//TAA ToneMapping 
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "TAA-ToneMapping";
			auto shader = Renderer::GetShaderLibrary()->Get("TAA_ToneMapping");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments = { ImageFormat::RGBA32F }; 
			//framebufferSpec.ExistingImages[0] = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
			framebufferSpec.Attachments.Attachments[0].Blend = true;

			framebufferSpec.DebugName = "TAA-ToneMapping";
			framebufferSpec.ClearColorOnLoad = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_TAAToneMappingPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_TAAToneMappingMaterial = Ref<VulkanMaterial>::Create(shader, "TAA-ToneMapping");
		}

		//TAA UnMapping 
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "TAA-ToneUnMapping";
			auto shader = Renderer::GetShaderLibrary()->Get("TAA_ToneUnMapping");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments = { ImageFormat::RGBA32F };
			framebufferSpec.ExistingImages[0] = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>();
			framebufferSpec.Attachments.Attachments[0].Blend = false;

			framebufferSpec.DebugName = "TAA-ToneUnMapping";
			framebufferSpec.ClearColorOnLoad = true;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = Ref<VulkanFramebuffer>::Create(framebufferSpec);
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			m_TAAToneUnMappingPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_TAAToneUnMappingMaterial = Ref<VulkanMaterial>::Create(shader, "TAA-ToneUnMapping");
		}


		// Blue Noise Loading
		m_BlueNoiseTextures.resize(m_Options.NUM_BLUE_NOISE_TEXTURES);

		TextureSpecification spec;
		spec.SamplerWrap = TextureWrap::Clamp;

		for (uint32_t n = 0; n < m_Options.NUM_BLUE_NOISE_TEXTURES; ++n)
		{
			std::string noisePath = std::string(PROJECT_ROOT) + "Resources/Renderer/BlueNoise_256_px/LDR_LLL1_" + std::to_string(n) + ".png";
			m_BlueNoiseTextures[n] = Ref<VulkanTexture2D>::Create(spec, std::filesystem::path(noisePath));
		}

		// Ray Marching 
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::RGBA16F;
			spec.Usage = ImageUsage::Storage;
			spec.Width = m_Options.VOXEL_GRID_SIZE_X;
			spec.Height = m_Options.VOXEL_GRID_SIZE_Y;
			spec.Depth = m_Options.VOXEL_GRID_SIZE_Z; 
			
			//1.Light Injection
			{
				spec.DebugName = "Light Injection Grid 0";
				m_lightInjectionImage[0] = Ref<VulkanImage2D>::Create(spec);
				m_lightInjectionImage[0]->Invalidate();

				spec.DebugName = "Light Injection Grid 1";
				m_lightInjectionImage[1] = Ref<VulkanImage2D>::Create(spec);
				m_lightInjectionImage[1]->Invalidate();

				Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("RayMarchingLightInjection");
				m_RayInjectionPipeline = Ref<VulkanComputePipeline>::Create(shader);

				m_RayInjectionMaterial[0] = Ref<VulkanMaterial>::Create(shader, "RayMarchingLightInjection 0");
				m_RayInjectionMaterial[1] = Ref<VulkanMaterial>::Create(shader, "RayMarchingLightInjection 1");
			}

			// 2.Scattering
			{
				spec.DebugName = "Scattering Grid";
				m_ScatteringImage = Ref<VulkanImage2D>::Create(spec);
				m_ScatteringImage->Invalidate();

				Ref<VulkanShader> shader = Renderer::GetShaderLibrary()->Get("RayMarchingScattering");
				m_ScatteringPipeline = Ref<VulkanComputePipeline>::Create(shader);
				m_ScatteringMaterial[0] = Ref<VulkanMaterial>::Create(shader, "RayMarchingScattering 0 ");
				m_ScatteringMaterial[1] = Ref<VulkanMaterial>::Create(shader, "RayMarchingScattering 1 ");
			}
		}


		// DOF
		{
			FramebufferSpecification compFramebufferSpec;
			compFramebufferSpec.DebugName = "POST-DepthOfField";
			compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			compFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

			Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(compFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("DOF");

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = framebuffer;
			renderPassSpec.DebugName = compFramebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
			pipelineSpecification.DebugName = compFramebufferSpec.DebugName;
			pipelineSpecification.DepthWrite = false;
			m_DOFPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_DOFMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
		}

		// Edge Detection
		if (false)
		{
			FramebufferSpecification compFramebufferSpec;
			compFramebufferSpec.DebugName = "POST-EdgeDetection";
			compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			compFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

			Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(compFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("EdgeDetection");

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = framebuffer;
			renderPassSpec.DebugName = compFramebufferSpec.DebugName;
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
			pipelineSpecification.DebugName = compFramebufferSpec.DebugName;
			pipelineSpecification.DepthWrite = false;
			m_EdgeDetectionPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_EdgeDetectionMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
		}

		// External compositing
		{
			FramebufferSpecification extCompFramebufferSpec;
			extCompFramebufferSpec.DebugName = "External-Composite";
			extCompFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::DEPTH32FSTENCIL8UINT };
			extCompFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			extCompFramebufferSpec.ClearColorOnLoad = false;
			extCompFramebufferSpec.ClearDepthOnLoad = false;
			// Use the color buffer from the final compositing pass, but the depth buffer from
			// the actual 3D geometry pass, in case we want to composite elements behind meshes
			// in the scene
			extCompFramebufferSpec.ExistingImages[0] = m_CompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage();
			extCompFramebufferSpec.ExistingImages[1] = m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage();
			Ref<VulkanFramebuffer> framebuffer = Ref<VulkanFramebuffer>::Create(extCompFramebufferSpec);

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = extCompFramebufferSpec.DebugName;
			renderPassSpec.TargetFramebuffer = framebuffer;
			m_ExternalCompositeRenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Wireframe";
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Wireframe = true;
			pipelineSpecification.DepthTest = true;
			pipelineSpecification.LineWidth = 2.0f;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Wireframe");
			pipelineSpecification.Layout = vertexLayout;
			pipelineSpecification.InstanceLayout = instanceLayout;
			pipelineSpecification.RenderPass = m_ExternalCompositeRenderPass;
			m_GeometryWireframePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DebugName = "Wireframe-OnTop";
			m_GeometryWireframeOnTopPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);

			pipelineSpecification.DepthTest = true;
			pipelineSpecification.DebugName = "Wireframe-Anim";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Wireframe_Anim");
			pipelineSpecification.BoneInfluenceLayout = boneInfluenceLayout;
			m_GeometryWireframePipelineAnim = Ref<VulkanPipeline>::Create(pipelineSpecification); // Note: same framebuffer and renderpass as m_GeometryWireframePipeline

			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DebugName = "Wireframe-Anim-OnTop";
			m_GeometryWireframeOnTopPipelineAnim = Ref<VulkanPipeline>::Create(pipelineSpecification);

		}

		// Read-back Image
		if (false) // WIP
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::RGBA32F;
			spec.Usage = ImageUsage::HostRead;
			spec.Transfer = true;
			spec.DebugName = "ReadBack";
			m_ReadBackImage = Ref<VulkanImage2D>::Create(spec);
		}

		// Temporary framebuffers for re-use
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { ImageFormat::RGBA32F };
			framebufferSpec.Samples = 1;
			framebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			framebufferSpec.BlendMode = FramebufferBlendMode::OneZero;
			framebufferSpec.DebugName = "Temporaries";

			for (uint32_t i = 0; i < 2; i++)
				m_TempFramebuffers.emplace_back(Ref<VulkanFramebuffer>::Create(framebufferSpec));
		}

		// Jump Flood (outline)
		{
			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "JumpFlood-Init";
			renderPassSpec.TargetFramebuffer = m_TempFramebuffers[0];

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = renderPassSpec.DebugName;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("JumpFlood_Init");
			m_JumpFloodInitPipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
			m_JumpFloodInitMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);

			const char* passName[2] = { "EvenPass", "OddPass" };
			for (uint32_t i = 0; i < 2; i++)
			{
				renderPassSpec.TargetFramebuffer = m_TempFramebuffers[(i + 1) % 2];
				renderPassSpec.DebugName = fmt::format("JumpFlood-{0}", passName[i]);

				pipelineSpecification.DebugName = renderPassSpec.DebugName;
				pipelineSpecification.RenderPass = Ref<VulkanRenderPass>::Create(renderPassSpec);
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("JumpFlood_Pass");
				m_JumpFloodPassPipeline[i] = Ref<VulkanPipeline>::Create(pipelineSpecification);
				m_JumpFloodPassMaterial[i] = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
			}

			// Outline compositing
			{
				pipelineSpecification.RenderPass = m_CompositePipeline->GetSpecification().RenderPass;
				pipelineSpecification.DebugName = "JumpFlood-Composite";
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("JumpFlood_Composite");
				pipelineSpecification.DepthTest = false;
				m_JumpFloodCompositePipeline = Ref<VulkanPipeline>::Create(pipelineSpecification);
				m_JumpFloodCompositeMaterial = Ref<VulkanMaterial>::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
			}
		}

		// Grid
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Grid";
			pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("Grid");
			pipelineSpec.BackfaceCulling = false;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.RenderPass = m_ExternalCompositeRenderPass;
			m_GridPipeline = Ref<VulkanPipeline>::Create(pipelineSpec);

			const float gridScale = 16.025f;
			const float gridSize = 0.025f;
			m_GridMaterial = Ref<VulkanMaterial>::Create(pipelineSpec.Shader, pipelineSpec.DebugName);
			m_GridMaterial->Set("u_Settings.Scale", gridScale);
			m_GridMaterial->Set("u_Settings.Size", gridSize);
		}

		// Collider
		m_SimpleColliderMaterial = Ref<VulkanMaterial>::Create(Renderer::GetShaderLibrary()->Get("Wireframe"), "SimpleCollider");
		m_SimpleColliderMaterial->Set("u_MaterialUniforms.Color", m_Options.SimplePhysicsCollidersColor);
		m_ComplexColliderMaterial = Ref<VulkanMaterial>::Create(Renderer::GetShaderLibrary()->Get("Wireframe"), "ComplexCollider");
		m_ComplexColliderMaterial->Set("u_MaterialUniforms.Color", m_Options.ComplexPhysicsCollidersColor);

		m_WireframeMaterial = Ref<VulkanMaterial>::Create(Renderer::GetShaderLibrary()->Get("Wireframe"), "Wireframe");
		m_WireframeMaterial->Set("u_MaterialUniforms.Color", glm::vec4{ 1.0f, 0.5f, 0.0f, 1.0f });

		// Skybox
		{
			auto skyboxShader = Renderer::GetShaderLibrary()->Get("Skybox");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Skybox";
			pipelineSpec.Shader = skyboxShader;
			pipelineSpec.DepthWrite = false;
			pipelineSpec.DepthTest = false;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.RenderPass = m_GeometryPipeline->GetSpecification().RenderPass;
			m_SkyboxPipeline = Ref<VulkanPipeline>::Create(pipelineSpec);
			m_SkyboxMaterial = Ref<VulkanMaterial>::Create(pipelineSpec.Shader, pipelineSpec.DebugName);
			m_SkyboxMaterial->SetFlag(MaterialFlag::DepthTest, false);

		}

		// TODO(Yan): resizeable/flushable
		const size_t TransformBufferCount = 10 * 1024; // 10240 transforms
		m_SubmeshTransformBuffers.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_SubmeshTransformBuffers[i].Buffer = Ref<VulkanVertexBuffer>::Create(sizeof(TransformVertexData) * TransformBufferCount);
			m_SubmeshTransformBuffers[i].Data = hnew TransformVertexData[TransformBufferCount];
		}

		//const size_t BoneTransformBufferCount = 1 * 1024; // basically means limited to 1024 animated meshes   TODO(0x): resizeable/flushable
		//m_BoneTransformStorageBuffers.resize(Renderer::GetConfig().FramesInFlight);
		//for (auto& buffer : m_BoneTransformStorageBuffers)
		//{
		//	buffer = Ref<VulkanStorageBuffer>::Create(static_cast<uint32_t>(sizeof(BoneTransforms) * BoneTransformBufferCount), 0);
		//}
		//m_BoneTransformsData = hnew BoneTransforms[BoneTransformBufferCount];

		Renderer::Submit([instance = Ref(this)]() mutable { instance->m_ResourcesCreatedGPU = true; });

		InitMaterials();
		InitOptions();
	}

	void SceneRenderer::InitMaterials()
	{
		Ref<SceneRenderer> instance = this;
		Renderer::Submit([instance]() mutable
			{
				instance->m_HBAOBlurMaterials[0]->Set("u_InputTex", instance->m_ReinterleavingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());
				instance->m_HBAOBlurMaterials[1]->Set("u_InputTex", instance->m_HBAOBlurPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());

				instance->m_GTAODenoiseMaterial[0]->Set("u_Edges", instance->m_GTAOEdgesOutputImage);
				instance->m_GTAODenoiseMaterial[0]->Set("u_AOTerm", instance->m_GTAOOutputImage);
				instance->m_GTAODenoiseMaterial[0]->Set("o_AOTerm", instance->m_GTAODenoiseImage);

				instance->m_GTAODenoiseMaterial[1]->Set("u_Edges", instance->m_GTAOEdgesOutputImage);
				instance->m_GTAODenoiseMaterial[1]->Set("u_AOTerm", instance->m_GTAODenoiseImage);
				instance->m_GTAODenoiseMaterial[1]->Set("o_AOTerm", instance->m_GTAOOutputImage);


				//Froxel Volume Fog
				instance->m_RayInjectionMaterial[0]->Set("o_VoxelGrid", instance->m_lightInjectionImage[0]);
				instance->m_RayInjectionMaterial[0]->Set("u_History", instance->m_lightInjectionImage[1]);
				instance->m_RayInjectionMaterial[0]->Set("u_ShadowMapTexture", instance->m_ShadowPassPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());


				instance->m_RayInjectionMaterial[1]->Set("o_VoxelGrid", instance->m_lightInjectionImage[1]);
				instance->m_RayInjectionMaterial[1]->Set("u_History", instance->m_lightInjectionImage[0]);
				instance->m_RayInjectionMaterial[1]->Set("u_ShadowMapTexture", instance->m_ShadowPassPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());


				instance->m_ScatteringMaterial[0]->Set("i_VoxelGrid", instance->m_lightInjectionImage[0]);
				instance->m_ScatteringMaterial[0]->Set("o_VoxelGrid", instance->m_ScatteringImage);

				instance->m_ScatteringMaterial[1]->Set("i_VoxelGrid", instance->m_lightInjectionImage[1]);
				instance->m_ScatteringMaterial[1]->Set("o_VoxelGrid", instance->m_ScatteringImage);
			});
	}

	void SceneRenderer::Shutdown()
	{
		/*hdelete[] m_BoneTransformsData;*/
		for (auto& transformBuffer : m_SubmeshTransformBuffers)
			hdelete[] transformBuffer.Data;
	}

	void SceneRenderer::InitOptions()
	{
		//TODO(Karim): Deserialization?
		if (m_Options.EnableGTAO)
			*(int*)&m_Options.ReflectionOcclusionMethod |= (int)ShaderDef::AOMethod::GTAO;
		if (m_Options.EnableHBAO)
			*(int*)&m_Options.ReflectionOcclusionMethod |= (int)ShaderDef::AOMethod::HBAO;

		// Special macros are strictly starting with "__X2_"
		Renderer::SetGlobalMacroInShaders("__X2_REFLECTION_OCCLUSION_METHOD", fmt::format("{}", (int)m_Options.ReflectionOcclusionMethod));
		Renderer::SetGlobalMacroInShaders("__X2_AO_METHOD", fmt::format("{}", ShaderDef::GetAOMethod(m_Options.EnableGTAO, m_Options.EnableHBAO)));
		Renderer::SetGlobalMacroInShaders("__X2_GTAO_COMPUTE_BENT_NORMALS", fmt::format("{}", (int)m_Options.GTAOBentNormals));
	}

	void SceneRenderer::InsertGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor)
	{
		Renderer::Submit([=]
			{
				Renderer::RT_InsertGPUPerfMarker(renderCommandBuffer, label, markerColor);
			});
	}

	void SceneRenderer::BeginGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor)
	{
		Renderer::Submit([=]
			{
				Renderer::RT_BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
			});
	}

	void SceneRenderer::EndGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([=]
			{
				Renderer::RT_EndGPUPerfMarker(renderCommandBuffer);
			});
	}

	void SceneRenderer::SetScene(Ref<Scene> scene)
	{
		X2_CORE_ASSERT(!m_Active, "Can't change scenes while rendering");
		m_Scene = scene;
	}

	void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		width = (uint32_t)(width * m_Specification.Tiering.RendererScale);
		height = (uint32_t)(height * m_Specification.Tiering.RendererScale);

		if (m_ViewportWidth != width || m_ViewportHeight != height)
		{
			m_ViewportWidth = width;
			m_ViewportHeight = height;
			m_InvViewportWidth = 1.f / (float)width;
			m_InvViewportHeight = 1.f / (float)height;
			m_NeedsResize = true;
		}
	}

	void SceneRenderer::UpdateHBAOData()
	{
		const auto& opts = m_Options;
		UBHBAOData& hbaoData = HBAODataUB;

		// radius
		const float meters2viewSpace = 1.0f;
		const float R = opts.HBAORadius * meters2viewSpace;
		const float R2 = R * R;
		hbaoData.NegInvR2 = -1.0f / R2;
		hbaoData.InvQuarterResolution = 1.f / glm::vec2{ (float)m_ViewportWidth / 4, (float)m_ViewportHeight / 4 };
		hbaoData.RadiusToScreen = R * 0.5f * (float)m_ViewportHeight / (tanf(m_SceneData.SceneCamera.FOV * 0.5f) * 2.0f);

		const float* P = glm::value_ptr(m_SceneData.SceneCamera.Camera.GetProjectionMatrix());
		const glm::vec4 projInfoPerspective = {
				2.0f / (P[4 * 0 + 0]),                  // (x) * (R - L)/N
				2.0f / (P[4 * 1 + 1]),                  // (y) * (T - B)/N
				-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0],  // L/N
				-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1],  // B/N
		};
		hbaoData.PerspectiveInfo = projInfoPerspective;
		hbaoData.IsOrtho = false; //TODO: change depending on camera
		hbaoData.PowExponent = glm::max(opts.HBAOIntensity, 0.f);
		hbaoData.NDotVBias = glm::min(std::max(0.f, opts.HBAOBias), 1.f);
		hbaoData.AOMultiplier = 1.f / (1.f - hbaoData.NDotVBias);
		hbaoData.ShadowTolerance = m_Options.AOShadowTolerance;
	}

	// Some other settings are directly set in gui
	void SceneRenderer::UpdateGTAOData()
	{
		CBGTAOData& gtaoData = GTAODataCB;
		gtaoData.NDCToViewMul_x_PixelSize = { CameraDataUB.NDCToViewMul * (gtaoData.HalfRes ? ScreenDataUB.InvHalfResolution : ScreenDataUB.InvFullResolution) };
		gtaoData.HZBUVFactor = m_SSROptions.HZBUvFactor;
		gtaoData.ShadowTolerance = m_Options.AOShadowTolerance;
	}

	void SceneRenderer::BeginScene(const SceneRendererCamera& camera)
	{
		X2_PROFILE_FUNC();

		X2_CORE_ASSERT(m_Scene);
		X2_CORE_ASSERT(!m_Active);
		m_Active = true;

		const bool updatedAnyShaders = Renderer::UpdateDirtyShaders();
		if (updatedAnyShaders)
			InitMaterials();

		if (m_ResourcesCreatedGPU)
			m_ResourcesCreated = true;

		if (!m_ResourcesCreated)
			return;

		m_GTAOFinalImage = m_Options.GTAODenoisePasses && m_Options.GTAODenoisePasses % 2 != 0 ? m_GTAODenoiseImage : m_GTAOOutputImage;


		static bool isFlip = false;
		if (!isFlip)
		{
			m_CurTransformMap = &m_MeshTransformMap[0];
			m_PrevTransformMap = &m_MeshTransformMap[1];
		}
		else
		{
			m_CurTransformMap = &m_MeshTransformMap[1];
			m_PrevTransformMap = &m_MeshTransformMap[0];
		}
		isFlip = !isFlip;
		m_CurTransformMap->clear();

		
		m_TAAJitterCounter++;
		if (m_TAAJitterCounter >= 8)
			m_TAAJitterCounter = 0;


		m_SceneData.SceneCamera = camera;
		m_SceneData.SceneEnvironment = m_Scene->m_Environment;
		m_SceneData.SceneEnvironmentIntensity = m_Scene->m_EnvironmentIntensity;
		m_SceneData.ActiveLight = m_Scene->m_Light;
		m_SceneData.SceneLightEnvironment = m_Scene->m_LightEnvironment;
		m_SceneData.SkyboxLod = m_Scene->m_SkyboxLod;

		if (m_NeedsResize)
		{
			m_NeedsResize = false;

			const glm::uvec2 viewportSize = { m_ViewportWidth, m_ViewportHeight };

			ScreenDataUB.FullResolution = { m_ViewportWidth, m_ViewportHeight };
			ScreenDataUB.InvFullResolution = { m_InvViewportWidth,  m_InvViewportHeight };
			ScreenDataUB.HalfResolution = glm::ivec2{ m_ViewportWidth,  m_ViewportHeight } / 2;
			ScreenDataUB.InvHalfResolution = { m_InvViewportWidth * 2.0f,  m_InvViewportHeight * 2.0f };

			// Both Pre-depth and geometry framebuffers need to be resized first.
			// Note the _Anim variants of these pipelines share the same framebuffer
			m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_PreDepthTransparentPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);

			m_TAAVelocityImage->Resize(m_ViewportWidth, m_ViewportHeight);
			m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_SelectedGeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);

			//Dependent on Geometry 
			m_SSRCompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);

			m_SMAAEdgeDetectionPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_SMAABlendWeightPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_SMAANeighborBlendPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);

			//TAA Resize Callbacks
			m_TAAPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_TAAToneMappingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_TAAToneUnMappingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			//m_TAACurColorImage->Resize(m_ViewportWidth, m_ViewportHeight);
			m_TAAToneMappedPreColorImage->Resize(m_ViewportWidth, m_ViewportHeight);


			m_VisibilityTexture->Resize(m_ViewportWidth, m_ViewportHeight);
			m_ReinterleavingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_HBAOBlurPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_HBAOBlurPipelines[1]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_AOCompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight); //Only resize after geometry framebuffer
			m_CompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);


			if (m_DOFPipeline)
				m_DOFPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);

			if (m_ReadBackImage)
				m_ReadBackImage->Resize({ m_ViewportWidth, m_ViewportHeight });

			// HZB
			{
				//HZB size must be power of 2's
				const glm::uvec2 numMips = glm::ceil(glm::log2(glm::vec2(viewportSize)));
				m_SSROptions.NumDepthMips = glm::max(numMips.x, numMips.y);

				const glm::uvec2 hzbSize = BIT(numMips);
				m_HierarchicalDepthTexture->Resize(hzbSize.x, hzbSize.y);

				const glm::vec2 hzbUVFactor = { (glm::vec2)viewportSize / (glm::vec2)hzbSize };
				m_SSROptions.HZBUvFactor = hzbUVFactor;
			}

			// Light culling
			{
				constexpr uint32_t TILE_SIZE = 16u;
				glm::uvec2 size = viewportSize;
				size += TILE_SIZE - viewportSize % TILE_SIZE;
				m_LightCullingWorkGroups = { size / TILE_SIZE, 1 };
				RendererDataUB.TilesCountX = m_LightCullingWorkGroups.x;

				m_GTAODebugOutputImage->Resize(viewportSize);


				m_StorageBufferSet->Resize(14, 0, m_LightCullingWorkGroups.x * m_LightCullingWorkGroups.y * 4 * 1024);
				m_StorageBufferSet->Resize(23, 0, m_LightCullingWorkGroups.x * m_LightCullingWorkGroups.y * 4 * 1024);
			}

			// GTAO
			{
				glm::uvec2 gtaoSize = GTAODataCB.HalfRes ? (viewportSize + 1u) / 2u : viewportSize;
				glm::uvec2 denoiseSize = gtaoSize;
				const ImageFormat gtaoImageFormat = m_Options.GTAOBentNormals ? ImageFormat::RED32UI : ImageFormat::RED8UI;
				m_GTAOOutputImage->GetSpecification().Format = gtaoImageFormat;
				m_GTAODenoiseImage->GetSpecification().Format = gtaoImageFormat;

				m_GTAOOutputImage->Resize(gtaoSize);
				m_GTAODenoiseImage->Resize(gtaoSize);
				m_GTAOEdgesOutputImage->Resize(gtaoSize);


				constexpr uint32_t WORK_GROUP_SIZE = 16u;
				gtaoSize += WORK_GROUP_SIZE - gtaoSize % WORK_GROUP_SIZE;
				m_GTAOWorkGroups.x = gtaoSize.x / WORK_GROUP_SIZE;
				m_GTAOWorkGroups.y = gtaoSize.y / WORK_GROUP_SIZE;
				//m_GTAOWorkGroups.z = WORK_GROUP_SIZE;

				constexpr uint32_t DENOISE_WORK_GROUP_SIZE = 8u;
				denoiseSize += DENOISE_WORK_GROUP_SIZE - denoiseSize % DENOISE_WORK_GROUP_SIZE;
				m_GTAODenoiseWorkGroups.x = (denoiseSize.x + 2u * DENOISE_WORK_GROUP_SIZE - 1u) / (DENOISE_WORK_GROUP_SIZE * 2u); // 2 horizontal pixels at a time.
				m_GTAODenoiseWorkGroups.y = denoiseSize.y / DENOISE_WORK_GROUP_SIZE;
			}

			// Quarter-size hbao textures
			{
				glm::uvec2 quarterSize = (viewportSize + 3u) / 4u;
				m_DeinterleavingPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(quarterSize.x, quarterSize.y);
				m_DeinterleavingPipelines[1]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->Resize(quarterSize.x, quarterSize.y);
				m_HBAOOutputImage->Resize(quarterSize);
				m_HBAOOutputImage->CreatePerLayerImageViews();


				constexpr uint32_t WORK_GROUP_SIZE = 16u;
				quarterSize += WORK_GROUP_SIZE - quarterSize % WORK_GROUP_SIZE;
				m_HBAOWorkGroups.x = quarterSize.x / 16u;
				m_HBAOWorkGroups.y = quarterSize.y / 16u;
				m_HBAOWorkGroups.z = 16;
			}

			// Ray Marching 
			{
				//1.Light Injection
				uint32_t LOCAL_SIZE_X = 16u;
				uint32_t LOCAL_SIZE_Y = 2u;
				uint32_t LOCAL_SIZE_Z = 16u;

				m_RayInjectionWorkGroups.x = static_cast<uint32_t>(ceil(float(m_Options.VOXEL_GRID_SIZE_X) / float(LOCAL_SIZE_X)));
				m_RayInjectionWorkGroups.y = static_cast<uint32_t>(ceil(float(m_Options.VOXEL_GRID_SIZE_Y) / float(LOCAL_SIZE_Y)));
				m_RayInjectionWorkGroups.z = static_cast<uint32_t>(ceil(float(m_Options.VOXEL_GRID_SIZE_Z) / float(LOCAL_SIZE_Z)));

				//2.Scattering
				LOCAL_SIZE_X = 64u;
				LOCAL_SIZE_Y = 2u;

				m_ScatteringWorkGroups.x = static_cast<uint32_t>(ceil(float(m_Options.VOXEL_GRID_SIZE_X) / float(LOCAL_SIZE_X)));
				m_ScatteringWorkGroups.y = static_cast<uint32_t>(ceil(float(m_Options.VOXEL_GRID_SIZE_Y) / float(LOCAL_SIZE_Y)));
				m_ScatteringWorkGroups.z = 1;
			}

			//SSR
			{
				constexpr uint32_t WORK_GROUP_SIZE = 8u;
				glm::uvec2 ssrSize = m_SSROptions.HalfRes ? (viewportSize + 1u) / 2u : viewportSize;
				m_SSRImage->Resize(ssrSize);
				m_PreConvolutedTexture->Resize(ssrSize.x, ssrSize.y);
				ssrSize += WORK_GROUP_SIZE - ssrSize % WORK_GROUP_SIZE;
				m_SSRWorkGroups.x = ssrSize.x / WORK_GROUP_SIZE;
				m_SSRWorkGroups.y = ssrSize.y / WORK_GROUP_SIZE;
			}

			//Bloom
			{
				glm::uvec2 bloomSize = (viewportSize + 1u) / 2u;
				bloomSize += m_BloomComputeWorkgroupSize - bloomSize % m_BloomComputeWorkgroupSize;
				m_BloomComputeTextures[0]->Resize(bloomSize);
				m_BloomComputeTextures[1]->Resize(bloomSize);
				m_BloomComputeTextures[2]->Resize(bloomSize);
			}

			for (auto& tempFB : m_TempFramebuffers)
				tempFB->Resize(m_ViewportWidth, m_ViewportHeight);

			if (m_ExternalCompositeRenderPass)
				m_ExternalCompositeRenderPass->GetSpecification().TargetFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
		}

		// Update uniform buffers
		UBCamera& cameraData = CameraDataUB;
		UBScene& sceneData = SceneDataUB;
		UBShadow& shadowData = ShadowData;
		UBRendererData& rendererData = RendererDataUB;
		UBPointLights& pointLightData = PointLightsUB;
		UBHBAOData& hbaoData = HBAODataUB;
		UBScreenData& screenData = ScreenDataUB;
		UBSpotLights& spotLightData = SpotLightUB;
		UBSpotShadowData& spotShadowData = SpotShadowDataUB;
		UBTAAData& taaData = TAADataUB;
		UBRayMarchingData& rayMarchingData = RayMarchingDataUB;
		Ref<SceneRenderer> instance = this;

		
		//TAA History
		taaData.ViewProjectionHistory = cameraData.ViewProjection;


		auto& sceneCamera = m_SceneData.SceneCamera;
		const auto viewProjection = sceneCamera.Camera.GetProjectionMatrix() * sceneCamera.ViewMatrix;

		//auto view = 
		const glm::mat4 viewInverse = glm::inverse(sceneCamera.ViewMatrix);
		const glm::mat4 projectionInverse = glm::inverse(sceneCamera.Camera.GetProjectionMatrix());
		const glm::vec3 cameraPosition = viewInverse[3];

		cameraData.ViewProjection = viewProjection;
		cameraData.Projection = sceneCamera.Camera.GetProjectionMatrix();
		cameraData.InverseProjection = projectionInverse;
		cameraData.View = sceneCamera.ViewMatrix;
		cameraData.InverseView = viewInverse;
		cameraData.InverseViewProjection = viewInverse * cameraData.InverseProjection;

		//float depthLinearizeMul = (-cameraData.Projection[3][2]);     // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
		//float depthLinearizeAdd = (cameraData.Projection[2][2]);     // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
		
		float depthLinearizeMul = -(sceneCamera.Far * sceneCamera.Near) / (sceneCamera.Far - sceneCamera.Near);     // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
		float depthLinearizeAdd =  sceneCamera.Far / (sceneCamera.Far - sceneCamera.Near);  // float depthLinearizeAdd = clipFar / ( clipFar - clipNear ); 
		
		//// correct the handedness issue.
		//if (depthLinearizeMul * depthLinearizeAdd < 0)
		//	depthLinearizeAdd = -depthLinearizeAdd;
		cameraData.DepthUnpackConsts = { depthLinearizeMul, depthLinearizeAdd };
		const float* P = glm::value_ptr(m_SceneData.SceneCamera.Camera.GetProjectionMatrix());
		const glm::vec4 projInfoPerspective = {
				 2.0f / (P[4 * 0 + 0]),                  // (x) * (R - L)/N
				 2.0f / (P[4 * 1 + 1]),                  // (y) * (T - B)/N
				-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0],  // L/N
				-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1],  // B/N
		};
		float tanHalfFOVY = 1.0f / cameraData.Projection[1][1];    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
		float tanHalfFOVX = 1.0f / cameraData.Projection[0][0];    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
		cameraData.CameraTanHalfFOV = { tanHalfFOVX, tanHalfFOVY };
		cameraData.NDCToViewMul = { projInfoPerspective[0], projInfoPerspective[1] };
		cameraData.NDCToViewAdd = { projInfoPerspective[2], projInfoPerspective[3] };

		
		Renderer::Submit([instance, cameraData]() mutable
			{
				uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::CameraData, 0, bufferIndex)->RT_SetData(&cameraData, sizeof(cameraData));
			});

		const auto lightEnvironment = m_SceneData.SceneLightEnvironment;
		const std::vector<PointLight>& pointLightsVec = lightEnvironment.PointLights;
		pointLightData.Count = int(pointLightsVec.size());
		std::memcpy(pointLightData.PointLights, pointLightsVec.data(), lightEnvironment.GetPointLightsSize()); //(Karim) Do we really have to copy that?
		Renderer::Submit([instance, &pointLightData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				Ref<VulkanUniformBuffer> bufferSet = instance->m_UniformBufferSet->Get(Binding::PointLightData, 0, bufferIndex);
				bufferSet->RT_SetData(&pointLightData, 16ull + sizeof PointLight * pointLightData.Count);
			});

		const std::vector<SpotLight>& spotLightsVec = lightEnvironment.SpotLights;
		spotLightData.Count = int(spotLightsVec.size());
		std::memcpy(spotLightData.SpotLights, spotLightsVec.data(), lightEnvironment.GetSpotLightsSize()); //(Karim) Do we really have to copy that?
		Renderer::Submit([instance, &spotLightData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				Ref<VulkanUniformBuffer> bufferSet = instance->m_UniformBufferSet->Get(19, 0, bufferIndex);
				bufferSet->RT_SetData(&spotLightData, 16ull + sizeof SpotLight * spotLightData.Count);
			});

		for (size_t i = 0; i < spotLightsVec.size(); ++i)
		{
			auto& light = spotLightsVec[i];
			if (!light.CastsShadows)
				continue;

			glm::mat4 projection = glm::perspective(glm::radians(light.Angle), 1.f, 0.1f, light.Range);
			// NOTE(Yan): ShadowMatrices[0] because we only support ONE shadow casting spot light at the moment and it MUST be index 0
			spotShadowData.ShadowMatrices[0] = projection * glm::lookAt(light.Position, light.Position - light.Direction, glm::vec3(0.0f, 1.0f, 0.0f));
		}

		Renderer::Submit([instance, spotShadowData, spotLightsVec]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				Ref<VulkanUniformBuffer> bufferSet = instance->m_UniformBufferSet->Get(20, 0, bufferIndex);
				bufferSet->RT_SetData(&spotShadowData, (uint32_t)(sizeof(glm::mat4) * spotLightsVec.size()));
			});

		const auto& directionalLight = m_SceneData.SceneLightEnvironment.DirectionalLights[0];
		sceneData.Lights.Direction = directionalLight.Direction;
		sceneData.Lights.Radiance = directionalLight.Radiance;
		sceneData.Lights.Intensity = directionalLight.Intensity;
		sceneData.Lights.ShadowAmount = directionalLight.ShadowAmount;

		sceneData.CameraPosition = cameraPosition;
		sceneData.EnvironmentMapIntensity = m_SceneData.SceneEnvironmentIntensity;
		Renderer::Submit([instance, sceneData]() mutable
			{
				uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::SceneData, 0, bufferIndex)->RT_SetData(&sceneData, sizeof(sceneData));
			});

		if (m_Options.EnableHBAO)
			UpdateHBAOData();
		if (m_Options.EnableGTAO)
			UpdateGTAOData();

		Renderer::Submit([instance, hbaoData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::HBAOData, 0, bufferIndex)->RT_SetData(&hbaoData, sizeof(hbaoData));
			});

		Renderer::Submit([instance, screenData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::ScreenData, 0, bufferIndex)->RT_SetData(&screenData, sizeof(screenData));
			});

		CascadeData cascades[4];
		if (m_UseManualCascadeSplits)
			CalculateCascadesManualSplit(cascades, sceneCamera, directionalLight.Direction);
		else
			CalculateCascades(cascades, sceneCamera, directionalLight.Direction);


		// TODO: four cascades for now
		for (int i = 0; i < 4; i++)
		{
			CascadeSplits[i] = cascades[i].SplitDepth;
			shadowData.ViewProjection[i] = cascades[i].ViewProj;
		}
		Renderer::Submit([instance, shadowData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::ShadowData, 0, bufferIndex)->RT_SetData(&shadowData, sizeof(shadowData));
			});

		rendererData.CascadeSplits = CascadeSplits;
		Renderer::Submit([instance, rendererData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::RendererData, 0, bufferIndex)->RT_SetData(&rendererData, sizeof(rendererData));
			});

		Renderer::SetSceneEnvironment(this, m_SceneData.SceneEnvironment,
			m_ShadowPassPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage(),
			m_SpotShadowPassPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage(),
			m_ScatteringImage);


		//TAA
		taaData.ScreenSpaceJitter = (m_TAAHaltonSequence[m_TAAJitterCounter] - 0.5f) * glm::vec2(m_InvViewportWidth, m_InvViewportHeight);

		glm::mat4 jitterMatrix(sceneCamera.Camera.GetProjectionMatrix());
		jitterMatrix[2][0] += taaData.ScreenSpaceJitter.x * 2;
		jitterMatrix[2][1] += taaData.ScreenSpaceJitter.y * 2;

		taaData.InvJitterdProjection = glm::inverse(jitterMatrix);

		taaData.JitterdViewProjection = jitterMatrix * sceneCamera.ViewMatrix;
		taaData.FeedBack = m_Options.TAAFeedback;

		Renderer::Submit([instance, taaData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::TAAData, 0, bufferIndex)->RT_SetData(&taaData, sizeof(taaData));
			});


		//RayMarching
		rayMarchingData.bias_near_far_pow = glm::vec4(0.002f , m_SceneData.SceneCamera.Near, m_SceneData.SceneCamera.Far, 1.0f);
		rayMarchingData.aniso_density_scattering_absorption = glm::vec4(m_Options.rayMarchingAnisotropy, m_Options.rayMarchingDensity, m_Options.VolumeLightMul, 0.0f);

		glm::vec2 frustumUVs[4] = {{0,0}, {1,0}, {0,1}, {1,1}};
		for (uint32_t i = 0; i < 4; ++i)
		{
			float viewZ = m_SceneData.SceneCamera.Near - m_SceneData.SceneCamera.Far;
			glm::vec3 ViewCornerPos = glm::vec3((cameraData.NDCToViewMul* frustumUVs[i] + cameraData.NDCToViewAdd) * viewZ, viewZ);
			
			glm::vec4 WorldCornerPos = cameraData.InverseView * glm::vec4(ViewCornerPos, 1.0);
			WorldCornerPos /= WorldCornerPos.w;
			
			rayMarchingData.frustumRays[i] = WorldCornerPos - glm::vec4(cameraPosition, 1.0);
		}

		const auto fogVolumes = m_Scene->m_FogVolumes;
		rayMarchingData.FogVolumeCount = fogVolumes.size();
		std::memcpy(rayMarchingData.boxFogVolumes, fogVolumes.data(), (uint32_t)(fogVolumes.size() * sizeof FogVolume)); //(Karim) Do we really have to copy that?

		Renderer::Submit([instance, rayMarchingData]() mutable
			{
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
				instance->m_UniformBufferSet->Get(Binding::RayMarchingData, 0, bufferIndex)->RT_SetData(&rayMarchingData, sizeof(rayMarchingData));
			});

	}

	void SceneRenderer::EndScene()
	{
		X2_PROFILE_FUNC();

		X2_CORE_ASSERT(m_Active);
#if MULTI_THREAD
		Ref<SceneRenderer> instance = this;
		s_ThreadPool.emplace_back(([instance]() mutable
			{
				instance->FlushDrawList();
			}));
#else 
		FlushDrawList();
#endif

		m_Active = false;
	}

	void SceneRenderer::WaitForThreads()
	{
		for (uint32_t i = 0; i < s_ThreadPool.size(); i++)
			s_ThreadPool[i].join();

		s_ThreadPool.clear();
	}

	void SceneRenderer::SubmitMesh(uint64_t entityUUID, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform, const std::vector<glm::mat4>& boneTransforms, Ref<VulkanMaterial> overrideMaterial)
	{
		X2_PROFILE_FUNC();

		// TODO: Culling, sorting, etc.

		const auto meshSource = mesh->GetMeshSource();
		const auto& submeshes = meshSource->GetSubmeshes();
		const auto& submesh = submeshes[submeshIndex];
		uint32_t materialIndex = submesh.MaterialIndex;
		bool isRigged = submesh.IsRigged;

		AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : mesh->GetMaterials()->GetMaterial(materialIndex);
		Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

		MeshKey meshKey = { entityUUID, mesh->Handle, materialHandle, submeshIndex, false };
		auto& transformStorage = (* m_CurTransformMap)[meshKey].Transforms.emplace_back();


		transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };


		if ((*m_PrevTransformMap).find(meshKey) == (*m_PrevTransformMap).end())
		{
			(*m_PrevTransformMap)[meshKey] = (*m_CurTransformMap)[meshKey];
		}
	

		/*if (isRigged)
		{
			CopyToBoneTransformStorage(meshKey, meshSource, boneTransforms);
		}*/
		// Main geo
		{
			bool isTransparent = material->IsTransparent();
			auto& destDrawList = !isTransparent ? m_DrawList : m_TransparentDrawList;
			auto& dc = destDrawList[meshKey];
			dc.Mesh = mesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
			dc.IsRigged = isRigged;  // TODO: would it be better to have separate draw list for rigged meshes, or this flag is OK?
		}

		// Shadow pass
		if (material->IsShadowCasting())
		{
			auto& dc = m_ShadowPassDrawList[meshKey];
			dc.Mesh = mesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
			dc.IsRigged = isRigged;
		}
	}

	void SceneRenderer::SubmitStaticMesh(uint64_t entityUUID, Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<VulkanMaterial> overrideMaterial)
	{
		X2_PROFILE_FUNC();

		Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
		const auto& submeshData = meshSource->GetSubmeshes();
		for (uint32_t submeshIndex : staticMesh->GetSubmeshes())
		{
			glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

			const auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
			uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;

			AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : staticMesh->GetMaterials()->GetMaterial(materialIndex);
			X2_CORE_VERIFY(materialHandle);
			Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

			MeshKey meshKey = { entityUUID, staticMesh->Handle, materialHandle, submeshIndex, false };
			auto& transformStorage = (* m_CurTransformMap)[meshKey].Transforms.emplace_back();

			transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
			transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
			transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };
			

			if ((*m_PrevTransformMap).find(meshKey) == (*m_PrevTransformMap).end())
			{
				(*m_PrevTransformMap)[meshKey] = (*m_CurTransformMap)[meshKey];
			}

			// Main geo
			{
				bool isTransparent = material->IsTransparent();
				auto& destDrawList = !isTransparent ? m_StaticMeshDrawList : m_TransparentStaticMeshDrawList;
				auto& dc = destDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// Shadow pass
			if (material->IsShadowCasting())
			{
				auto& dc = m_StaticMeshShadowPassDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}
		}

	}

	void SceneRenderer::SubmitSelectedMesh(uint64_t entityUUID, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform, const std::vector<glm::mat4>& boneTransforms, Ref<VulkanMaterial> overrideMaterial)
	{
		X2_PROFILE_FUNC();

		// TODO: Culling, sorting, etc.

		const auto meshSource = mesh->GetMeshSource();
		const auto& submeshes = meshSource->GetSubmeshes();
		const auto& submesh = submeshes[submeshIndex];
		uint32_t materialIndex = submesh.MaterialIndex;
		bool isRigged = submesh.IsRigged;

		AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : mesh->GetMaterials()->GetMaterial(materialIndex);
		X2_CORE_VERIFY(materialHandle);
		Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

		MeshKey meshKey = { entityUUID, mesh->Handle, materialHandle, submeshIndex, true };
		auto& transformStorage = (* m_CurTransformMap)[meshKey].Transforms.emplace_back();

		transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

		if ((*m_PrevTransformMap).find(meshKey) == (*m_PrevTransformMap).end())
		{
			(*m_PrevTransformMap)[meshKey] = (*m_CurTransformMap)[meshKey];
		}

		/*if (isRigged)
		{
			CopyToBoneTransformStorage(meshKey, meshSource, boneTransforms);
		}*/

		uint32_t instanceIndex = 0;

		// Main geo
		{
			bool isTransparent = material->IsTransparent();
			auto& destDrawList = !isTransparent ? m_DrawList : m_TransparentDrawList;
			auto& dc = destDrawList[meshKey];
			dc.Mesh = mesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;

			instanceIndex = dc.InstanceCount;
			dc.InstanceCount++;
			dc.IsRigged = isRigged;
		}

		// Selected mesh list
		{
			auto& dc = m_SelectedMeshDrawList[meshKey];
			dc.Mesh = mesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
			dc.InstanceOffset = instanceIndex;
			dc.IsRigged = isRigged;
		}

		// Shadow pass
		if (material->IsShadowCasting())
		{
			auto& dc = m_ShadowPassDrawList[meshKey];
			dc.Mesh = mesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
			dc.IsRigged = isRigged;
		}
	}

	void SceneRenderer::SubmitSelectedStaticMesh(uint64_t entityUUID, Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<VulkanMaterial> overrideMaterial)
	{
		X2_PROFILE_FUNC();

		Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
		const auto& submeshData = meshSource->GetSubmeshes();
		for (uint32_t submeshIndex : staticMesh->GetSubmeshes())
		{
			glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

			const auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
			uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;

			AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : staticMesh->GetMaterials()->GetMaterial(materialIndex);
			X2_CORE_VERIFY(materialHandle);
			Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

			MeshKey meshKey = { entityUUID, staticMesh->Handle, materialHandle, submeshIndex, true };
			auto& transformStorage = (* m_CurTransformMap)[meshKey].Transforms.emplace_back();

			transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
			transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
			transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };

			if ((*m_PrevTransformMap).find(meshKey) == (*m_PrevTransformMap).end())
			{
				(*m_PrevTransformMap)[meshKey] = (*m_CurTransformMap)[meshKey];
			}

			// Main geo
			{
				bool isTransparent = material->IsTransparent();
				auto& destDrawList = !isTransparent ? m_StaticMeshDrawList : m_TransparentStaticMeshDrawList;
				auto& dc = destDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// Selected mesh list
			{
				auto& dc = m_SelectedStaticMeshDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// Shadow pass
			if (material->IsShadowCasting())
			{
				auto& dc = m_StaticMeshShadowPassDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}
		}
	}

	void SceneRenderer::SubmitPhysicsDebugMesh(uint64_t entityUUID, Ref<Mesh> mesh, uint32_t submeshIndex, const glm::mat4& transform)
	{
		X2_CORE_VERIFY(mesh->Handle);

		Ref<MeshSource> meshSource = mesh->GetMeshSource();
		const auto& submeshData = meshSource->GetSubmeshes();
		glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

		MeshKey meshKey = { entityUUID, mesh->Handle, 5, submeshIndex, false };
		auto& transformStorage = (* m_CurTransformMap)[meshKey].Transforms.emplace_back();


		transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

		if ((*m_PrevTransformMap).find(meshKey) == (*m_PrevTransformMap).end())
		{
			(*m_PrevTransformMap)[meshKey] = (*m_CurTransformMap)[meshKey];
		}

		{
			auto& dc = m_ColliderDrawList[meshKey];
			dc.Mesh = mesh;
			dc.SubmeshIndex = submeshIndex;
			dc.InstanceCount++;
		}
	}

	void SceneRenderer::SubmitPhysicsStaticDebugMesh(uint64_t entityUUID, Ref<StaticMesh> staticMesh, const glm::mat4& transform, const bool isPrimitiveCollider)
	{
		X2_CORE_VERIFY(staticMesh->Handle);
		Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
		const auto& submeshData = meshSource->GetSubmeshes();
		for (uint32_t submeshIndex : staticMesh->GetSubmeshes())
		{
			glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

			MeshKey meshKey = {entityUUID, staticMesh->Handle, 5, submeshIndex, false };
			auto& transformStorage = (* m_CurTransformMap)[meshKey].Transforms.emplace_back();

			transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
			transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
			transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };
			
			if ((*m_PrevTransformMap).find(meshKey) == (*m_PrevTransformMap).end())
			{
				(*m_PrevTransformMap)[meshKey] = (*m_CurTransformMap)[meshKey];
			}

			{
				auto& dc = m_StaticColliderDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.OverrideMaterial = isPrimitiveCollider ? m_SimpleColliderMaterial : m_ComplexColliderMaterial;
				dc.InstanceCount++;
			}

		}
	}

	void SceneRenderer::ClearPass(Ref<VulkanRenderPass> renderPass, bool explicitClear)
	{
		X2_PROFILE_FUNC();
		Renderer::BeginRenderPass(m_CommandBuffer, renderPass, explicitClear);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::ShadowMapPass()
	{
		X2_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_GPUTimeQueries.DirShadowMapPassQuery = m_CommandBuffer->BeginTimestampQuery();

		auto& directionalLights = m_SceneData.SceneLightEnvironment.DirectionalLights;
		if (directionalLights[0].Intensity == 0.0f || !directionalLights[0].CastShadows)
		{
			// Clear shadow maps
			for (int i = 0; i < 4; i++)
				ClearPass(m_ShadowPassPipelines[i]->GetSpecification().RenderPass);

			return;
		}

		// TODO: change to four cascades (or set number)
		for (int i = 0; i < 4; i++)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_ShadowPassPipelines[i]->GetSpecification().RenderPass);

			// static glm::mat4 scaleBiasMatrix = glm::scale(glm::mat4(1.0f), { 0.5f, 0.5f, 0.5f }) * glm::translate(glm::mat4(1.0f), { 1, 1, 1 });

			// Render entities
			const Buffer cascade(&i, sizeof(uint32_t));
			for (auto& [mk, dc] : m_StaticMeshShadowPassDrawList)
			{
				X2_CORE_VERIFY(m_CurTransformMap->find(mk) != m_CurTransformMap->end());
				const auto& transformData = m_CurTransformMap->at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_ShadowPassPipelines[i], m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_ShadowPassMaterial, cascade);
			}
			for (auto& [mk, dc] : m_ShadowPassDrawList)
			{
				X2_CORE_VERIFY(m_CurTransformMap->find(mk) != m_CurTransformMap->end());
				const auto& transformData = m_CurTransformMap->at(mk);
				/*if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_ShadowPassPipelinesAnim[i], m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex, dc.InstanceCount, m_ShadowPassMaterial, cascade);
				}
				else
				{*/
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_ShadowPassPipelines[i], m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_ShadowPassMaterial, cascade);
				//}
			}

			Renderer::EndRenderPass(m_CommandBuffer);
		}

		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.DirShadowMapPassQuery);
	}

	void SceneRenderer::SpotShadowMapPass()
	{
		X2_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_GPUTimeQueries.SpotShadowMapPassQuery = m_CommandBuffer->BeginTimestampQuery();

		// Spot shadow maps
		Renderer::BeginRenderPass(m_CommandBuffer, m_SpotShadowPassPipeline->GetSpecification().RenderPass);
		for (uint32_t i = 0; i < 1; i++)
		{

			const Buffer lightIndex(&i, sizeof(uint32_t));
			for (auto& [mk, dc] : m_StaticMeshShadowPassDrawList)
			{
				X2_CORE_VERIFY(m_CurTransformMap->find(mk) != m_CurTransformMap->end());
				const auto& transformData = m_CurTransformMap->at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_SpotShadowPassPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_SpotShadowPassMaterial, lightIndex);
			}
			for (auto& [mk, dc] : m_ShadowPassDrawList)
			{
				X2_CORE_VERIFY(m_CurTransformMap->find(mk) != m_CurTransformMap->end());
				const auto& transformData = m_CurTransformMap->at(mk);
				/*if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_SpotShadowPassAnimPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex, dc.InstanceCount, m_SpotShadowPassMaterial, lightIndex);
				}
				else
				{*/
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_SpotShadowPassPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_SpotShadowPassMaterial, lightIndex);
				//}
			}

		}
		Renderer::EndRenderPass(m_CommandBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SpotShadowMapPassQuery);

	}

	void SceneRenderer::PreDepthPass()
	{
		X2_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_GPUTimeQueries.DepthPrePassQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_CommandBuffer, m_PreDepthPipeline->GetSpecification().RenderPass);
		for (auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			if(!IsUsingTAA(m_Options.AAMethod) || !m_Options.EnableAA)
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_PreDepthPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_PreDepthMaterial);
			else
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_PreDepthTAAPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_PreDepthTAAMaterial);
		}
		for (auto& [mk, dc] : m_DrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			/*if (dc.IsRigged)
			{
				const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
				Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_PreDepthPipelineAnim, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex, dc.InstanceCount, m_PreDepthMaterial);
			}
			else
			{*/
			if (!IsUsingTAA(m_Options.AAMethod) || !m_Options.EnableAA)
				Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_PreDepthPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_PreDepthMaterial);
			else
				Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_PreDepthTAAPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_PreDepthTAAMaterial);
			//}
		}

		Renderer::EndRenderPass(m_CommandBuffer);

		Renderer::BeginRenderPass(m_CommandBuffer, m_PreDepthTransparentPipeline->GetSpecification().RenderPass);
#if 1
		for (auto& [mk, dc] : m_TransparentStaticMeshDrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_PreDepthTransparentPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_PreDepthMaterial);
		}
		for (auto& [mk, dc] : m_TransparentDrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			//if (dc.IsRigged)
			//{
			//	const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);

			//	// TODO(0x): This needs to be pre-depth transparent-anim pipeline
			//	Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_PreDepthPipelineAnim, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex, dc.InstanceCount, m_PreDepthMaterial);
			//}
			//else
			//{
				Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_PreDepthTransparentPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_PreDepthMaterial);
			//}
		}
#endif

		Renderer::EndRenderPass(m_CommandBuffer);
		//SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

#if 0 // Is this necessary?
		Renderer::Submit([cb = m_CommandBuffer, image = m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage().As<VulkanImage2D>()]()
			{
				VkImageMemoryBarrier imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				imageMemoryBarrier.image = image->GetImageInfo().Image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, image->GetSpecification().Mips, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(
					cb.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::GetCurrentFrameIndex()),
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			});
#endif

		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.DepthPrePassQuery);
	}

	void SceneRenderer::HZBCompute()
	{
		X2_PROFILE_FUNC();

		Ref<VulkanComputePipeline> pipeline = m_HierarchicalDepthPipeline.As<VulkanComputePipeline>();

		auto srcDepthImage = m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage().As<VulkanImage2D>();

		m_GPUTimeQueries.HierarchicalDepthQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::Submit([srcDepthImage, commandBuffer = m_CommandBuffer, hierarchicalZTex = m_HierarchicalDepthTexture.As<VulkanTexture2D>(), material = m_HierarchicalDepthMaterial.As<VulkanMaterial>(), pipeline]() mutable
			{
				const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				Ref<VulkanImage2D> hierarchicalZImage = hierarchicalZTex->GetImage().As<VulkanImage2D>();

				auto shader = material->GetShader().As<VulkanShader>();

				VkDescriptorSetLayout descriptorSetLayout = shader->GetDescriptorSetLayout(0);

				VkDescriptorSetAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayout;

				constexpr uint32_t maxMipBatchSize = 4;
				const uint32_t hzbMipCount = hierarchicalZTex->GetMipLevelCount();

				pipeline->RT_Begin(commandBuffer);

				auto ReduceHZB = [hzbMipCount, maxMipBatchSize, &shader, &hierarchicalZImage, &device, &pipeline, &allocInfo]
				(const uint32_t startDestMip, const uint32_t parentMip, Ref<VulkanImage2D> parentImage, const glm::vec2& DispatchThreadIdToBufferUV, const glm::vec2& InputViewportMaxBound, const bool isFirstPass)
				{
					const VkDescriptorSet descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
					struct HierarchicalZComputePushConstants
					{
						glm::vec2 DispatchThreadIdToBufferUV;
						glm::vec2 InputViewportMaxBound;
						glm::vec2 InvSize;
						int FirstLod;
						bool IsFirstPass;
						char Padding[3]{ 0, 0, 0 };
					} hierarchicalZComputePushConstants;

					hierarchicalZComputePushConstants.IsFirstPass = isFirstPass;
					hierarchicalZComputePushConstants.FirstLod = (int)startDestMip;
					hierarchicalZComputePushConstants.DispatchThreadIdToBufferUV = DispatchThreadIdToBufferUV;
					hierarchicalZComputePushConstants.InputViewportMaxBound = InputViewportMaxBound;


					std::array<VkWriteDescriptorSet, 5> writeDescriptors{};
					std::array<VkDescriptorImageInfo, 5> hzbImageInfos{};
					const uint32_t endDestMip = glm::min(startDestMip + maxMipBatchSize, hzbMipCount);
					uint32_t destMip;
					for (destMip = startDestMip; destMip < endDestMip; ++destMip)
					{
						uint32_t idx = destMip - startDestMip;
						hzbImageInfos[idx] = hierarchicalZImage->GetDescriptorInfo();
						hzbImageInfos[idx].imageView = hierarchicalZImage->RT_GetMipImageView(destMip);
						hzbImageInfos[idx].sampler = VK_NULL_HANDLE;
					}

					// Fill the rest .. or we could enable the null descriptor feature
					destMip -= startDestMip;
					for (; destMip < maxMipBatchSize; ++destMip)
					{
						hzbImageInfos[destMip] = hierarchicalZImage->GetDescriptorInfo();
						hzbImageInfos[destMip].imageView = hierarchicalZImage->RT_GetMipImageView(hzbMipCount - 1);
						hzbImageInfos[destMip].sampler = VK_NULL_HANDLE;
					}

					for (uint32_t i = 0; i < maxMipBatchSize; ++i)
					{
						writeDescriptors[i] = *shader->GetDescriptorSet("o_HZB");
						writeDescriptors[i].dstSet = descriptorSet; // Should this be set inside the shader?
						writeDescriptors[i].dstArrayElement = i;
						writeDescriptors[i].pImageInfo = &hzbImageInfos[i];
					}

					hzbImageInfos[4] = parentImage->GetDescriptorInfo();
					hzbImageInfos[4].sampler = VulkanRenderer::GetPointSampler();

					writeDescriptors[4] = *shader->GetDescriptorSet("u_InputDepth");
					writeDescriptors[4].dstSet = descriptorSet; // Should this be set inside the shader?
					writeDescriptors[4].pImageInfo = &hzbImageInfos[maxMipBatchSize];

					vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

					const glm::ivec2 srcSize{ Math::DivideAndRoundUp(parentImage->GetSize(), 1u << parentMip) };
					const glm::ivec2 dstSize = Math::DivideAndRoundUp(hierarchicalZImage->GetSize(), 1u << startDestMip);
					hierarchicalZComputePushConstants.InvSize = glm::vec2{ 1.0f / (float)srcSize.x, 1.0f / (float)srcSize.y };

					pipeline->SetPushConstants(&hierarchicalZComputePushConstants, sizeof(hierarchicalZComputePushConstants));

					pipeline->Dispatch(descriptorSet, Math::DivideAndRoundUp(dstSize.x, 8), Math::DivideAndRoundUp(dstSize.y, 8), 1);
				};
				glm::ivec2 srcSize = srcDepthImage->GetSize();

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "HZB");

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "HZB-FirstPass");
				// Reduce first 4 mips
				ReduceHZB(0, 0, srcDepthImage, { 1.0f / glm::vec2{ srcSize } }, { (glm::vec2{ srcSize } - 0.5f) / glm::vec2{ srcSize } }, true);
				Renderer::RT_EndGPUPerfMarker(commandBuffer);
				// Reduce the next mips
				for (uint32_t startDestMip = maxMipBatchSize; startDestMip < hzbMipCount; startDestMip += maxMipBatchSize)
				{
					Renderer::RT_BeginGPUPerfMarker(commandBuffer, fmt::format("HZB-Pass({})", startDestMip));
					srcSize = Math::DivideAndRoundUp(hierarchicalZTex->GetSize(), 1u << uint32_t(startDestMip - 1));
					ReduceHZB(startDestMip, startDestMip - 1, hierarchicalZImage, { 2.0f / glm::vec2{ srcSize } }, glm::vec2{ 1.0f }, false);
					Renderer::RT_EndGPUPerfMarker(commandBuffer);
				}


				Renderer::RT_EndGPUPerfMarker(commandBuffer);
				pipeline->End();
			});
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.HierarchicalDepthQuery);
	}

	void SceneRenderer::PreIntegration()
	{
		X2_PROFILE_FUNC();

		Ref<VulkanComputePipeline> pipeline = m_PreIntegrationPipeline.As<VulkanComputePipeline>();

		m_GPUTimeQueries.PreIntegrationQuery = m_CommandBuffer->BeginTimestampQuery();
		glm::vec2 projectionParams = { m_SceneData.SceneCamera.Far, m_SceneData.SceneCamera.Near }; // Reversed 
		Renderer::Submit([projectionParams, hzbUVFactor = m_SSROptions.HZBUvFactor, depthImage = m_HierarchicalDepthTexture->GetImage().As<VulkanImage2D>(), commandBuffer = m_CommandBuffer.As<VulkanRenderCommandBuffer>(),
			visibilityTexture = m_VisibilityTexture, material = m_PreIntegrationMaterial.As<VulkanMaterial>(), pipeline]() mutable
			{
				const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				Ref<VulkanImage2D> visibilityImage = visibilityTexture->GetImage().As<VulkanImage2D>();

				struct PreIntegrationComputePushConstants
				{
					glm::vec2 HZBResFactor;
					glm::vec2 ResFactor;
					glm::vec2 ProjectionParams; //(x) = Near plane, (y) = Far plane
					int PrevLod = 0;
				} preIntegrationComputePushConstants;

				//Clear to 100% visibility .. TODO: this can be done once when the image is resized
				VkClearColorValue clearColor{ { 1.f } };
				VkImageSubresourceRange subresourceRange{};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.layerCount = 1;
				subresourceRange.levelCount = 1;
				vkCmdClearColorImage(commandBuffer->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex()), visibilityImage->GetImageInfo().Image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);

				std::array<VkWriteDescriptorSet, 3> writeDescriptors{};

				auto shader = material->GetShader().As<VulkanShader>();
				VkDescriptorSetLayout descriptorSetLayout = shader->GetDescriptorSetLayout(0);

				VkDescriptorSetAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayout;

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "PreIntegration");
				pipeline->RT_Begin(commandBuffer);

				auto visibilityDescriptorImageInfo = visibilityImage->GetDescriptorInfo();
				auto depthDescriptorImageInfo = depthImage->GetDescriptorInfo();
				depthDescriptorImageInfo.sampler = VulkanRenderer::GetPointSampler();

				for (uint32_t i = 1; i < visibilityTexture->GetMipLevelCount(); i++)
				{
					Renderer::RT_BeginGPUPerfMarker(commandBuffer, fmt::format("PreIntegration-Pass({})", i));
					auto [mipWidth, mipHeight] = visibilityTexture->GetMipSize(i);
					const auto workGroupsX = (uint32_t)glm::ceil((float)mipWidth / 8.0f);
					const auto workGroupsY = (uint32_t)glm::ceil((float)mipHeight / 8.0f);

					// Output visibilityImage
					visibilityDescriptorImageInfo.imageView = visibilityImage->RT_GetMipImageView(i);

					const VkDescriptorSet descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
					writeDescriptors[0] = *shader->GetDescriptorSet("o_VisibilityImage");
					writeDescriptors[0].dstSet = descriptorSet;
					writeDescriptors[0].pImageInfo = &visibilityDescriptorImageInfo;

					// Input visibilityImage
					writeDescriptors[1] = *shader->GetDescriptorSet("u_VisibilityTex");
					writeDescriptors[1].dstSet = descriptorSet;
					writeDescriptors[1].pImageInfo = &visibilityImage->GetDescriptorInfo();

					// Input HZB
					writeDescriptors[2] = *shader->GetDescriptorSet("u_HZB");
					writeDescriptors[2].dstSet = descriptorSet;
					writeDescriptors[2].pImageInfo = &depthDescriptorImageInfo;

					vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

					auto [width, height] = visibilityTexture->GetMipSize(i);
					glm::vec2 resFactor = 1.0f / glm::vec2{ width, height };
					preIntegrationComputePushConstants.HZBResFactor = resFactor * hzbUVFactor;
					preIntegrationComputePushConstants.ResFactor = resFactor;
					preIntegrationComputePushConstants.ProjectionParams = projectionParams;
					preIntegrationComputePushConstants.PrevLod = (int)i - 1;

					pipeline->SetPushConstants(&preIntegrationComputePushConstants, sizeof(preIntegrationComputePushConstants));
					pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
					Renderer::RT_EndGPUPerfMarker(commandBuffer);
				}
				Renderer::RT_EndGPUPerfMarker(commandBuffer);
				pipeline->End();
			});
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.PreIntegrationQuery);
	}

	void SceneRenderer::LightCullingPass()
	{
		m_LightCullingMaterial->Set("u_DepthMap", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());
		m_LightCullingMaterial->Set("o_Debug", m_GTAODebugOutputImage);

		m_GPUTimeQueries.LightCullingPassQuery = m_CommandBuffer->BeginTimestampQuery();
		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "LightCulling", { 1.0f, 1.0f, 1.0f, 1.0f });
		Renderer::LightCulling(m_CommandBuffer, m_LightCullingPipeline, m_UniformBufferSet, m_StorageBufferSet, m_LightCullingMaterial, m_LightCullingWorkGroups);
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.LightCullingPassQuery);
	}

	void SceneRenderer::RayMarchingPass()
	{
		static int FroxelIndex = 0;

		int swapIndex = FroxelIndex % 2;

		m_RayInjectionMaterial[swapIndex]->Set("u_BlueNoise", m_BlueNoiseTextures[FroxelIndex]->GetImage());


		m_GPUTimeQueries.RayMarchingQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::DispatchComputeShader(m_CommandBuffer, m_RayInjectionPipeline, m_UniformBufferSet, nullptr, m_RayInjectionMaterial[swapIndex], m_RayInjectionWorkGroups);

		Renderer::DispatchComputeShader(m_CommandBuffer, m_ScatteringPipeline, m_UniformBufferSet, nullptr, m_ScatteringMaterial[swapIndex], m_ScatteringWorkGroups);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.RayMarchingQuery);


		if (++ FroxelIndex == 8)
			FroxelIndex = 0;
	}



	void SceneRenderer::GeometryPass()
	{
		X2_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_GPUTimeQueries.GeometryPassQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::BeginRenderPass(m_CommandBuffer, m_SelectedGeometryPipeline->GetSpecification().RenderPass);
		for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_SelectedGeometryPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount, m_SelectedGeometryMaterial);
		}
		for (auto& [mk, dc] : m_SelectedMeshDrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			/*if (dc.IsRigged)
			{
				const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
				Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_SelectedGeometryPipelineAnim, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex + dc.InstanceOffset, dc.InstanceCount, m_SelectedGeometryMaterial);
			}
			else
			{*/
				Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_SelectedGeometryPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), {}, 0, dc.InstanceCount, m_SelectedGeometryMaterial);
			//}
		}
		Renderer::EndRenderPass(m_CommandBuffer);

		Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryPipeline->GetSpecification().RenderPass);
		// Skybox
		m_SkyboxMaterial->Set("u_Uniforms.TextureLod", m_SceneData.SkyboxLod);
		m_SkyboxMaterial->Set("u_Uniforms.Intensity", m_SceneData.SceneEnvironmentIntensity);

		const Ref<VulkanTextureCube> radianceMap = m_SceneData.SceneEnvironment ? m_SceneData.SceneEnvironment->RawEnvMap : Renderer::GetBlackCubeTexture();
		m_SkyboxMaterial->Set("u_Texture", radianceMap);

		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Skybox", { 0.3f, 0.0f, 1.0f, 1.0f });
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_SkyboxPipeline, m_UniformBufferSet, nullptr, m_SkyboxMaterial);
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		// Render static meshes
		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Static Meshes");
		for (auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);

			if (!IsUsingTAA(m_Options.AAMethod) || !m_Options.EnableAA)
				Renderer::RenderStaticMesh(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);
			else
				Renderer::RenderStaticMesh(m_CommandBuffer, m_GeometryTAAPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);

		}
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		// Render dynamic meshes
		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Dynamic Meshes");
		for (auto& [mk, dc] : m_DrawList)
		{
			const auto& transformData = m_CurTransformMap->at(mk);
			/*if (dc.IsRigged)
			{
				const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
				Renderer::RenderSubmeshInstanced(m_CommandBuffer, m_GeometryPipelineAnim, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex, dc.InstanceCount);
			}
			else
			{*/
				Renderer::RenderSubmeshInstanced(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount);
			//}
		}
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		{
			// Render static meshes
			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Static Transparent Meshes");
			for (auto& [mk, dc] : m_TransparentStaticMeshDrawList)
			{
				const auto& transformData = m_CurTransformMap->at(mk);
				Renderer::RenderStaticMesh(m_CommandBuffer, m_TransparentGeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);

			}
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

			// Render dynamic meshes
			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Dynamic Transparent Meshes");
			for (auto& [mk, dc] : m_TransparentDrawList)
			{
				const auto& transformData = m_CurTransformMap->at(mk);
				//Renderer::RenderSubmesh(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), dc.Transform);
				Renderer::RenderSubmeshInstanced(m_CommandBuffer, m_TransparentGeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount);
			}
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);
		}


		Renderer::EndRenderPass(m_CommandBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GeometryPassQuery);
	}

	void SceneRenderer::PreConvolutionCompute()
	{
		X2_PROFILE_FUNC();

		// TODO: Other techniques might need it in the future
		if (!m_Options.EnableSSR)
			return;
		Ref<VulkanComputePipeline> pipeline = m_GaussianBlurPipeline.As<VulkanComputePipeline>();
		struct PreConvolutionComputePushConstants
		{
			int PrevLod = 0;
			int Mode = 0; // 0 = Copy, 1 = GaussianHorizontal, 2 = GaussianVertical
		} preConvolutionComputePushConstants;

		//Might change to be maximum res used by other techniques other than SSR.
		int halfRes = int(m_SSROptions.HalfRes);

		m_GPUTimeQueries.PreConvolutionQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::Submit([preConvolutionComputePushConstants, inputColorImage = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>(),
			preConvolutedTexture = m_PreConvolutedTexture.As<VulkanTexture2D>(), commandBuffer = m_CommandBuffer.As<VulkanRenderCommandBuffer>(),
			material = m_GaussianBlurMaterial.As<VulkanMaterial>(), pipeline, halfRes]() mutable
			{
				const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				Ref<VulkanImage2D> preConvolutedImage = preConvolutedTexture->GetImage().As<VulkanImage2D>();

				auto shader = material->GetShader().As<VulkanShader>();

				VkDescriptorSetLayout descriptorSetLayout = shader->GetDescriptorSetLayout(0);
				VkDescriptorSetAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayout;

				std::array<VkWriteDescriptorSet, 2> writeDescriptors{};

				const VkDescriptorSet descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
				auto descriptorImageInfo = preConvolutedImage->GetDescriptorInfo();
				descriptorImageInfo.imageView = preConvolutedImage->RT_GetMipImageView(0);
				descriptorImageInfo.sampler = VK_NULL_HANDLE;

				// Output Pre-convoluted image
				writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
				writeDescriptors[0].dstSet = descriptorSet;
				writeDescriptors[0].pImageInfo = &descriptorImageInfo;

				// Input original colors
				writeDescriptors[1] = *shader->GetDescriptorSet("u_Input");
				writeDescriptors[1].dstSet = descriptorSet;
				writeDescriptors[1].pImageInfo = &inputColorImage->GetDescriptorInfo();

				uint32_t workGroupsX = (uint32_t)glm::ceil((float)inputColorImage->GetWidth() / 16.0f);
				uint32_t workGroupsY = (uint32_t)glm::ceil((float)inputColorImage->GetHeight() / 16.0f);

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Pre-Convolution");
				pipeline->RT_Begin(commandBuffer);
				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Pre-Convolution-FirstPass");
				pipeline->SetPushConstants(&preConvolutionComputePushConstants, sizeof preConvolutionComputePushConstants);
				pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
				Renderer::RT_EndGPUPerfMarker(commandBuffer);

				const uint32_t mipCount = preConvolutedTexture->GetMipLevelCount();
				for (uint32_t mip = 1; mip < mipCount; mip++)
				{
					Renderer::RT_BeginGPUPerfMarker(commandBuffer, fmt::format("Pre-Convolution-Mip({})", mip));

					auto [mipWidth, mipHeight] = preConvolutedTexture->GetMipSize(mip);
					workGroupsX = (uint32_t)glm::ceil((float)mipWidth / 16.0f);
					workGroupsY = (uint32_t)glm::ceil((float)mipHeight / 16.0f);

					const VkDescriptorSet descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
					// Output image
					descriptorImageInfo.imageView = preConvolutedImage->RT_GetMipImageView(mip);

					writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
					writeDescriptors[0].dstSet = descriptorSet;
					writeDescriptors[0].pImageInfo = &descriptorImageInfo;

					writeDescriptors[1] = *shader->GetDescriptorSet("u_Input");
					writeDescriptors[1].dstSet = descriptorSet;
					writeDescriptors[1].pImageInfo = &preConvolutedImage->GetDescriptorInfo();

					vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);
					preConvolutionComputePushConstants.PrevLod = (int)mip - 1;

					auto blur = [&](const int mode)
					{
						Renderer::RT_BeginGPUPerfMarker(commandBuffer, fmt::format("Pre-Convolution-Mode({})", mode));
						preConvolutionComputePushConstants.Mode = (int)mode;
						pipeline->SetPushConstants(&preConvolutionComputePushConstants, sizeof(preConvolutionComputePushConstants));
						pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
						Renderer::RT_EndGPUPerfMarker(commandBuffer);
					};



					blur(1); // Horizontal blur
					blur(2); // Vertical Blur
					Renderer::RT_EndGPUPerfMarker(commandBuffer);

				}
				pipeline->End();
				Renderer::RT_EndGPUPerfMarker(commandBuffer);
			});
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.PreConvolutionQuery);
	}

	void SceneRenderer::DeinterleavingPass()
	{
		m_DeinterleavingMaterial->Set("u_Depth", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());

		for (int i = 0; i < 2; i++)
		{
			Renderer::Submit([i, material = m_DeinterleavingMaterial]() mutable
				{
					material->Set("u_Info.UVOffsetIndex", i);
				});
			Renderer::BeginRenderPass(m_CommandBuffer, m_DeinterleavingPipelines[i]->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_DeinterleavingPipelines[i], m_UniformBufferSet, nullptr, m_DeinterleavingMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRenderer::HBAOCompute()
	{
		m_HBAOMaterial->Set("u_LinearDepthTexArray", m_DeinterleavingPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());
		m_HBAOMaterial->Set("u_ViewNormalsMaskTex", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(1));
		m_HBAOMaterial->Set("o_Color", m_HBAOOutputImage);



		Renderer::DispatchComputeShader(m_CommandBuffer, m_HBAOPipeline, m_UniformBufferSet, nullptr, m_HBAOMaterial, m_HBAOWorkGroups);
	}

	void SceneRenderer::GTAOCompute()
	{
		m_GTAOMaterial->Set("u_HiZDepth", m_HierarchicalDepthTexture);
		m_GTAOMaterial->Set("u_HilbertLut", Renderer::GetHilbertLut());
		m_GTAOMaterial->Set("u_ViewNormal", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(1));
		m_GTAOMaterial->Set("o_AOwBentNormals", m_GTAOOutputImage);
		m_GTAOMaterial->Set("o_Edges", m_GTAOEdgesOutputImage);



		const Buffer pushConstantBuffer(&GTAODataCB, sizeof GTAODataCB);

		m_GPUTimeQueries.GTAOPassQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::DispatchComputeShader(m_CommandBuffer, m_GTAOPipeline, m_UniformBufferSet, nullptr, m_GTAOMaterial, m_GTAOWorkGroups, pushConstantBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GTAOPassQuery);
	}

	void SceneRenderer::GTAODenoiseCompute()
	{

		struct DenoisePushConstant
		{
			float DenoiseBlurBeta;
			bool HalfRes;
			char Padding1[3]{ 0, 0, 0 };
		} denoisePushConstant;

		denoisePushConstant.DenoiseBlurBeta = GTAODataCB.DenoiseBlurBeta;
		denoisePushConstant.HalfRes = GTAODataCB.HalfRes;
		const Buffer pushConstantBuffer(&denoisePushConstant, sizeof denoisePushConstant);

		m_GPUTimeQueries.GTAODenoisePassQuery = m_CommandBuffer->BeginTimestampQuery();
		for (uint32_t pass = 0; pass < (uint32_t)m_Options.GTAODenoisePasses; ++pass)
			Renderer::DispatchComputeShader(m_CommandBuffer, m_GTAODenoisePipeline, m_UniformBufferSet, nullptr, m_GTAODenoiseMaterial[uint32_t(pass % 2 != 0)], m_GTAODenoiseWorkGroups, pushConstantBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GTAODenoisePassQuery);
	}

	void SceneRenderer::ReinterleavingPass()
	{
		if (!m_Options.EnableHBAO)
		{
			ClearPass(m_ReinterleavingPipeline->GetSpecification().RenderPass);
			return;
		}
		Renderer::BeginRenderPass(m_CommandBuffer, m_ReinterleavingPipeline->GetSpecification().RenderPass);
		m_ReinterleavingMaterial->Set("u_TexResultsArray", m_HBAOOutputImage);
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_ReinterleavingPipeline, nullptr, nullptr, m_ReinterleavingMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::HBAOBlurPass()
	{
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_HBAOBlurPipelines[0]->GetSpecification().RenderPass);
			m_HBAOBlurMaterials[0]->Set("u_Info.InvResDirection", glm::vec2{ m_InvViewportWidth, 0.0f });
			m_HBAOBlurMaterials[0]->Set("u_Info.Sharpness", m_Options.HBAOBlurSharpness);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_HBAOBlurPipelines[0], nullptr, nullptr, m_HBAOBlurMaterials[0]);
			Renderer::EndRenderPass(m_CommandBuffer);
		}

		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_HBAOBlurPipelines[1]->GetSpecification().RenderPass);
			m_HBAOBlurMaterials[1]->Set("u_Info.InvResDirection", glm::vec2{ 0.0f, m_InvViewportHeight });
			m_HBAOBlurMaterials[1]->Set("u_Info.Sharpness", m_Options.HBAOBlurSharpness);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_HBAOBlurPipelines[1], nullptr, nullptr, m_HBAOBlurMaterials[1]);
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRenderer::AOComposite()
	{
		if (m_Options.EnableGTAO)
			m_AOCompositeMaterial->Set("u_GTAOTex", m_GTAOFinalImage);
		if (m_Options.EnableHBAO)
			m_AOCompositeMaterial->Set("u_HBAOTex", m_HBAOBlurPipelines[1]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());
		m_GPUTimeQueries.AOCompositePassQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_CommandBuffer, m_AOCompositeRenderPass);
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_AOCompositePipeline, nullptr, m_AOCompositeMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.AOCompositePassQuery);
	}

	void SceneRenderer::JumpFloodPass()
	{
		X2_PROFILE_FUNC();

		m_GPUTimeQueries.JumpFloodPassQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodInitPipeline->GetSpecification().RenderPass);

		auto framebuffer = m_SelectedGeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer;
		m_JumpFloodInitMaterial->Set("u_Texture", framebuffer->GetImage());

		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_JumpFloodInitPipeline, nullptr, m_JumpFloodInitMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);

		m_JumpFloodPassMaterial[0]->Set("u_Texture", m_TempFramebuffers[0]->GetImage());
		m_JumpFloodPassMaterial[1]->Set("u_Texture", m_TempFramebuffers[1]->GetImage());

		int steps = 2;
		int step = (int)glm::round(glm::pow<int>(steps - 1, 2));
		int index = 0;
		Buffer vertexOverrides;
		Ref<VulkanFramebuffer> passFB = m_JumpFloodPassPipeline[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer;
		glm::vec2 texelSize = { 1.0f / (float)passFB->GetWidth(), 1.0f / (float)passFB->GetHeight() };
		vertexOverrides.Allocate(sizeof(glm::vec2) + sizeof(int));
		vertexOverrides.Write(glm::value_ptr(texelSize), sizeof(glm::vec2));
		while (step != 0)
		{
			vertexOverrides.Write(&step, sizeof(int), sizeof(glm::vec2));

			Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodPassPipeline[index]->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuadWithOverrides(m_CommandBuffer, m_JumpFloodPassPipeline[index], nullptr, m_JumpFloodPassMaterial[index], vertexOverrides, Buffer());
			Renderer::EndRenderPass(m_CommandBuffer);

			index = (index + 1) % 2;
			step /= 2;
		}

		vertexOverrides.Release();

		m_JumpFloodCompositeMaterial->Set("u_Texture", m_TempFramebuffers[1]->GetImage());
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.JumpFloodPassQuery);
	}

	void SceneRenderer::SSRCompute()
	{
		X2_PROFILE_FUNC();

		m_SSRMaterial->Set("outColor", m_SSRImage);
		m_SSRMaterial->Set("u_InputColor", m_PreConvolutedTexture);
		m_SSRMaterial->Set("u_VisibilityBuffer", m_VisibilityTexture);
		m_SSRMaterial->Set("u_HiZBuffer", m_HierarchicalDepthTexture);
		m_SSRMaterial->Set("u_Normal", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(1));
		m_SSRMaterial->Set("u_MetalnessRoughness", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(2));

		if ((int)m_Options.ReflectionOcclusionMethod & (int)ShaderDef::AOMethod::GTAO)
			m_SSRMaterial->Set("u_GTAOTex", m_GTAOFinalImage);

		if ((int)m_Options.ReflectionOcclusionMethod & (int)ShaderDef::AOMethod::HBAO)
			m_SSRMaterial->Set("u_HBAOTex", (m_HBAOBlurPipelines[1]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage()));

		const Buffer pushConstantsBuffer(&m_SSROptions, sizeof m_SSROptions);

		m_GPUTimeQueries.SSRQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::DispatchComputeShader(m_CommandBuffer, m_SSRPipeline, m_UniformBufferSet, nullptr, m_SSRMaterial, m_SSRWorkGroups, pushConstantsBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SSRQuery);
	}

	void SceneRenderer::SSRCompositePass()
	{
		// Currently scales the SSR, renders with transparency.
		// The alpha channel is the confidence.
		m_SSRCompositeMaterial->Set("u_SSR", m_SSRImage);

		m_GPUTimeQueries.SSRCompositeQuery = m_CommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_CommandBuffer, m_SSRCompositePipeline->GetSpecification().RenderPass);
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_SSRCompositePipeline, m_UniformBufferSet, m_SSRCompositeMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SSRCompositeQuery);
	}

	void SceneRenderer::BloomCompute()
	{
		Ref<VulkanComputePipeline> pipeline = m_BloomComputePipeline.As<VulkanComputePipeline>();

		//m_BloomComputeMaterial->Set("o_Image", m_BloomComputeTexture);

		struct BloomComputePushConstants
		{
			glm::vec4 Params;
			float LOD = 0.0f;
			int Mode = 0; // 0 = prefilter, 1 = downsample, 2 = firstUpsample, 3 = upsample
		} bloomComputePushConstants;
		bloomComputePushConstants.Params = { m_BloomSettings.Threshold, m_BloomSettings.Threshold - m_BloomSettings.Knee, m_BloomSettings.Knee * 2.0f, 0.25f / m_BloomSettings.Knee };
		bloomComputePushConstants.Mode = 0;

		auto inputImage = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>();

		m_GPUTimeQueries.BloomComputePassQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::Submit([bloomComputePushConstants, inputImage, workGroupSize = m_BloomComputeWorkgroupSize, commandBuffer = m_CommandBuffer, bloomTextures = m_BloomComputeTextures, ubs = m_UniformBufferSet, material = m_BloomComputeMaterial.As<VulkanMaterial>(), pipeline]() mutable
			{
				constexpr bool useComputeQueue = false;

				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				Ref<VulkanImage2D> images[3] =
				{
					bloomTextures[0]->GetImage().As<VulkanImage2D>(),
					bloomTextures[1]->GetImage().As<VulkanImage2D>(),
					bloomTextures[2]->GetImage().As<VulkanImage2D>()
				};

				auto shader = pipeline->GetShader().As<VulkanShader>();

				auto descriptorImageInfo = images[0]->GetDescriptorInfo();
				descriptorImageInfo.imageView = images[0]->RT_GetMipImageView(0);

				std::array<VkWriteDescriptorSet, 3> writeDescriptors;

				VkDescriptorSetLayout descriptorSetLayout = shader->GetDescriptorSetLayout(0);

				VkDescriptorSetAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayout;

				pipeline->RT_Begin(useComputeQueue ? nullptr : commandBuffer);

				if (false)
				{
					VkImageMemoryBarrier imageMemoryBarrier = {};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageMemoryBarrier.image = inputImage->GetImageInfo().Image;
					imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 };
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					vkCmdPipelineBarrier(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}

				// Output image
				VkDescriptorSet descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
				writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
				writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[0].pImageInfo = &descriptorImageInfo;

				// Input image
				writeDescriptors[1] = *shader->GetDescriptorSet("u_Texture");
				writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[1].pImageInfo = &inputImage->GetDescriptorInfo();

				writeDescriptors[2] = *shader->GetDescriptorSet("u_BloomTexture");
				writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[2].pImageInfo = &inputImage->GetDescriptorInfo();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

				uint32_t workGroupsX = bloomTextures[0]->GetWidth() / workGroupSize;
				uint32_t workGroupsY = bloomTextures[0]->GetHeight() / workGroupSize;

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Bloom-Prefilter");
				pipeline->SetPushConstants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
				pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
				Renderer::RT_EndGPUPerfMarker(commandBuffer);

				{
					VkImageMemoryBarrier imageMemoryBarrier = {};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageMemoryBarrier.image = images[0]->GetImageInfo().Image;
					imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[0]->GetSpecification().Mips, 0, 1 };
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					vkCmdPipelineBarrier(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}

				bloomComputePushConstants.Mode = 1;


				uint32_t mips = bloomTextures[0]->GetMipLevelCount() - 2;
				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Bloom-DownSample");
				for (uint32_t i = 1; i < mips; i++)
				{
					auto [mipWidth, mipHeight] = bloomTextures[0]->GetMipSize(i);
					workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
					workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);

					{
						// Output image
						descriptorImageInfo.imageView = images[1]->RT_GetMipImageView(i);

						descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
						writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
						writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
						writeDescriptors[0].pImageInfo = &descriptorImageInfo;

						// Input image
						writeDescriptors[1] = *shader->GetDescriptorSet("u_Texture");
						writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
						auto descriptor = bloomTextures[0]->GetImage().As<VulkanImage2D>()->GetDescriptorInfo();
						//descriptor.sampler = samplerClamp;
						writeDescriptors[1].pImageInfo = &descriptor;

						writeDescriptors[2] = *shader->GetDescriptorSet("u_BloomTexture");
						writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
						writeDescriptors[2].pImageInfo = &inputImage->GetDescriptorInfo();

						vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

						bloomComputePushConstants.LOD = i - 1.0f;
						pipeline->SetPushConstants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
						pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
					}

					{
						VkImageMemoryBarrier imageMemoryBarrier = {};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
						imageMemoryBarrier.image = images[1]->GetImageInfo().Image;
						imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[1]->GetSpecification().Mips, 0, 1 };
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						vkCmdPipelineBarrier(
							pipeline->GetActiveCommandBuffer(),
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &imageMemoryBarrier);
					}

					{
						descriptorImageInfo.imageView = images[0]->RT_GetMipImageView(i);

						// Output image
						descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
						writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
						writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
						writeDescriptors[0].pImageInfo = &descriptorImageInfo;

						// Input image
						writeDescriptors[1] = *shader->GetDescriptorSet("u_Texture");
						writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
						auto descriptor = bloomTextures[1]->GetImage().As<VulkanImage2D>()->GetDescriptorInfo();
						//descriptor.sampler = samplerClamp;
						writeDescriptors[1].pImageInfo = &descriptor;

						writeDescriptors[2] = *shader->GetDescriptorSet("u_BloomTexture");
						writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
						writeDescriptors[2].pImageInfo = &inputImage->GetDescriptorInfo();

						vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

						bloomComputePushConstants.LOD = (float)i;
						pipeline->SetPushConstants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
						pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
					}

					{
						VkImageMemoryBarrier imageMemoryBarrier = {};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
						imageMemoryBarrier.image = images[0]->GetImageInfo().Image;
						imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[0]->GetSpecification().Mips, 0, 1 };
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						vkCmdPipelineBarrier(
							pipeline->GetActiveCommandBuffer(),
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &imageMemoryBarrier);
					}
				}
				Renderer::RT_EndGPUPerfMarker(commandBuffer);

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Bloom-FirstUpsamle");
				bloomComputePushConstants.Mode = 2;
				workGroupsX *= 2;
				workGroupsY *= 2;

				// Output image
				descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
				descriptorImageInfo.imageView = images[2]->RT_GetMipImageView(mips - 2);

				writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
				writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[0].pImageInfo = &descriptorImageInfo;

				// Input image
				writeDescriptors[1] = *shader->GetDescriptorSet("u_Texture");
				writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[1].pImageInfo = &bloomTextures[0]->GetImage().As<VulkanImage2D>()->GetDescriptorInfo();

				writeDescriptors[2] = *shader->GetDescriptorSet("u_BloomTexture");
				writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[2].pImageInfo = &inputImage->GetDescriptorInfo();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

				bloomComputePushConstants.LOD--;
				pipeline->SetPushConstants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));

				auto [mipWidth, mipHeight] = bloomTextures[2]->GetMipSize(mips - 2);
				workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
				workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);
				pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);

				{
					VkImageMemoryBarrier imageMemoryBarrier = {};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageMemoryBarrier.image = images[2]->GetImageInfo().Image;
					imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[2]->GetSpecification().Mips, 0, 1 };
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					vkCmdPipelineBarrier(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}

				Renderer::RT_EndGPUPerfMarker(commandBuffer);

				Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Bloom-Upsample");
				bloomComputePushConstants.Mode = 3;

				// Upsample
				for (int32_t mip = mips - 3; mip >= 0; mip--)
				{
					auto [mipWidth, mipHeight] = bloomTextures[2]->GetMipSize(mip);
					workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
					workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);

					// Output image
					descriptorImageInfo.imageView = images[2]->RT_GetMipImageView(mip);
					auto descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
					writeDescriptors[0] = *shader->GetDescriptorSet("o_Image");
					writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
					writeDescriptors[0].pImageInfo = &descriptorImageInfo;

					// Input image
					writeDescriptors[1] = *shader->GetDescriptorSet("u_Texture");
					writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
					writeDescriptors[1].pImageInfo = &bloomTextures[0]->GetImage().As<VulkanImage2D>()->GetDescriptorInfo();

					writeDescriptors[2] = *shader->GetDescriptorSet("u_BloomTexture");
					writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
					writeDescriptors[2].pImageInfo = &images[2]->GetDescriptorInfo();

					vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

					bloomComputePushConstants.LOD = (float)mip;
					pipeline->SetPushConstants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
					pipeline->Dispatch(descriptorSet, workGroupsX, workGroupsY, 1);

					{
						VkImageMemoryBarrier imageMemoryBarrier = {};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
						imageMemoryBarrier.image = images[2]->GetImageInfo().Image;
						imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[2]->GetSpecification().Mips, 0, 1 };
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						vkCmdPipelineBarrier(
							pipeline->GetActiveCommandBuffer(),
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &imageMemoryBarrier);
					}
				}
				Renderer::RT_EndGPUPerfMarker(commandBuffer);

				pipeline->End();
			});
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.BloomComputePassQuery);
	}

	void SceneRenderer::EdgeDetectionPass()
	{
		Renderer::BeginRenderPass(m_CommandBuffer, m_EdgeDetectionPipeline->GetSpecification().RenderPass);
		m_EdgeDetectionMaterial->Set("u_ViewNormalsTexture", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(1));
		m_EdgeDetectionMaterial->Set("u_DepthTexture", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_EdgeDetectionPipeline, m_UniformBufferSet, m_EdgeDetectionMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::CompositePass()
	{
		X2_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_GPUTimeQueries.CompositePassQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePipeline->GetSpecification().RenderPass, true);

		auto framebuffer = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer;
		float exposure = m_SceneData.SceneCamera.Camera.GetExposure();
		int textureSamples = framebuffer->GetSpecification().Samples;

		m_CompositeMaterial->Set("u_Uniforms.Exposure", exposure);
		if (m_BloomSettings.Enabled)
		{
			m_CompositeMaterial->Set("u_Uniforms.BloomIntensity", m_BloomSettings.Intensity);
			m_CompositeMaterial->Set("u_Uniforms.BloomDirtIntensity", m_BloomSettings.DirtIntensity);
		}
		else
		{
			m_CompositeMaterial->Set("u_Uniforms.BloomIntensity", 0.0f);
			m_CompositeMaterial->Set("u_Uniforms.BloomDirtIntensity", 0.0f);
		}

		m_CompositeMaterial->Set("u_Uniforms.Opacity", m_Opacity);
		m_CompositeMaterial->Set("u_Uniforms.Time", Application::Get().GetTime());

		// CompositeMaterial->Set("u_Uniforms.TextureSamples", textureSamples);

		auto inputImage = m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage();
		m_CompositeMaterial->Set("u_Texture", inputImage);
		m_CompositeMaterial->Set("u_BloomTexture", m_BloomComputeTextures[2]);
		m_CompositeMaterial->Set("u_BloomDirtTexture", m_BloomDirtTexture);
		m_CompositeMaterial->Set("u_DepthTexture", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());
		m_CompositeMaterial->Set("u_TransparentDepthTexture", m_PreDepthTransparentPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());

		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Composite");
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_CompositePipeline, m_UniformBufferSet, m_CompositeMaterial);
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		if (m_DOFSettings.Enabled)
		{
			Renderer::EndRenderPass(m_CommandBuffer);

			Renderer::BeginRenderPass(m_CommandBuffer, m_DOFPipeline->GetSpecification().RenderPass);
			m_DOFMaterial->Set("u_Texture", m_CompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());
			m_DOFMaterial->Set("u_DepthTexture", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());
			m_DOFMaterial->Set("u_Uniforms.DOFParams", glm::vec2(m_DOFSettings.FocusDistance, m_DOFSettings.BlurSize));
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_DOFPipeline, m_UniformBufferSet, m_DOFMaterial);

			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "JumpFlood-Composite");
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_JumpFloodCompositePipeline, nullptr, m_JumpFloodCompositeMaterial);
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

			Renderer::EndRenderPass(m_CommandBuffer);

			// Copy DOF image to composite pipeline
			Renderer::CopyImage(m_CommandBuffer,
				m_DOFPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(),
				m_CompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());

			// WIP - will later be used for debugging/editor mouse picking
			if (m_ReadBackImage)
			{
				Renderer::CopyImage(m_CommandBuffer,
					m_DOFPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(),
					m_ReadBackImage);

				{
					auto alloc = m_ReadBackImage.As<VulkanImage2D>()->GetImageInfo().MemoryAlloc;
					VulkanAllocator allocator("SceneRenderer");
					glm::vec4* mappedMem = allocator.MapMemory<glm::vec4>(alloc);
					delete[] m_ReadBackBuffer;
					m_ReadBackBuffer = new glm::vec4[m_ReadBackImage->GetWidth() * m_ReadBackImage->GetHeight()];
					memcpy(m_ReadBackBuffer, mappedMem, sizeof(glm::vec4) * m_ReadBackImage->GetWidth() * m_ReadBackImage->GetHeight());
					allocator.UnmapMemory(alloc);
				}
			}
		}
		else
		{
			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "JumpFlood-Composite");
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_JumpFloodCompositePipeline, nullptr, m_JumpFloodCompositeMaterial);
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

			Renderer::EndRenderPass(m_CommandBuffer);
		}

		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.CompositePassQuery);

		// Grid
		if (GetOptions().ShowGrid)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_ExternalCompositeRenderPass);
			const static glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(8.0f));
			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Grid");
			Renderer::RenderQuad(m_CommandBuffer, m_GridPipeline, m_UniformBufferSet, nullptr, m_GridMaterial, transform);
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);
			Renderer::EndRenderPass(m_CommandBuffer);
		}

		if (m_Options.ShowSelectedInWireframe)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_ExternalCompositeRenderPass);

			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Static Meshes Wireframe");
			for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
			{
				const auto& transformData = m_CurTransformMap->at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_GeometryWireframePipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount, m_WireframeMaterial);
			}
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Dynamic Meshes Wireframe");
			for (auto& [mk, dc] : m_SelectedMeshDrawList)
			{
				const auto& transformData = m_CurTransformMap->at(mk);
				/*if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_GeometryWireframePipelineAnim, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex + dc.InstanceOffset, dc.InstanceCount, m_WireframeMaterial);
				}
				else
				{*/
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, m_GeometryWireframePipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), {}, 0, dc.InstanceCount, m_WireframeMaterial);
				//}
			}
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

			Renderer::EndRenderPass(m_CommandBuffer);
		}

		if (m_Options.ShowPhysicsColliders)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_ExternalCompositeRenderPass);
			auto pipeline = m_Options.ShowPhysicsCollidersOnTop ? m_GeometryWireframeOnTopPipeline : m_GeometryWireframePipeline;
			auto pipelineAnim = m_Options.ShowPhysicsCollidersOnTop ? m_GeometryWireframeOnTopPipelineAnim : m_GeometryWireframePipelineAnim;

			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Static Meshes Collider");
			for (auto& [mk, dc] : m_StaticColliderDrawList)
			{
				X2_CORE_VERIFY(m_CurTransformMap->find(mk) != m_CurTransformMap->end());
				const auto& transformData = m_CurTransformMap->at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, pipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, dc.OverrideMaterial);
			}
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);


			SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Dynamic Meshes Collider");
			for (auto& [mk, dc] : m_ColliderDrawList)
			{
				X2_CORE_VERIFY(m_CurTransformMap->find(mk) != m_CurTransformMap->end());
				const auto& transformData = m_CurTransformMap->at(mk);
				/*if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, pipelineAnim, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, m_BoneTransformStorageBuffers, boneTransformsData.BoneTransformsBaseIndex, dc.InstanceCount, m_SimpleColliderMaterial);
				}
				else
				{*/
					Renderer::RenderMeshWithMaterial(m_CommandBuffer, pipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, {}, 0, dc.InstanceCount, m_SimpleColliderMaterial);
				//}
			}
			SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}


	void SceneRenderer::TAAPass()
	{

		{
			//Cpoy Cuurent Color Image
			Ref<SceneRenderer> instance = this;
			//Renderer::Submit([instance]() mutable
			//	{
			//		auto inputImage = instance->m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>();

			//		Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), inputImage->GetImageInfo().Image,
			//			VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			//			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			//			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			//			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });

			//		Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), instance->m_TAACurColorImage->GetImageInfo().Image,
			//			 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			//			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			//			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			//			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });


			//		VkImageCopy copyRegion{};
			//		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			//		copyRegion.srcOffset = { 0, 0, 0 }; // 从源图像的哪个位置开始复制
			//		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			//		copyRegion.dstOffset = { 0, 0, 0 }; // 复制到目标图像的哪个位置
			//		copyRegion.extent = { inputImage->GetWidth(), inputImage->GetHeight(), 1 }; // 复制的区域的大小

			//		vkCmdCopyImage(
			//			instance->m_CommandBuffer->GetActiveCommandBuffer(),
			//			inputImage->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			//			instance->m_TAACurColorImage->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			//			1, &copyRegion
			//		);


			//		Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), inputImage->GetImageInfo().Image,
			//			VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
			//			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			//			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			//			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });

			//		Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), instance->m_TAACurColorImage->GetImageInfo().Image,
			//			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			//			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			//			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			//			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });



			//	});

			//Tone Mapping Pass
			m_TAAToneMappingMaterial->Set("u_color", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>());
			
			m_GPUTimeQueries.TAAQuery = m_CommandBuffer->BeginTimestampQuery();
			Renderer::BeginRenderPass(m_CommandBuffer, m_TAAToneMappingPipeline->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_TAAToneMappingPipeline, m_UniformBufferSet, m_TAAToneMappingMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);


			Renderer::Submit([instance]() mutable
				{
					auto inputImage = instance->m_TAAToneMappingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>();

					Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), inputImage->GetImageInfo().Image,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });
				});

			//TAA Pass
			m_TAAMaterial->Set("u_color", m_TAAToneMappingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>());
			m_TAAMaterial->Set("u_velocity", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(3).As<VulkanImage2D>());
			m_TAAMaterial->Set("u_depth", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());

			static bool firstFrame = true;
			if (firstFrame)
			{
				m_TAAMaterial->Set("u_colorHistory", m_TAAToneMappingPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>());
				firstFrame = false;
			}
			else
				m_TAAMaterial->Set("u_colorHistory", m_TAAToneMappedPreColorImage);


			Renderer::BeginRenderPass(m_CommandBuffer, m_TAAPipeline->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_TAAPipeline, m_UniformBufferSet, m_TAAMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
	


			Renderer::Submit([instance]() mutable
				{
					auto inputImage = instance->m_TAAPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>();


					Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), inputImage->GetImageInfo().Image,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });

					Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), instance->m_TAAToneMappedPreColorImage->GetImageInfo().Image,
						VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });


					VkImageCopy copyRegion{};
					copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
					copyRegion.srcOffset = { 0, 0, 0 }; // 从源图像的哪个位置开始复制
					copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
					copyRegion.dstOffset = { 0, 0, 0 }; // 复制到目标图像的哪个位置
					copyRegion.extent = { inputImage->GetWidth(), inputImage->GetHeight(), 1 }; // 复制的区域的大小

					vkCmdCopyImage(
						instance->m_CommandBuffer->GetActiveCommandBuffer(),
						inputImage->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						instance->m_TAAToneMappedPreColorImage->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1, &copyRegion
					);


					Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), inputImage->GetImageInfo().Image,
						VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });

					Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), instance->m_TAAToneMappedPreColorImage->GetImageInfo().Image,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });

				});


			//Tone Mapping Pass
			m_TAAToneUnMappingMaterial->Set("u_color", m_TAAPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>());

			Renderer::BeginRenderPass(m_CommandBuffer, m_TAAToneUnMappingPipeline->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_TAAToneUnMappingPipeline, m_UniformBufferSet, m_TAAToneUnMappingMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);


			m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.TAAQuery);

		}
	}


	void SceneRenderer::SMAAPass()
	{
		Ref<SceneRenderer> instance = this;

		{
			m_SMAAEdgeDetectionMaterial->Set("colorTex", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>());

			m_GPUTimeQueries.SMAAEdgeDetectPassQuery = m_CommandBuffer->BeginTimestampQuery();
			Renderer::BeginRenderPass(m_CommandBuffer, m_SMAAEdgeDetectionPipeline->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_SMAAEdgeDetectionPipeline, m_UniformBufferSet, m_SMAAEdgeDetectionMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
			m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SMAAEdgeDetectPassQuery);
		}

		{

			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance]() mutable
				{
					auto inputImage = instance->m_SMAABlendWeightPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage().As<VulkanImage2D>();

					Utils::InsertImageMemoryBarrier(instance->m_CommandBuffer->GetActiveCommandBuffer(), inputImage->GetImageInfo().Image,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->GetSpecification().Mips, 0, 1 });
				});
			

			// Second Pass BlendWeight 
			m_SMAABlendWeightMaterial->Set("colorTex", m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0));
			m_SMAABlendWeightMaterial->Set("edgesTex", m_SMAAEdgeDetectionPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());
			m_SMAABlendWeightMaterial->Set("areaTex", Renderer::GetSMAAAreaLut());
			m_SMAABlendWeightMaterial->Set("searchTex", Renderer::GetSMAASearchLut());

			m_GPUTimeQueries.SMAABlendWeightPassQuery = m_CommandBuffer->BeginTimestampQuery();
			Renderer::BeginRenderPass(m_CommandBuffer, m_SMAABlendWeightPipeline->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_SMAABlendWeightPipeline, m_UniformBufferSet, m_SMAABlendWeightMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
			m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SMAABlendWeightPassQuery);

		
		}


		
		{
			m_SMAANeighborBlendMaterial->Set("colorTex", m_SMAABlendWeightPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(0));
			m_SMAANeighborBlendMaterial->Set("blendTex", m_SMAABlendWeightPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(1));

			m_GPUTimeQueries.SMAANeighborBlendPassQuery = m_CommandBuffer->BeginTimestampQuery();
			Renderer::BeginRenderPass(m_CommandBuffer, m_SMAANeighborBlendPipeline->GetSpecification().RenderPass);
			Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_SMAANeighborBlendPipeline, m_UniformBufferSet, m_SMAANeighborBlendMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
			m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SMAANeighborBlendPassQuery);

		}
	}

	void SceneRenderer::FlushDrawList()
	{
		if (m_ResourcesCreated && m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{


			// Reset GPU time queries
			m_GPUTimeQueries = SceneRenderer::GPUTimeQueries();

			PreRender();

			m_CommandBuffer->Begin();

			// Main render passes
			ShadowMapPass();
			SpotShadowMapPass();
			PreDepthPass();
			HZBCompute();
			PreIntegration();
			LightCullingPass();
			RayMarchingPass();
			GeometryPass();

			// HBAO
			if (m_Options.EnableHBAO)
			{
				m_GPUTimeQueries.HBAOPassQuery = m_CommandBuffer->BeginTimestampQuery();
				DeinterleavingPass();
				HBAOCompute();
				ReinterleavingPass();
				HBAOBlurPass();
				m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.HBAOPassQuery);
			}
			// GTAO
			if (m_Options.EnableGTAO)
			{
				GTAOCompute();
				GTAODenoiseCompute();
			}

			if (m_Options.EnableGTAO || m_Options.EnableHBAO)
				AOComposite();

			PreConvolutionCompute();

			// Post-processing
			JumpFloodPass();

			//SSR
			if (m_Options.EnableSSR)
			{
				SSRCompute();
				SSRCompositePass();
			}
			if (m_Options.EnableAA)
			{
				if (IsUsingTAA(m_Options.AAMethod)) 
					TAAPass();
				if (IsUsingSMAA(m_Options.AAMethod))
					SMAAPass();
			}
			BloomCompute();
			CompositePass();

			m_CommandBuffer->End();
			m_CommandBuffer->Submit();
		}
		else
		{
			// Empty pass
			m_CommandBuffer->Begin();

			ClearPass();

			m_CommandBuffer->End();
			m_CommandBuffer->Submit();
		}

		UpdateStatistics();

		m_DrawList.clear();
		m_TransparentDrawList.clear();
		m_SelectedMeshDrawList.clear();
		m_ShadowPassDrawList.clear();

		m_StaticMeshDrawList.clear();
		m_TransparentStaticMeshDrawList.clear();
		m_SelectedStaticMeshDrawList.clear();
		m_StaticMeshShadowPassDrawList.clear();

		m_ColliderDrawList.clear();
		m_StaticColliderDrawList.clear();
		m_SceneData = {};

		
		//m_CurTransformMap->clear();
		//m_MeshBoneTransformsMap.clear();
	}

	void SceneRenderer::PreRender()
	{
		X2_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		uint32_t offset = 0;
		for (auto& [key, transformData] : *m_CurTransformMap)
		{
			
			transformData.TransformOffset = offset * sizeof(TransformVertexData);

			auto& prevTransformData = (*m_PrevTransformMap)[key];

			uint32_t transformIndex = 0;
			for (const auto& transform : transformData.Transforms)
			{
				m_SubmeshTransformBuffers[frameIndex].Data[offset] = transform;
				offset++;

				//Prev Frame Model Matrix; Used in Taa
				m_SubmeshTransformBuffers[frameIndex].Data[offset] = prevTransformData.Transforms[transformIndex];
				offset++;
			}

		}

		m_SubmeshTransformBuffers[frameIndex].Buffer->SetData(m_SubmeshTransformBuffers[frameIndex].Data, offset * sizeof(TransformVertexData));


		/*uint32_t index = 0;
		for (auto& [key, boneTransformsData] : m_MeshBoneTransformsMap)
		{
			boneTransformsData.BoneTransformsBaseIndex = index;
			for (const auto& boneTransforms : boneTransformsData.BoneTransformsData)
			{
				m_BoneTransformsData[index++] = boneTransforms;
			}
		}

		if (index > 0)
		{
			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, index]() mutable
				{
					instance->m_BoneTransformStorageBuffers[Renderer::RT_GetCurrentFrameIndex()]->RT_SetData(instance->m_BoneTransformsData, static_cast<uint32_t>(index * sizeof(BoneTransforms)));
				});
		}*/
	}

	/*void SceneRenderer::CopyToBoneTransformStorage(const MeshKey& meshKey, const Ref<MeshSource>& meshSource, const std::vector<glm::mat4>& boneTransforms)
	{
		auto& boneTransformStorage = m_MeshBoneTransformsMap[meshKey].BoneTransformsData.emplace_back();
		if (boneTransforms.empty())
		{
			boneTransformStorage.fill(glm::identity<glm::mat4>());
		}
		else
		{
			for (size_t i = 0; i < meshSource->m_BoneInfo.size(); ++i)
			{
				const auto submeshInvTransform = meshSource->m_BoneInfo[i].SubMeshInverseTransform;
				const auto boneTransform = boneTransforms[meshSource->m_BoneInfo[i].BoneIndex];
				const auto invBindPose = meshSource->m_BoneInfo[i].InverseBindPose;
				boneTransformStorage[i] = submeshInvTransform * boneTransform * invBindPose;
			}
		}
	}*/

	void SceneRenderer::ClearPass()
	{
		X2_PROFILE_FUNC();

		Renderer::BeginRenderPass(m_CommandBuffer, m_PreDepthPipeline->GetSpecification().RenderPass, true);
		Renderer::EndRenderPass(m_CommandBuffer);

		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePipeline->GetSpecification().RenderPass, true);
		Renderer::EndRenderPass(m_CommandBuffer);

		Renderer::BeginRenderPass(m_CommandBuffer, m_DOFPipeline->GetSpecification().RenderPass, true);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	Ref<VulkanPipeline> SceneRenderer::GetFinalPipeline()
	{
		return m_CompositePipeline;
	}

	Ref<VulkanRenderPass> SceneRenderer::GetFinalRenderPass()
	{
		return GetFinalPipeline()->GetSpecification().RenderPass;
	}

	Ref<VulkanImage2D> SceneRenderer::GetFinalPassImage()
	{
		if (!m_ResourcesCreated)
			return nullptr;
		//return m_GTAODebugOutputImage;
		return GetFinalPipeline()->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage();
		//return m_GeometryPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(3);
	}

	SceneRendererOptions& SceneRenderer::GetOptions()
	{
		return m_Options;
	}

	void SceneRenderer::CalculateCascades(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const
	{
		//TODO: Reversed Z projection?

		float scaleToOrigin = m_ScaleShadowCascadesToOrigin;

		glm::mat4 viewMatrix = sceneCamera.ViewMatrix;
		constexpr glm::vec4 origin = glm::vec4(glm::vec3(0.0f), 1.0f);
		viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

		auto viewProjection = sceneCamera.Camera.GetUnReversedProjectionMatrix() * viewMatrix;

		const int SHADOW_MAP_CASCADE_COUNT = 4;
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

		float nearClip = sceneCamera.Near;
		float farClip = sceneCamera.Far;
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
		{
			float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = CascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		cascadeSplits[3] = 0.3f;

		// Manually set cascades here
		// cascadeSplits[0] = 0.05f;
		// cascadeSplits[1] = 0.15f;
		// cascadeSplits[2] = 0.3f;
		// cascadeSplits[3] = 1.0f;

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
		{
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] =
			{
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(viewProjection);
			for (uint32_t i = 0; i < 8; i++)
			{
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++)
			{
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++)
				frustumCenter += frustumCorners[i];

			frustumCenter /= 8.0f;

			//frustumCenter *= 0.01f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++)
			{
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 up = std::abs(glm::dot(lightDirection, glm::vec3(0.f, 1.0f, 0.0f))) > 0.9 ? glm::normalize(glm::vec3(0.01f, 1.0f, 0.01f)) : glm::vec3(0.f, 1.0f, 0.0f);
			
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDirection * radius, frustumCenter, up);
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, - radius + CascadeNearPlaneOffset - CascadeFarPlaneOffset, 2 * radius);

			// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			float ShadowMapResolution = (float)m_ShadowPassPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetWidth();
			glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / ShadowMapResolution;
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;

			lightOrthoMatrix[3] += roundOffset;

			// Store split distance and matrix in cascade
			cascades[i].SplitDepth = (nearClip + splitDist * clipRange) * -1.0f;
			cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
			cascades[i].View = lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void SceneRenderer::CalculateCascadesManualSplit(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const
	{
		//TODO: Reversed Z projection?

		float scaleToOrigin = m_ScaleShadowCascadesToOrigin;

		glm::mat4 viewMatrix = sceneCamera.ViewMatrix;
		constexpr glm::vec4 origin = glm::vec4(glm::vec3(0.0f), 1.0f);
		viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

		auto viewProjection = sceneCamera.Camera.GetUnReversedProjectionMatrix() * viewMatrix;

		const int SHADOW_MAP_CASCADE_COUNT = 4;

		float nearClip = sceneCamera.Near;
		float farClip = sceneCamera.Far;
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
		{
			float splitDist = m_ShadowCascadeSplits[0];
			lastSplitDist = 0.0;

			glm::vec3 frustumCorners[8] =
			{
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(viewProjection);
			for (uint32_t i = 0; i < 8; i++)
			{
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++)
			{
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++)
				frustumCenter += frustumCorners[i];

			frustumCenter /= 8.0f;

			//frustumCenter *= 0.01f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++)
			{
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;
			radius *= m_ShadowCascadeSplits[1];

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 up = std::abs(glm::dot(lightDirection, glm::vec3(0.f, 1.0f, 0.0f))) > 0.9 ? glm::normalize(glm::vec3(0.01f, 1.0f, 0.01f)) : glm::vec3(0.f, 1.0f, 0.0f);
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDirection * radius, frustumCenter, up);
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, -radius + CascadeNearPlaneOffset - CascadeFarPlaneOffset, 2 * radius);

			// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			float ShadowMapResolution = (float)m_ShadowPassPipelines[0]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetWidth();
			glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / ShadowMapResolution;
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;

			lightOrthoMatrix[3] += roundOffset;

			// Store split distance and matrix in cascade
			cascades[i].SplitDepth = (nearClip + splitDist * clipRange) * -1.0f;
			cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
			cascades[i].View = lightViewMatrix;

			lastSplitDist = m_ShadowCascadeSplits[0];
		}
	}

	void SceneRenderer::UpdateStatistics()
	{
		m_Statistics.DrawCalls = 0;
		m_Statistics.Instances = 0;
		m_Statistics.Meshes = 0;

		for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		for (auto& [mk, dc] : m_StaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		for (auto& [mk, dc] : m_SelectedMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		for (auto& [mk, dc] : m_DrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		m_Statistics.SavedDraws = m_Statistics.Instances - m_Statistics.DrawCalls;

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_Statistics.TotalGPUTime = m_CommandBuffer->GetExecutionGPUTime(frameIndex);
	}

	void SceneRenderer::SetLineWidth(const float width)
	{
		m_LineWidth = width;
		if (m_GeometryWireframePipeline)
			m_GeometryWireframePipeline->GetSpecification().LineWidth = width;
	}

}
