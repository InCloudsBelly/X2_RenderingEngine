#pragma once

#include "X2/Scene/Scene.h"
#include "X2/Scene/Components.h"


#include "Mesh.h"
#include "ShaderDefs.h"

#include "X2/Vulkan/VulkanRenderPass.h"
#include "X2/Vulkan/VulkanMaterial.h"
#include "X2/Vulkan/VulkanUniformBufferSet.h"
#include "X2/Vulkan/VulkanRenderCommandBuffer.h"
#include "X2/Vulkan/VulkanComputePipeline.h"
#include "X2/Vulkan/VulkanStorageBufferSet.h"

#include "X2/Project/TieringSettings.h"

#include "DebugRenderer.h"

namespace X2
{

	struct SceneRendererOptions
	{
		bool ShowGrid = true;
		bool ShowSelectedInWireframe = false;

		enum class PhysicsColliderView
		{
			SelectedEntity = 0, All = 1
		};

		bool ShowPhysicsColliders = false;
		PhysicsColliderView PhysicsColliderMode = PhysicsColliderView::SelectedEntity;
		bool ShowPhysicsCollidersOnTop = false;
		glm::vec4 SimplePhysicsCollidersColor = glm::vec4{ 0.2f, 1.0f, 0.2f, 1.0f };
		glm::vec4 ComplexPhysicsCollidersColor = glm::vec4{ 0.5f, 0.5f, 1.0f, 1.0f };

		// General AO
		float AOShadowTolerance = 0.15f;

		// HBAO
		bool EnableHBAO = false;
		float HBAOIntensity = 1.5f;
		float HBAORadius = 0.275f;
		float HBAOBias = 0.6f;
		float HBAOBlurSharpness = 1.0f;

		// GTAO
		bool EnableGTAO = true;
		bool GTAOBentNormals = false;
		int GTAODenoisePasses = 4;

		// SSR
		bool EnableSSR = false;
		ShaderDef::AOMethod ReflectionOcclusionMethod = ShaderDef::AOMethod::None;

		bool EnableAA = true;
		ShaderDef::AAMethod AAMethod = ShaderDef::AAMethod::SMAA;
		ShaderDef::SMAAEdgeMethod SMAAEdgeMethod = ShaderDef::SMAAEdgeMethod::Color;
		ShaderDef::SMAAQuality SMAAQuality = ShaderDef::SMAAQuality::Ultra;

		//TAA
		float TAAFeedback = 0.1f;

		// Froxel Volume Fog & light
		uint32_t VOXEL_GRID_SIZE_X = 160;
		uint32_t VOXEL_GRID_SIZE_Y = 90;
		uint32_t VOXEL_GRID_SIZE_Z = 128;

		uint32_t NUM_BLUE_NOISE_TEXTURES = 8;
	
		float froxelFogAnisotropy = - 0.3f;
		float froxelFogDensity = 0.5f;
		float froxelFogLightMul = 2.5;

		bool froxelFogEnableJitter = true;
		bool froxelFogEnableTemperalAccumulating = true;
		bool froxelFogFastJitter = false;

		float heightFogExponent = 0.0f;
		float heightFogOffset = 0.0f ;
		float heightFogAmount = 0.0f;


	};

	struct SSROptionsUB
	{
		//SSR
		glm::vec2 HZBUvFactor;
		glm::vec2 FadeIn = { 0.1f, 0.15f };
		float Brightness = 0.7f;
		float DepthTolerance = 0.8f;
		float FacingReflectionsFading = 0.1f;
		int MaxSteps = 70;
		uint32_t NumDepthMips;
		float RoughnessDepthTolerance = 1.0f;
		bool HalfRes = true;
		char Padding[3]{ 0, 0, 0 };
		bool EnableConeTracing = true;
		char Padding1[3]{ 0, 0, 0 };
		float LuminanceFactor = 1.0f;
	};

	struct SceneRendererCamera
	{
		X2::Camera Camera;
		glm::mat4 ViewMatrix;
		float Near, Far; //Non-reversed
		float FOV;
	};

