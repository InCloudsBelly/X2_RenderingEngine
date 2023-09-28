
#include "PointLightShadow.h"
#include "X2/Vulkan/VulkanRenderer.h"
#include <glm/gtx/compatibility.hpp>

namespace X2
{

	PointLightShadow::PointLightShadow(uint32_t resolution)
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
		spec.Transfer = true;
		spec.Width = resolution;
		spec.Height = resolution;
		spec.Layers = 6;
		spec.DebugName = "Point Light Shadow Array";


		FramebufferSpecification shadowMapFramebufferSpec;
		shadowMapFramebufferSpec.DebugName = "Point Light Shadow Map";
		shadowMapFramebufferSpec.Width = resolution;
		shadowMapFramebufferSpec.Height = resolution;
		shadowMapFramebufferSpec.Attachments = { ImageFormat::DEPTH32F };
		shadowMapFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		shadowMapFramebufferSpec.DepthClearValue = 1.0f;
		shadowMapFramebufferSpec.NoResize = true;

		auto shadowPassShader = Renderer::GetShaderLibrary()->Get("PointShadowMap");
		//auto shadowPassShaderAnim = Renderer::GetShaderLibrary()->Get("SpotShadowMap_Anim");


		PipelineSpecification pipelineSpec;
		pipelineSpec.DebugName = "SpotShadowPass";
		pipelineSpec.Shader = shadowPassShader;
		pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;
		pipelineSpec.Layout = vertexLayout;
		pipelineSpec.InstanceLayout = instanceLayout;

		for (uint32_t lightIndex = 0; lightIndex < MAX_POINT_LIGHT_SHADOW_COUNT; lightIndex++)
		{
			//Image
			m_ShaodwArrays[lightIndex] = CreateRef<VulkanImage2D>(spec);
			m_ShaodwArrays[lightIndex]->Invalidate();
			m_ShaodwArrays[lightIndex]->CreatePerLayerImageViews();

			//FrameBuffer
			shadowMapFramebufferSpec.ExistingImage = m_ShaodwArrays[lightIndex];



			for (int layer = 0; layer < 6; layer++)
			{
				shadowMapFramebufferSpec.ExistingImageLayers.clear();
				shadowMapFramebufferSpec.ExistingImageLayers.emplace_back(layer);

				RenderPassSpecification shadowMapRenderPassSpec;
				shadowMapRenderPassSpec.DebugName = shadowMapFramebufferSpec.DebugName;
				shadowMapRenderPassSpec.TargetFramebuffer = CreateRef<VulkanFramebuffer>(shadowMapFramebufferSpec);
				auto renderpass = CreateRef<VulkanRenderPass>(shadowMapRenderPassSpec);

				pipelineSpec.RenderPass = renderpass;
				m_ShadowPassPipelines[lightIndex * 6 + layer] = CreateRef<VulkanPipeline>(pipelineSpec);
			}
		}

		ImageSpecification shadowAtlasSpec;
		shadowAtlasSpec.Format = ImageFormat::DEPTH32F;
		shadowAtlasSpec.Type = ImageType::ImageCubeArray;
		shadowAtlasSpec.Usage = ImageUsage::Texture;
		shadowAtlasSpec.Width = resolution;
		shadowAtlasSpec.Height = resolution;
		shadowAtlasSpec.Layers = 6 * MAX_POINT_LIGHT_SHADOW_COUNT;
		shadowAtlasSpec.DebugName = "Point Light Shadow Array Atlas";

		m_ShadowCubemapAtlas = CreateRef<VulkanImage2D>(shadowAtlasSpec);
		m_ShadowCubemapAtlas->Invalidate();

		/*Renderer::Submit([cubeArray = m_ShadowCubemapAtlas]() mutable
			{
				VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->GetCommandBuffer(true);

				Utils::InsertImageMemoryBarrier(commandBuffer, cubeArray->GetImageInfo().Image,
					0, VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, cubeArray->GetSpecification().Mips, 0 , cubeArray->GetSpecification().Layers });

				VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);

			});*/

