
#include "SpotLightShadow.h"
#include <glm/gtx/compatibility.hpp>

namespace X2
{

	SpotLightShadow::SpotLightShadow(uint32_t resolution)
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
		spec.Layers = MAX_SPOT_LIGHT_SHADOW_COUNT;
		spec.DebugName = "Spot Shadows";
		Ref<VulkanImage2D> cascadedDepthImage = CreateRef<VulkanImage2D>(spec);
		cascadedDepthImage->Invalidate();
		cascadedDepthImage->CreatePerLayerImageViews();


		FramebufferSpecification shadowMapFramebufferSpec;
		shadowMapFramebufferSpec.DebugName = "Spot Shadow Map";
		shadowMapFramebufferSpec.Width = resolution;
		shadowMapFramebufferSpec.Height = resolution;
		shadowMapFramebufferSpec.Attachments = { ImageFormat::DEPTH32F };
		shadowMapFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		shadowMapFramebufferSpec.DepthClearValue = 1.0f;
		shadowMapFramebufferSpec.NoResize = true;
		shadowMapFramebufferSpec.ExistingImage = cascadedDepthImage;

		auto shadowPassShader = Renderer::GetShaderLibrary()->Get("SpotShadowMap");
		auto shadowPassShaderAnim = Renderer::GetShaderLibrary()->Get("SpotShadowMap_Anim");


		PipelineSpecification pipelineSpec;
		pipelineSpec.DebugName = "SpotShadowPass";
		pipelineSpec.Shader = shadowPassShader;
		pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;
		pipelineSpec.Layout = vertexLayout;
		pipelineSpec.InstanceLayout = instanceLayout;


		// 15 shadows
		for (int i = 0; i < MAX_SPOT_LIGHT_SHADOW_COUNT; i++)
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
		m_ShadowPassMaterial = CreateRef<VulkanMaterial>(shadowPassShader, "SpotShadowPass");
	}


	void SpotLightShadow::Update(const std::vector<SpotLightInfo>& spotLightInfos)
	{
		X2_ASSERT(spotLightInfos.size() <= MAX_SPOT_LIGHT_SHADOW_COUNT, "Now Only Supports max 15 spotLight Shadow");
		m_activeSpotLightCount = spotLightInfos.size();
		for (size_t i = 0; i < spotLightInfos.size(); ++i)
		{
			auto& light = spotLightInfos[i];
			if (!light.CastsShadows)
				continue;

			glm::mat4 projection = glm::perspective(glm::radians(light.Angle), 1.f, 0.1f, light.Range);

			m_data.ViewProjection[i] = projection * glm::lookAt(light.Position, light.Position - light.Direction, glm::vec3(0.0f, 1.0f, 0.0f));
		}
	}



	void SpotLightShadow::RenderStaticShadow(Ref<VulkanRenderCommandBuffer> cb, Ref<VulkanUniformBufferSet> uniformBufferSet, std::map<MeshKey, StaticDrawCommand>& staticDrawList, std::map<MeshKey, TransformMapData>* curTransformMap, Ref<VulkanVertexBuffer> buffer)
	{

		for (int i = 0; i < m_activeSpotLightCount; i++)
		{
			Renderer::BeginRenderPass(cb, m_ShadowPassPipelines[i]->GetSpecification().RenderPass);

			// static glm::mat4 scaleBiasMatrix = glm::scale(glm::mat4(1.0f), { 0.5f, 0.5f, 0.5f }) * glm::translate(glm::mat4(1.0f), { 1, 1, 1 });

			// Render entities
			const Buffer lightIndex(&i, sizeof(uint32_t));
			for (auto& [mk, dc] : staticDrawList)
			{
				X2_CORE_VERIFY(curTransformMap->find(mk) != curTransformMap->end());
				const auto& transformData = curTransformMap->at(mk);
				Renderer::RenderStaticMeshWithMaterial(cb, m_ShadowPassPipelines[i], uniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, buffer, transformData.TransformOffset, dc.InstanceCount, m_ShadowPassMaterial, lightIndex);
			}

			Renderer::EndRenderPass(cb);
		}
	}


};