	struct BloomSettings
	{
		bool Enabled = true;
		float Threshold = 8.0f;
		float Knee = 0.1f;
		float UpsampleScale = 1.0f;
		float Intensity = 0.5f;
		float DirtIntensity = 1.0f;
	};

	struct DOFSettings
	{
		bool Enabled = false;
		float FocusDistance = 0.0f;
		float BlurSize = 1.0f;
	};

	struct SMAAParameters {
		float threshold;
		float depthThreshold;
		uint32_t  maxSearchSteps;
		uint32_t  maxSearchStepsDiag;

		uint32_t  cornerRounding;
		uint32_t  pad0;
		uint32_t  pad1;
		uint32_t  pad2;
	};


	struct SceneRendererSpecification
	{
		Tiering::Renderer::RendererTieringSettings Tiering;
	};

	class SceneRenderer 
	{
	public:
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Instances = 0;
			uint32_t SavedDraws = 0;

			float TotalGPUTime = 0.0f;
		};
	public:
		SceneRenderer(Ref<Scene> scene, SceneRendererSpecification specification = SceneRendererSpecification());
		virtual ~SceneRenderer();

		void Init();
		void InitMaterials();

		void Shutdown();

		// Should only be called at initialization.
		void InitOptions();

		void SetScene(Scene* scene);

		void SetViewportSize(uint32_t width, uint32_t height);

		void UpdateHBAOData();
		void UpdateGTAOData();

		void BeginScene(const SceneRendererCamera& camera);
		void EndScene();