		m_ShadowPassMaterial = CreateRef<VulkanMaterial>(shadowPassShader, "PointShadowPass");


		
	}


	void PointLightShadow::Update(const std::vector<PointLightInfo>& pointLightInfos)
	{
		X2_ASSERT(pointLightInfos.size() <= MAX_POINT_LIGHT_SHADOW_COUNT, "Now Only Supports max 16 point Light Shadow");
		m_activePointLightCount = pointLightInfos.size();

		static glm::vec3 faces[] = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
									 glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
									 glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f) };

		static glm::vec3 ups[] = {   glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
									 glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
									 glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f) };

		for (size_t lightIndex = 0; lightIndex < pointLightInfos.size(); ++lightIndex)
		{
			auto& light = pointLightInfos[lightIndex];
			if (!light.CastsShadows)
				continue;
			glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.f, 0.1f, light.Radius);

			for(uint32_t layer = 0; layer < 6; ++layer)
				m_data.ViewProjection[lightIndex * 6 + layer] = projection * glm::lookAt(light.Position, light.Position + faces[layer], ups[layer]);
		}
	}



	void PointLightShadow::RenderStaticShadow(Ref<VulkanRenderCommandBuffer> cb, Ref<VulkanUniformBufferSet> uniformBufferSet, std::map<MeshKey, StaticDrawCommand>& staticDrawList, std::map<MeshKey, TransformMapData>* curTransformMap, Ref<VulkanVertexBuffer> buffer)
	{

		for (size_t lightIndex = 0; lightIndex < m_activePointLightCount; ++lightIndex)
		{
			for (uint32_t layer = 0; layer < 6; ++layer)
			{
				Renderer::BeginRenderPass(cb, m_ShadowPassPipelines[lightIndex * 6 + layer]->GetSpecification().RenderPass);

				// static glm::mat4 scaleBiasMatrix = glm::scale(glm::mat4(1.0f), { 0.5f, 0.5f, 0.5f }) * glm::translate(glm::mat4(1.0f), { 1, 1, 1 });

				int lightLayerIndex = lightIndex * 6 + layer;
				// Render entities
				const Buffer Index(&lightLayerIndex, sizeof(uint32_t));
				for (auto& [mk, dc] : staticDrawList)
				{
					X2_CORE_VERIFY(curTransformMap->find(mk) != curTransformMap->end());
					const auto& transformData = curTransformMap->at(mk);
					Renderer::RenderStaticMeshWithMaterial(cb, m_ShadowPassPipelines[lightIndex * 6 + layer], uniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, buffer, transformData.TransformOffset, dc.InstanceCount, m_ShadowPassMaterial, Index);
				}

				Renderer::EndRenderPass(cb);
			}
		}

		FillTexturesToAtlas(cb);
	}

	void PointLightShadow::FillTexturesToAtlas(Ref<VulkanRenderCommandBuffer> cb)
	{

		Renderer::Submit([cubeArray = m_ShaodwArrays, atlas = m_ShadowCubemapAtlas , cb = cb, activeLightCount = m_activePointLightCount]() mutable
		{
			int frameIndex = Renderer::RT_GetCurrentFrameIndex();

			uint32_t activeLayer = activeLightCount * 6;

			Utils::InsertImageMemoryBarrier(cb->GetCommandBuffer(frameIndex), atlas->GetImageInfo().Image,
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, atlas->GetSpecification().Mips, 0 , activeLayer });

			for (uint32_t i = 0; i < activeLightCount; ++i)
			{
				Utils::InsertImageMemoryBarrier(cb->GetCommandBuffer(frameIndex), cubeArray[i]->GetImageInfo().Image,
					VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, cubeArray[i]->GetSpecification().Mips, 0 , 6});

				VkImageBlit blit = {};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { int32_t(cubeArray[i]->GetSpecification().Width), int32_t(cubeArray[i]->GetSpecification().Height), 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				blit.srcSubresource.mipLevel = 0;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 6;


				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { int32_t(atlas->GetSpecification().Width),
										int32_t(atlas->GetSpecification().Height), 1 };

				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				blit.dstSubresource.mipLevel = 0;
				blit.dstSubresource.baseArrayLayer = 6 * i;
				blit.dstSubresource.layerCount = 6;

				vkCmdBlitImage(cb->GetCommandBuffer(frameIndex),
					cubeArray[i]->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					atlas->GetImageInfo().Image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

				Utils::InsertImageMemoryBarrier(cb->GetCommandBuffer(frameIndex), cubeArray[i]->GetImageInfo().Image,
					VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, cubeArray[i]->GetSpecification().Mips, 0 , 6 });
			}

			Utils::InsertImageMemoryBarrier(cb->GetCommandBuffer(frameIndex), atlas->GetImageInfo().Image,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, atlas->GetSpecification().Mips, 0 , activeLayer });

		});
	}





};
