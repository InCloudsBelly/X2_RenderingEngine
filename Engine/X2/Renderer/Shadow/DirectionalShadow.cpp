
#include "DirectionalShadow.h"
#include <glm/gtx/compatibility.hpp>

namespace X2
{

	DirectionalLightShadow::DirectionalLightShadow(uint32_t resolution)
		:m_resolution(resolution)
	{
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


		ImageSpecification spec;
		spec.Format = ImageFormat::DEPTH32F;
		spec.Type = ImageType::Image2DArray;
		spec.Usage = ImageUsage::Attachment;
		spec.Width = resolution;
		spec.Height = resolution;
		spec.Layers = CASCADED_COUNT;
		spec.DebugName = "ShadowCascades";
		Ref<VulkanImage2D> cascadedDepthImage = CreateRef<VulkanImage2D>(spec);
		cascadedDepthImage->Invalidate();
		cascadedDepthImage->CreatePerLayerImageViews();


		FramebufferSpecification shadowMapFramebufferSpec;
		shadowMapFramebufferSpec.DebugName = "Dir Shadow Map";
		shadowMapFramebufferSpec.Width = resolution;
		shadowMapFramebufferSpec.Height = resolution;
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


		// 4 cascades
		for (int i = 0; i < CASCADED_COUNT; i++)
		{
			shadowMapFramebufferSpec.ExistingImageLayers.clear();
			shadowMapFramebufferSpec.ExistingImageLayers.emplace_back(i);

			RenderPassSpecification shadowMapRenderPassSpec;
			shadowMapRenderPassSpec.DebugName = shadowMapFramebufferSpec.DebugName;
			shadowMapRenderPassSpec.TargetFramebuffer = CreateRef<VulkanFramebuffer>(shadowMapFramebufferSpec);
			auto renderpass = CreateRef<VulkanRenderPass>(shadowMapRenderPassSpec);

			pipelineSpec.RenderPass = renderpass;
			m_ShadowPassPipelines[i] = CreateRef<VulkanPipeline>(pipelineSpec);
		}

		m_ShadowPassMaterial = CreateRef<VulkanMaterial>(shadowPassShader, "DirShadowPass");
	}


	void DirectionalLightShadow::Update(const glm::vec3 lightDirection, const SceneRendererCamera& camera, float splitLambda, float nearOffset, float farOffset)
	{


		float scaleToOrigin = 0.0;

		glm::mat4 viewMatrix = camera.ViewMatrix;
		constexpr glm::vec4 origin = glm::vec4(glm::vec3(0.0f), 1.0f);
		viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

		auto viewProjection = camera.Camera.GetUnReversedProjectionMatrix() * viewMatrix;

		const int SHADOW_MAP_CASCADE_COUNT = 4;
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

		float nearClip = camera.Near;
		float farClip = camera.Far;
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
			float d = splitLambda * (log - uniform) + uniform;
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
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, -radius + nearOffset - farOffset, 2 * radius);

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
			m_data.CascadeSplits[i] = (nearClip + splitDist * clipRange) * -1.0f;
			m_data.ViewProjection[i] = lightOrthoMatrix * lightViewMatrix;
			//cascades[i].View = lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}





	void DirectionalLightShadow::RenderStaticShadow(Ref<VulkanRenderCommandBuffer> cb, Ref<VulkanUniformBufferSet> uniformBufferSet, std::map<MeshKey, StaticDrawCommand>& staticDrawList, std::map<MeshKey, TransformMapData>* curTransformMap, Ref<VulkanVertexBuffer> buffer)
	{
		for (int i = 0; i < CASCADED_COUNT; i++)
		{
			Renderer::BeginRenderPass(cb, m_ShadowPassPipelines[i]->GetSpecification().RenderPass);

			// static glm::mat4 scaleBiasMatrix = glm::scale(glm::mat4(1.0f), { 0.5f, 0.5f, 0.5f }) * glm::translate(glm::mat4(1.0f), { 1, 1, 1 });

			// Render entities
			const Buffer cascade(&i, sizeof(uint32_t));
			for (auto& [mk, dc] : staticDrawList)
			{
				X2_CORE_VERIFY(curTransformMap->find(mk) != curTransformMap->end());
				const auto& transformData = curTransformMap->at(mk);
				Renderer::RenderStaticMeshWithMaterial(cb, m_ShadowPassPipelines[i], uniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, buffer, transformData.TransformOffset, dc.InstanceCount, m_ShadowPassMaterial, cascade);
			}

			Renderer::EndRenderPass(cb);
		}
	}


};