		static void InsertGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {});
		static void BeginGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {});
		static void EndGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer);

		void SubmitMesh(uint64_t entityUUID, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), const std::vector<glm::mat4>& boneTransforms = {}, Ref<VulkanMaterial> overrideMaterial = nullptr);
		void SubmitStaticMesh(uint64_t entityUUID, Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<VulkanMaterial> overrideMaterial = nullptr);

		void SubmitSelectedMesh(uint64_t entityUUID, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), const std::vector<glm::mat4>& boneTransforms = {}, Ref<VulkanMaterial> overrideMaterial = nullptr);
		void SubmitSelectedStaticMesh(uint64_t entityUUID, Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<VulkanMaterial> overrideMaterial = nullptr);

		void SubmitPhysicsDebugMesh(uint64_t entityUUID, Ref<Mesh> mesh, uint32_t submeshIndex, const glm::mat4& transform = glm::mat4(1.0f));
		void SubmitPhysicsStaticDebugMesh(uint64_t entityUUID, Ref<StaticMesh> mesh, const glm::mat4& transform = glm::mat4(1.0f), const bool isPrimitiveCollider = true);

		Ref<VulkanPipeline> GetFinalPipeline();
		Ref<VulkanRenderPass> GetFinalRenderPass();
		Ref<VulkanRenderPass> GetCompositeRenderPass() { return m_CompositePipeline->GetSpecification().RenderPass; }
		Ref<VulkanRenderPass> GetExternalCompositeRenderPass() { return m_ExternalCompositeRenderPass; }
		Ref<VulkanImage2D> GetFinalPassImage();

		Ref<Renderer2D> GetRenderer2D() { return m_Renderer2D; }
		Ref<DebugRenderer> GetDebugRenderer() { return m_DebugRenderer; }

		SceneRendererOptions& GetOptions();

		void SetShadowSettings(float nearPlane, float farPlane, float lambda, float scaleShadowToOrigin = 0.0f)
		{
			CascadeNearPlaneOffset = nearPlane;
			CascadeFarPlaneOffset = farPlane;
			CascadeSplitLambda = lambda;
			m_ScaleShadowCascadesToOrigin = scaleShadowToOrigin;
		}

		void SetShadowCascades(float a, float b, float c, float d)
		{
			m_UseManualCascadeSplits = true;
			m_ShadowCascadeSplits[0] = a;
			m_ShadowCascadeSplits[1] = b;
			m_ShadowCascadeSplits[2] = c;
			m_ShadowCascadeSplits[3] = d;
		}

		BloomSettings& GetBloomSettings() { return m_BloomSettings; }
		DOFSettings& GetDOFSettings() { return m_DOFSettings; }

		void SetLineWidth(float width);

		static void WaitForThreads();

		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		float GetOpacity() const { return m_Opacity; }
		void SetOpacity(float opacity) { m_Opacity = opacity; }

		const Statistics& GetStatistics() const { return m_Statistics; }
	private:
		void FlushDrawList();

		void PreRender();

		struct MeshKey
		{
			uint64_t EntityUUID;
			AssetHandle MeshHandle;
			AssetHandle MaterialHandle;
			uint32_t SubmeshIndex;
			bool IsSelected;

			MeshKey(uint64_t entityUUID, AssetHandle meshHandle, AssetHandle materialHandle, uint32_t submeshIndex, bool isSelected)
				:EntityUUID(entityUUID), MeshHandle(meshHandle), MaterialHandle(materialHandle), SubmeshIndex(submeshIndex), IsSelected(isSelected)
			{
			}

			bool operator<(const MeshKey& other) const
			{
				if (EntityUUID < other.EntityUUID)
					return true;

				if (EntityUUID > other.EntityUUID)
					return false;

				if (MeshHandle < other.MeshHandle)
					return true;

				if (MeshHandle > other.MeshHandle)
					return false;

				if (SubmeshIndex < other.SubmeshIndex)
					return true;

				if (SubmeshIndex > other.SubmeshIndex)
					return false;

				if (MaterialHandle < other.MaterialHandle)
					return true;

				if (MaterialHandle > other.MaterialHandle)
					return false;

				return IsSelected < other.IsSelected;

			}
		};

		//void CopyToBoneTransformStorage(const MeshKey& meshKey, const Ref<MeshSource>& meshSource, const std::vector<glm::mat4>& boneTransforms);

		void ClearPass();
		void ClearPass(Ref<VulkanRenderPass> renderPass, bool explicitClear = false);
		void DeinterleavingPass();
		void HBAOCompute();
		void GTAOCompute();

		void AOComposite();

		void GTAODenoiseCompute();

		void ReinterleavingPass();
		void HBAOBlurPass();
		void ShadowMapPass();
		void SpotShadowMapPass();
		void PreDepthPass();
		void HZBCompute();
		void PreIntegration();
		void LightCullingPass();
		void FroxelFogPass();
		void GeometryPass();
		void PreConvolutionCompute();
		void JumpFloodPass();

		// Post-processing
		void BloomCompute();
		void SSRCompute();
		void SSRCompositePass();
		void EdgeDetectionPass();
		void CompositePass();
		void SMAAPass();
		void TAAPass();


		struct CascadeData
		{
			glm::mat4 ViewProj;
			glm::mat4 View;
			float SplitDepth;
		};
		void CalculateCascades(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const;
		void CalculateCascadesManualSplit(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const;

		void UpdateStatistics();
	private:
		Scene* m_Scene;
		SceneRendererSpecification m_Specification;
		Ref<VulkanRenderCommandBuffer> m_CommandBuffer;

		Ref<Renderer2D> m_Renderer2D;
		Ref<DebugRenderer> m_DebugRenderer;

		struct SceneInfo
		{
			SceneRendererCamera SceneCamera;

			// Resources
			Ref<Environment> SceneEnvironment;
			float SkyboxLod = 0.0f;
			float SceneEnvironmentIntensity;
			LightEnvironment SceneLightEnvironment;
			DirLight ActiveLight;
		} m_SceneData;

		struct UBCamera
		{
			// projection with near and far inverted
			glm::mat4 ViewProjection;
			glm::mat4 InverseViewProjection;
			glm::mat4 Projection;
			glm::mat4 InverseProjection;
			glm::mat4 View;
			glm::mat4 InverseView;
			glm::vec2 NDCToViewMul;
			glm::vec2 NDCToViewAdd;
			glm::vec2 DepthUnpackConsts;
			glm::vec2 CameraTanHalfFOV;
		} CameraDataUB;

		struct UBHBAOData
		{
			glm::vec4 PerspectiveInfo;
			glm::vec2 InvQuarterResolution;
			float RadiusToScreen;
			float NegInvR2;

			float NDotVBias;
			float AOMultiplier;
			float PowExponent;
			bool IsOrtho;
			char Padding0[3]{ 0, 0, 0 };

			glm::vec4 Float2Offsets[16];
			glm::vec4 Jitters[16];

			glm::vec3 Padding;
			float ShadowTolerance;
		} HBAODataUB;

		struct CBGTAOData
		{
			glm::vec2 NDCToViewMul_x_PixelSize;
			float EffectRadius = 0.2f;
			float EffectFalloffRange = 0.3f;

			float RadiusMultiplier = 1.5f;
			float FinalValuePower = 2.5f;
			float DenoiseBlurBeta = 1.2f;
			bool HalfRes = false;
			char Padding0[3]{ 0, 0, 0 };

			float SampleDistributionPower = 2.0f;
			float ThinOccluderCompensation = 0.0f;
			float DepthMIPSamplingOffset = 3.3f;
			int NoiseIndex = 0;

			glm::vec2 HZBUVFactor;
			float ShadowTolerance;
			float Padding;
		} GTAODataCB;

		struct UBScreenData
		{
			glm::vec2 InvFullResolution;
			glm::vec2 FullResolution;
			glm::vec2 InvHalfResolution;
			glm::vec2 HalfResolution;
		} ScreenDataUB;

		struct UBShadow
		{
			glm::mat4 ViewProjection[4];
		} ShadowData;

		struct DirLight
		{
			glm::vec3 Direction;
			float ShadowAmount;
			glm::vec3 Radiance;
			float Intensity;
		};

		struct UBPointLights
		{
			uint32_t Count{ 0 };
			glm::vec3 Padding{};
			PointLight PointLights[1024]{};
		} PointLightsUB;

		struct UBSpotLights
		{
			uint32_t Count{ 0 };
			glm::vec3 Padding{};
			SpotLight SpotLights[1000]{};
		} SpotLightUB;

		struct UBSpotShadowData
		{
			glm::mat4 ShadowMatrices[1000]{};
		} SpotShadowDataUB;

		struct UBScene
		{
			DirLight Lights;
			glm::vec3 CameraPosition;
			float EnvironmentMapIntensity = 1.0f;
		} SceneDataUB;

		struct UBRendererData
		{
			glm::vec4 CascadeSplits;
			uint32_t TilesCountX{ 0 };
			bool ShowCascades = false;
			char Padding0[3] = { 0,0,0 }; // Bools are 4-bytes in GLSL
			bool SoftShadows = true;
			char Padding1[3] = { 0,0,0 };
			float Range = 0.5f;
			float MaxShadowDistance = 200.0f;
			float ShadowFade = 1.0f;
			bool CascadeFading = true;
			char Padding2[3] = { 0,0,0 };
			float CascadeTransitionFade = 1.0f;
			bool ShowLightComplexity = false;
			char Padding3[3] = { 0,0,0 };
		} RendererDataUB;

		struct UBSMAAData
		{
			SMAAParameters  smaaParameters;

			glm::vec4 subsampleIndices;

			float predicationThreshold;
			float predicationScale;
			float predicationStrength;
			float reprojWeigthScale;

		} SMAADataUB;

		struct UBTAAData
		{
			glm::mat4 ViewProjectionHistory;
			glm::mat4 JitterdViewProjection;
			glm::mat4 InvJitterdProjection;

			glm::vec2 ScreenSpaceJitter;
			float FeedBack;

		} TAADataUB;

		struct UBFroxelFogData
		{
			glm::vec4 bias_near_far_pow;
			glm::vec4 aniso_density_multipler_absorption;
			glm::vec4 frustumRays[4];

			glm::vec4 windDir_Speed{ 0.0f };
			glm::vec4 fogParams{0.0f}; //constant , Height Fog Exponent, Height Fog Offset, Height Fog Amount

			bool enableJitter = true;
			char Padding0[3]{ 0, 0, 0 };
			bool enableTemperalAccumulating = true;
			char Padding1[3]{ 0, 0, 0 };


		} FroxelFogDataUB;

		struct UBFogVolumesData {

			uint32_t FogVolumeCount = { 0 };
			glm::vec3 Padding{};
			FogVolume boxFogVolumes[100];

		} FogVolumes;

		// GTAO
		Ref<VulkanImage2D> m_GTAOOutputImage;
		Ref<VulkanImage2D> m_GTAODenoiseImage;
		// Points to m_GTAOOutputImage or m_GTAODenoiseImage!
		Ref<VulkanImage2D> m_GTAOFinalImage; //TODO: Weak!
		Ref<VulkanImage2D> m_GTAOEdgesOutputImage;
		Ref<VulkanImage2D> m_GTAODebugOutputImage;
		Ref<VulkanComputePipeline> m_GTAOPipeline;
		Ref<VulkanMaterial> m_GTAOMaterial;
		glm::uvec3 m_GTAOWorkGroups{ 1 };
		Ref<VulkanComputePipeline> m_GTAODenoisePipeline;
		Ref<VulkanMaterial> m_GTAODenoiseMaterial[2]; //Ping, Pong
		Ref<VulkanMaterial> m_AOCompositeMaterial;
		glm::uvec3 m_GTAODenoiseWorkGroups{ 1 };
		Ref<VulkanPipeline> m_AOCompositePipeline;
		Ref<VulkanRenderPass> m_AOCompositeRenderPass;


		//HBAO
		glm::uvec3 m_HBAOWorkGroups{ 1 };
		Ref<VulkanMaterial> m_DeinterleavingMaterial;
		Ref<VulkanMaterial> m_ReinterleavingMaterial;
		Ref<VulkanMaterial> m_HBAOBlurMaterials[2];
		Ref<VulkanMaterial> m_HBAOMaterial;
		Ref<VulkanPipeline> m_DeinterleavingPipelines[2];
		Ref<VulkanPipeline> m_ReinterleavingPipeline;
		Ref<VulkanPipeline> m_HBAOBlurPipelines[2];

		Ref<VulkanComputePipeline> m_HBAOPipeline;
		Ref<VulkanImage2D> m_HBAOOutputImage;

		Ref<VulkanShader> m_CompositeShader;


		// Shadows
		Ref<VulkanPipeline> m_SpotShadowPassPipeline;
		Ref<VulkanPipeline> m_SpotShadowPassAnimPipeline;
		Ref<VulkanMaterial> m_SpotShadowPassMaterial;

		glm::uvec3 m_LightCullingWorkGroups;

		Ref<VulkanUniformBufferSet> m_UniformBufferSet;
		Ref<VulkanStorageBufferSet> m_StorageBufferSet;

		float LightDistance = 0.1f;
		float CascadeSplitLambda = 0.35f;
		glm::vec4 CascadeSplits;
		float CascadeFarPlaneOffset = 20.0f, CascadeNearPlaneOffset = -20.0f;
		float m_ScaleShadowCascadesToOrigin = 0.0f;
		float m_ShadowCascadeSplits[4];
		bool m_UseManualCascadeSplits = false;

		//SSR
		Ref<VulkanPipeline> m_SSRCompositePipeline;
		Ref<VulkanComputePipeline> m_SSRPipeline;
		Ref<VulkanComputePipeline> m_HierarchicalDepthPipeline;
		Ref<VulkanComputePipeline> m_GaussianBlurPipeline;
		Ref<VulkanComputePipeline> m_PreIntegrationPipeline;
		Ref<VulkanComputePipeline> m_SSRUpscalePipeline;
		Ref<VulkanImage2D> m_SSRImage;
		Ref<VulkanTexture2D> m_HierarchicalDepthTexture;
		Ref<VulkanTexture2D> m_PreConvolutedTexture;
		Ref<VulkanTexture2D> m_VisibilityTexture;
		Ref<VulkanMaterial> m_SSRCompositeMaterial;
		Ref<VulkanMaterial> m_SSRMaterial;
		Ref<VulkanMaterial> m_GaussianBlurMaterial;
		Ref<VulkanMaterial> m_HierarchicalDepthMaterial;
		Ref<VulkanMaterial> m_PreIntegrationMaterial;
		glm::uvec3 m_SSRWorkGroups{ 1 };


		//SMAA
		Ref<VulkanPipeline> m_SMAAEdgeDetectionPipeline;
		Ref<VulkanMaterial> m_SMAAEdgeDetectionMaterial;
		Ref<VulkanPipeline> m_SMAABlendWeightPipeline;
		Ref<VulkanMaterial> m_SMAABlendWeightMaterial;
		Ref<VulkanPipeline> m_SMAANeighborBlendPipeline;
		Ref<VulkanMaterial> m_SMAANeighborBlendMaterial;

		//TAA
		Ref<VulkanImage2D> m_TAAToneMappedPreColorImage;

		Ref<VulkanImage2D> m_TAAVelocityImage = nullptr;
		Ref<VulkanPipeline> m_TAAPipeline;//current & history;
		Ref<VulkanMaterial> m_TAAMaterial;
		Ref<VulkanPipeline> m_TAAToneMappingPipeline;
		Ref<VulkanMaterial> m_TAAToneMappingMaterial;
		Ref<VulkanPipeline> m_TAAToneUnMappingPipeline;
		Ref<VulkanMaterial> m_TAAToneUnMappingMaterial;


		uint32_t m_HaltonJitterCounter = 0;
		glm::vec2 m_TAAHaltonSequence[8] =
		{
			 glm::vec2(0.5f, 1.0f / 3),
			 glm::vec2(0.25f, 2.0f / 3),
			 glm::vec2(0.75f, 1.0f / 9),
			 glm::vec2(0.125f, 4.0f / 9),
			 glm::vec2(0.625f, 7.0f / 9),
			 glm::vec2(0.375f, 2.0f / 9),
			 glm::vec2(0.875f, 5.0f / 9),
			 glm::vec2(0.0625f, 8.0f / 9),
		};

		//Volume Fog & Light
		Ref<VulkanComputePipeline> m_FroxelFog_RayInjectionPipeline;
		Ref<VulkanMaterial> m_FroxelFog_RayInjectionMaterial[2];
		glm::uvec3 m_FroxelFog_RayInjectionWorkGroups{ 1 };
		Ref<VulkanImage2D> m_FroxelFog_lightInjectionImage[2];//history, current
		std::vector<Ref<VulkanTexture2D>> m_BlueNoiseTextures;
		//Scattering
		Ref<VulkanComputePipeline> m_FroxelFog_ScatteringPipeline;
		Ref<VulkanMaterial> m_FroxelFog_ScatteringMaterial[2];
		glm::uvec3 m_FroxelFog_ScatteringWorkGroups{ 1 };
		Ref<VulkanImage2D> m_FroxelFog_ScatteringImage;
		//composite
		Ref<VulkanPipeline> m_FroxelFog_CompositePipeline;
		Ref<VulkanMaterial> m_FroxelFog_CompositeMaterial;
		Ref<VulkanImage2D> m_FroxelFog_ColorTempImage;




		glm::vec2 FocusPoint = { 0.5f, 0.5f };

		Ref<VulkanMaterial> m_CompositeMaterial;
		Ref<VulkanMaterial> m_LightCullingMaterial;

		Ref<VulkanPipeline> m_GeometryPipeline;
		Ref<VulkanPipeline> m_GeometryTAAPipeline;
		Ref<VulkanPipeline> m_TransparentGeometryPipeline;
		Ref<VulkanPipeline> m_GeometryPipelineAnim;

		Ref<VulkanPipeline> m_SelectedGeometryPipeline;
		Ref<VulkanPipeline> m_SelectedGeometryPipelineAnim;
		Ref<VulkanMaterial> m_SelectedGeometryMaterial;
		Ref<VulkanMaterial> m_SelectedGeometryMaterialAnim;

		Ref<VulkanPipeline> m_GeometryWireframePipeline;
		Ref<VulkanPipeline> m_GeometryWireframePipelineAnim;
		Ref<VulkanPipeline> m_GeometryWireframeOnTopPipeline;
		Ref<VulkanPipeline> m_GeometryWireframeOnTopPipelineAnim;
		Ref<VulkanMaterial> m_WireframeMaterial;

		Ref<VulkanPipeline> m_PreDepthPipeline;
		Ref<VulkanPipeline> m_PreDepthTAAPipeline;
		Ref<VulkanPipeline> m_PreDepthTransparentPipeline;
		Ref<VulkanPipeline> m_PreDepthPipelineAnim;
		Ref<VulkanMaterial> m_PreDepthMaterial;
		Ref<VulkanMaterial> m_PreDepthTAAMaterial;

		Ref<VulkanPipeline> m_CompositePipeline;

		Ref<VulkanPipeline> m_ShadowPassPipelines[4];
		Ref<VulkanPipeline> m_ShadowPassPipelinesAnim[4];

		Ref<VulkanPipeline> m_EdgeDetectionPipeline;
		Ref<VulkanMaterial> m_EdgeDetectionMaterial;

		Ref<VulkanMaterial> m_ShadowPassMaterial;

		Ref<VulkanPipeline> m_SkyboxPipeline;
		Ref<VulkanMaterial> m_SkyboxMaterial;

		Ref<VulkanPipeline> m_DOFPipeline;
		Ref<VulkanMaterial> m_DOFMaterial;

		Ref<VulkanComputePipeline> m_LightCullingPipeline;

		// Jump Flood Pass
		Ref<VulkanPipeline> m_JumpFloodInitPipeline;
		Ref<VulkanPipeline> m_JumpFloodPassPipeline[2];
		Ref<VulkanPipeline> m_JumpFloodCompositePipeline;
		Ref<VulkanMaterial> m_JumpFloodInitMaterial, m_JumpFloodPassMaterial[2];
		Ref<VulkanMaterial> m_JumpFloodCompositeMaterial;

		// Bloom compute
		uint32_t m_BloomComputeWorkgroupSize = 4;
		Ref<VulkanComputePipeline> m_BloomComputePipeline;
		std::vector<Ref<VulkanTexture2D>> m_BloomComputeTextures{ 3 };
		Ref<VulkanMaterial> m_BloomComputeMaterial;

		struct TransformVertexData
		{
			glm::vec4 MRow[3];
		};

		struct TransformBuffer
		{
			Ref<VulkanVertexBuffer> Buffer;
			TransformVertexData* Data = nullptr;
		};

		std::vector<TransformBuffer> m_SubmeshTransformBuffers;
		
		//using BoneTransforms = std::array<glm::mat4, 100>; // Note: 100 == MAX_BONES from the shaders
		//std::vector<Ref<VulkanStorageBuffer>> m_BoneTransformStorageBuffers;
		//BoneTransforms* m_BoneTransformsData = nullptr;

		std::vector<Ref<VulkanFramebuffer>> m_TempFramebuffers;

		Ref<VulkanRenderPass> m_ExternalCompositeRenderPass;

		struct DrawCommand
		{
			Ref<Mesh> Mesh;
			uint32_t SubmeshIndex;
			Ref<MaterialTable> MaterialTable;
			Ref<VulkanMaterial> OverrideMaterial;

			uint32_t InstanceCount = 0;
			uint32_t InstanceOffset = 0;
			bool IsRigged = false;
		};

		struct StaticDrawCommand
		{
			Ref<StaticMesh> StaticMesh;
			uint32_t SubmeshIndex;
			Ref<MaterialTable> MaterialTable;
			Ref<VulkanMaterial> OverrideMaterial;

			uint32_t InstanceCount = 0;
			uint32_t InstanceOffset = 0;
		};

		struct TransformMapData
		{
			std::vector<TransformVertexData> Transforms;
			uint32_t TransformOffset = 0;
		};

		//struct BoneTransformsMapData
		//{
		//	std::vector<BoneTransforms> BoneTransformsData;
		//	uint32_t BoneTransformsBaseIndex = 0;
		//};

		std::map<MeshKey, TransformMapData> m_MeshTransformMap[2];
		std::map<MeshKey, TransformMapData> * m_CurTransformMap;
		std::map<MeshKey, TransformMapData> * m_PrevTransformMap;


		//std::map<MeshKey, BoneTransformsMapData> m_MeshBoneTransformsMap;

		//std::vector<DrawCommand> m_DrawList;
		std::map<MeshKey, DrawCommand> m_DrawList;
		std::map<MeshKey, DrawCommand> m_TransparentDrawList;
		std::map<MeshKey, DrawCommand> m_SelectedMeshDrawList;
		std::map<MeshKey, DrawCommand> m_ShadowPassDrawList;

		std::map<MeshKey, StaticDrawCommand> m_StaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_TransparentStaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_SelectedStaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_StaticMeshShadowPassDrawList;

		// Debug
		std::map<MeshKey, StaticDrawCommand> m_StaticColliderDrawList;
		std::map<MeshKey, DrawCommand> m_ColliderDrawList;

		// Grid
		Ref<VulkanPipeline> m_GridPipeline;
		Ref<VulkanMaterial> m_GridMaterial;

		Ref<VulkanMaterial> m_OutlineMaterial;
		Ref<VulkanMaterial> m_SimpleColliderMaterial;
		Ref<VulkanMaterial> m_ComplexColliderMaterial;

		SceneRendererOptions m_Options;
		SSROptionsUB m_SSROptions;

		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
		float m_InvViewportWidth = 0.f, m_InvViewportHeight = 0.f;
		bool m_NeedsResize = false;
		bool m_Active = false;
		bool m_ResourcesCreatedGPU = false;
		bool m_ResourcesCreated = false;

		float m_LineWidth = 2.0f;

		BloomSettings m_BloomSettings;
		DOFSettings m_DOFSettings;
		Ref<VulkanTexture2D> m_BloomDirtTexture;

		Ref<VulkanImage2D> m_ReadBackImage;
		glm::vec4* m_ReadBackBuffer = nullptr;

		float m_Opacity = 1.0f;

		struct GPUTimeQueries
		{
			uint32_t DirShadowMapPassQuery = -1;
			uint32_t SpotShadowMapPassQuery = -1;
			uint32_t DepthPrePassQuery = -1;
			uint32_t HierarchicalDepthQuery = -1;
			uint32_t PreIntegrationQuery = -1;
			uint32_t LightCullingPassQuery = -1;
			uint32_t GeometryPassQuery = -1;
			uint32_t PreConvolutionQuery = -1;
			uint32_t HBAOPassQuery = -1;
			uint32_t GTAOPassQuery = -1;
			uint32_t GTAODenoisePassQuery = -1;
			uint32_t AOCompositePassQuery = -1;
			uint32_t SSRQuery = -1;
			uint32_t SSRCompositeQuery = -1;
			uint32_t BloomComputePassQuery = -1;
			uint32_t SMAAEdgeDetectPassQuery = -1;
			uint32_t SMAABlendWeightPassQuery = -1;
			uint32_t SMAANeighborBlendPassQuery = -1;
			uint32_t TAAQuery = -1;
			uint32_t JumpFloodPassQuery = -1;
			uint32_t CompositePassQuery = -1;
			uint32_t FroxelFogQuery = -1;

		} m_GPUTimeQueries;

		Statistics m_Statistics;

		friend class SceneRendererPanel;
	};

}
