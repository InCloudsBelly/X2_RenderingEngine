#include "Precompiled.h"
#include "VulkanRenderer.h"

#include "imgui.h"

#include "Vulkan.h"
#include "VulkanContext.h"

#include "X2/Core/Application.h"

#include "X2/Renderer/Renderer.h"
#include "X2/Renderer/SceneRenderer.h"

#include "X2/Asset/AssetManager.h"

#include "VulkanPipeline.h"
#include "VulkanVertexBuffer.h"
#include "VulkanIndexBuffer.h"
#include "VulkanFramebuffer.h"
#include "VulkanRenderCommandBuffer.h"

#include "VulkanShader.h"
#include "VulkanTexture.h"

#include "backends/imgui_impl_glfw.h"
//#include "examples/imgui_impl_vulkan_with_textures.h"

#include "X2/Core/Timer.h"
#include "X2/Core/Debug/Profiler.h"

#include "ShaderCompiler/VulkanShaderCompiler.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace X2 {
	struct VulkanRendererData
	{
		RendererCapabilities RenderCaps;

		Ref<VulkanTexture2D> BRDFLut;

		Ref<VulkanVertexBuffer> QuadVertexBuffer;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;
		VulkanShader::ShaderMaterialDescriptorSet QuadDescriptorSet;

		std::unordered_map<SceneRenderer*, std::vector<VulkanShader::ShaderMaterialDescriptorSet>> RendererDescriptorSet;
		VkDescriptorSet ActiveRendererDescriptorSet = nullptr;
		std::vector<VkDescriptorPool> DescriptorPools;
		std::vector<uint32_t> DescriptorPoolAllocationCount;

		// VulkanUniformBufferSet -> VulkanShader Hash -> Frame -> WriteDescriptor
		std::unordered_map<VulkanUniformBufferSet*, std::unordered_map<uint64_t, std::vector<std::vector<VkWriteDescriptorSet>>>> UniformBufferWriteDescriptorCache;
		std::unordered_map<VulkanStorageBufferSet*, std::unordered_map<uint64_t, std::vector<std::vector<VkWriteDescriptorSet>>>> StorageBufferWriteDescriptorCache;

		// Default samplers
		VkSampler SamplerClamp = nullptr;
		VkSampler SamplerPoint = nullptr;

		int32_t SelectedDrawCall = -1;
		int32_t DrawCallCount = 0;
	};

	static VulkanRendererData* s_Data = nullptr;

	namespace Utils {

		static const char* VulkanVendorIDToString(uint32_t vendorID)
		{
			switch (vendorID)
			{
			case 0x10DE: return "NVIDIA";
			case 0x1002: return "AMD";
			case 0x8086: return "INTEL";
			case 0x13B5: return "ARM";
			}
			return "Unknown";
		}

	}

	void VulkanRenderer::Init()
	{
		s_Data = hnew VulkanRendererData();
		const auto& config = Renderer::GetConfig();
		s_Data->DescriptorPools.resize(config.FramesInFlight);
		s_Data->DescriptorPoolAllocationCount.resize(config.FramesInFlight);

		auto& caps = s_Data->RenderCaps;
		auto& properties = VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetProperties();
		caps.Vendor = Utils::VulkanVendorIDToString(properties.vendorID);
		caps.Device = properties.deviceName;
		caps.Version = std::to_string(properties.driverVersion);

		Utils::DumpGPUInfo();

		// Create descriptor pools
		Renderer::Submit([]() mutable
			{
				// Create Descriptor Pool
				VkDescriptorPoolSize pool_sizes[] =
				{
					{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
					{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
					{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
					{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
					{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
					{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
					{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
				};
				VkDescriptorPoolCreateInfo pool_info = {};
				pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
				pool_info.maxSets = 100000;
				pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
				pool_info.pPoolSizes = pool_sizes;
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
				for (uint32_t i = 0; i < framesInFlight; i++)
				{
					VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &s_Data->DescriptorPools[i]));
					s_Data->DescriptorPoolAllocationCount[i] = 0;
				}
			});

		// Create fullscreen quad
		float x = -1;
		float y = -1;
		float width = 2, height = 2;
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec2 TexCoord;
		};

		QuadVertex* data = hnew QuadVertex[4];

		data[0].Position = glm::vec3(x, y, 0.0f);
		data[0].TexCoord = glm::vec2(0, 0);

		data[1].Position = glm::vec3(x + width, y, 0.0f);
		data[1].TexCoord = glm::vec2(1, 0);

		data[2].Position = glm::vec3(x + width, y + height, 0.0f);
		data[2].TexCoord = glm::vec2(1, 1);

		data[3].Position = glm::vec3(x, y + height, 0.0f);
		data[3].TexCoord = glm::vec2(0, 1);

		s_Data->QuadVertexBuffer = Ref<VulkanVertexBuffer>::Create(data, 4 * sizeof(QuadVertex));
		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0, };
		s_Data->QuadIndexBuffer = Ref<VulkanIndexBuffer>::Create(indices, 6 * sizeof(uint32_t));

		s_Data->BRDFLut = Renderer::GetBRDFLutTexture();
	}

	void VulkanRenderer::Shutdown()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

#if X2_HAS_SHADER_COMPILER
		VulkanShaderCompiler::ClearUniformBuffers();
#endif
		delete s_Data;
	}

	RendererCapabilities& VulkanRenderer::GetCapabilities()
	{
		return s_Data->RenderCaps;
	}

	static const std::vector<std::vector<VkWriteDescriptorSet>>& RT_RetrieveOrCreateUniformBufferWriteDescriptors(Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> vulkanMaterial)
	{
		X2_PROFILE_FUNC();

		size_t shaderHash = vulkanMaterial->GetShader()->GetHash();
		if (s_Data->UniformBufferWriteDescriptorCache.find(uniformBufferSet.Raw()) != s_Data->UniformBufferWriteDescriptorCache.end())
		{
			const auto& shaderMap = s_Data->UniformBufferWriteDescriptorCache.at(uniformBufferSet.Raw());
			if (shaderMap.find(shaderHash) != shaderMap.end())
			{
				const auto& writeDescriptors = shaderMap.at(shaderHash);
				return writeDescriptors;
			}
		}

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		Ref<VulkanShader> vulkanShader = vulkanMaterial->GetShader().As<VulkanShader>();
		if (vulkanShader->HasDescriptorSet(0))
		{
			const auto& shaderDescriptorSets = vulkanShader->GetShaderDescriptorSets();
			if (!shaderDescriptorSets.empty())
			{
				for (auto&& [binding, shaderUB] : shaderDescriptorSets[0].UniformBuffers)
				{
					auto& writeDescriptors = s_Data->UniformBufferWriteDescriptorCache[uniformBufferSet.Raw()][shaderHash];
					writeDescriptors.resize(framesInFlight);
					for (uint32_t frame = 0; frame < framesInFlight; frame++)
					{
						Ref<VulkanUniformBuffer> uniformBuffer = uniformBufferSet->Get(binding, 0, frame); // set = 0 for now

						VkWriteDescriptorSet writeDescriptorSet = {};
						writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						writeDescriptorSet.pBufferInfo = &uniformBuffer->GetDescriptorBufferInfo();
						writeDescriptorSet.dstBinding = uniformBuffer->GetBinding();
						writeDescriptors[frame].push_back(writeDescriptorSet);
					}
				}

			}
		}

		return s_Data->UniformBufferWriteDescriptorCache[uniformBufferSet.Raw()][shaderHash];
	}

	static const std::vector<std::vector<VkWriteDescriptorSet>>& RT_RetrieveOrCreateStorageBufferWriteDescriptors(Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> vulkanMaterial)
	{
		X2_PROFILE_FUNC();

		size_t shaderHash = vulkanMaterial->GetShader()->GetHash();
		if (s_Data->StorageBufferWriteDescriptorCache.find(storageBufferSet.Raw()) != s_Data->StorageBufferWriteDescriptorCache.end())
		{
			const auto& shaderMap = s_Data->StorageBufferWriteDescriptorCache.at(storageBufferSet.Raw());
			if (shaderMap.find(shaderHash) != shaderMap.end())
			{
				const auto& writeDescriptors = shaderMap.at(shaderHash);
				return writeDescriptors;
			}
		}

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		Ref<VulkanShader> vulkanShader = vulkanMaterial->GetShader().As<VulkanShader>();
		if (vulkanShader->HasDescriptorSet(0))
		{
			const auto& shaderDescriptorSets = vulkanShader->GetShaderDescriptorSets();
			if (!shaderDescriptorSets.empty())
			{
				for (auto&& [binding, shaderSB] : shaderDescriptorSets[0].StorageBuffers)
				{
					auto& writeDescriptors = s_Data->StorageBufferWriteDescriptorCache[storageBufferSet.Raw()][shaderHash];
					writeDescriptors.resize(framesInFlight);
					for (uint32_t frame = 0; frame < framesInFlight; frame++)
					{
						Ref<VulkanStorageBuffer> storageBuffer = storageBufferSet->Get(binding, 0, frame); // set = 0 for now

						VkWriteDescriptorSet writeDescriptorSet = {};
						writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						writeDescriptorSet.pBufferInfo = &storageBuffer->GetDescriptorBufferInfo();
						writeDescriptorSet.dstBinding = storageBuffer->GetBinding();
						writeDescriptors[frame].push_back(writeDescriptorSet);
					}
				}
			}
		}

		return s_Data->StorageBufferWriteDescriptorCache[storageBufferSet.Raw()][shaderHash];
	}

#if 0
	void VulkanRenderer::RT_UpdateMaterialForRendering(Ref<VulkanMaterial> material, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet)
	{
		X2_PROFILE_FUNC();
		X2_SCOPE_PERF("VulkanRenderer::RT_UpdateMaterialForRendering");

		if (uniformBufferSet)
		{
			auto writeDescriptors = RT_RetrieveOrCreateUniformBufferWriteDescriptors(uniformBufferSet, material);
			if (storageBufferSet)
			{
				const auto& storageBufferWriteDescriptors = RT_RetrieveOrCreateStorageBufferWriteDescriptors(storageBufferSet, material);

				const uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
				for (uint32_t frame = 0; frame < framesInFlight; frame++)
				{
					writeDescriptors[frame].reserve(writeDescriptors[frame].size() + storageBufferWriteDescriptors[frame].size());
					writeDescriptors[frame].insert(writeDescriptors[frame].end(), storageBufferWriteDescriptors[frame].begin(), storageBufferWriteDescriptors[frame].end());
				}
			}
			material->RT_UpdateForRendering(writeDescriptors);
		}
		else
		{
			material->RT_UpdateForRendering();
		}
	}
#endif

	void VulkanRenderer::RT_UpdateMaterialForRendering(Ref<VulkanMaterial> material, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet)
	{
		X2_PROFILE_FUNC();
		X2_SCOPE_PERF("VulkanRenderer::RT_UpdateMaterialForRendering");

		if (uniformBufferSet)
		{
			const auto& ubWriteDescriptors = RT_RetrieveOrCreateUniformBufferWriteDescriptors(uniformBufferSet, material);
			if (storageBufferSet)
			{
				const auto& storageBufferWriteDescriptors = RT_RetrieveOrCreateStorageBufferWriteDescriptors(storageBufferSet, material);
				material->RT_UpdateForRendering(ubWriteDescriptors, storageBufferWriteDescriptors);
			}
			else
			{
				material->RT_UpdateForRendering(ubWriteDescriptors, std::vector<std::vector<VkWriteDescriptorSet>>());
			}
		}
		else
		{
			material->RT_UpdateForRendering();
		}
	}

	VkSampler VulkanRenderer::GetClampSampler()
	{
		if (s_Data->SamplerClamp)
			return s_Data->SamplerClamp;

		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 100.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &s_Data->SamplerClamp));
		return s_Data->SamplerClamp;
	}

	VkSampler VulkanRenderer::GetPointSampler()
	{
		if (s_Data->SamplerPoint)
			return s_Data->SamplerPoint;

		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 100.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &s_Data->SamplerPoint));
		return s_Data->SamplerPoint;
	}

	int32_t& VulkanRenderer::GetSelectedDrawCall()
	{
		return s_Data->SelectedDrawCall;
	}

	void VulkanRenderer::RenderStaticMesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount)
	{
		X2_CORE_VERIFY(mesh);
		X2_CORE_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, materialTable, transformBuffer, transformOffset, instanceCount]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderMesh");
				X2_SCOPE_PERF("VulkanRenderer::RenderMesh");

				if (s_Data->SelectedDrawCall != -1 && s_Data->DrawCallCount > s_Data->SelectedDrawCall)
					return;

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<MeshSource> meshSource = mesh->GetMeshSource();
				Ref<VulkanVertexBuffer> vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

				Ref<VulkanVertexBuffer> vulkanTransformBuffer = transformBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbTransformBuffer = vulkanTransformBuffer->GetVulkanBuffer();
				VkDeviceSize instanceOffsets[1] = { transformOffset };
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vbTransformBuffer, instanceOffsets);

				auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				std::vector<std::vector<VkWriteDescriptorSet>> writeDescriptors;

				const auto& submeshes = meshSource->GetSubmeshes();
				const Submesh& submesh = submeshes[submeshIndex];
				auto& meshMaterialTable = mesh->GetMaterials();
				uint32_t materialCount = meshMaterialTable->GetMaterialCount();
				// NOTE(Yan): probably should not involve Asset Manager at this stage
				AssetHandle materialHandle = materialTable->HasMaterial(submesh.MaterialIndex) ? materialTable->GetMaterial(submesh.MaterialIndex) : meshMaterialTable->GetMaterial(submesh.MaterialIndex);
				Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);
				Ref<VulkanMaterial> vulkanMaterial = material->GetMaterial().As<VulkanMaterial>();
				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				if (s_Data->SelectedDrawCall != -1 && s_Data->DrawCallCount > s_Data->SelectedDrawCall)
					return;

				// NOTE: Descriptor Set 1 is owned by the renderer
				std::array<VkDescriptorSet, 2> descriptorSets = {
				vulkanMaterial->GetDescriptorSet(frameIndex),
					s_Data->ActiveRendererDescriptorSet
				};

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)uniformStorageBuffer.Size, uniformStorageBuffer.Data);

				vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);
				s_Data->DrawCallCount++;
			});
	}

#if 0
	void VulkanRenderer::RenderSubmesh(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform)
	{
		X2_CORE_VERIFY(mesh);
		X2_CORE_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, transform, materialTable]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderMesh");
				X2_SCOPE_PERF("VulkanRenderer::RenderMesh");

				if (s_Data->SelectedDrawCall != -1 && s_Data->DrawCallCount > s_Data->SelectedDrawCall)
					return;

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<MeshSource> meshSource = mesh->GetMeshSource();
				Ref<VulkanVertexBuffer> vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

				auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				VkDescriptorSet animationDataDS = VK_NULL_HANDLE;
				if (mesh->IsRigged())
				{
					auto temp = vulkanPipeline->GetSpecification().VulkanShader.As<VulkanShader>()->AllocateDescriptorSet(2); //  hard coding 2 = animation data.  Yuk.

					VkWriteDescriptorSet writeDescriptorSet;
					writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDescriptorSet.pNext = nullptr;
					writeDescriptorSet.dstSet = temp.DescriptorSets[0];
					writeDescriptorSet.dstBinding = 0;
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					writeDescriptorSet.pImageInfo = nullptr;
					writeDescriptorSet.pBufferInfo = &mesh->GetBoneTransformUB(frameIndex).As<VulkanUniformBuffer>()->GetDescriptorBufferInfo();
					writeDescriptorSet.pTexelBufferView = nullptr;
				}

				const auto& submeshes = meshSource->GetSubmeshes();
				const auto& submesh = submeshes[submeshIndex];
				auto& meshMaterialTable = mesh->GetMaterials();
				uint32_t materialCount = meshMaterialTable->GetMaterialCount();
				Ref<MaterialAsset> material = materialTable->HasMaterial(submesh.MaterialIndex) ? materialTable->GetMaterial(submesh.MaterialIndex) : meshMaterialTable->GetMaterial(submesh.MaterialIndex);
				Ref<VulkanMaterial> vulkanMaterial = material->GetMaterial().As<VulkanMaterial>();
				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				if (s_Data->SelectedDrawCall != -1 && s_Data->DrawCallCount > s_Data->SelectedDrawCall)
					return;

			NOTE: Descriptor Set 1 is owned by the renderer
				std::vector<VkDescriptorSet> descriptorSets = {
					vulkanMaterial->GetDescriptorSet(frameIndex),
					s_Data->ActiveRendererDescriptorSet
			};
			if (animationDataDS)
				descriptorSets.emplace_back(animationDataDS);

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

			Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), uniformStorageBuffer.Size, uniformStorageBuffer.Data);
			vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
			s_Data->DrawCallCount++;
			});
	}
#endif

	void VulkanRenderer::RenderSubmeshInstanced(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformStorageBuffers, uint32_t boneTransformsOffset, uint32_t instanceCount)
	{
		X2_CORE_VERIFY(mesh);
		X2_CORE_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, materialTable, transformBuffer, transformOffset, boneTransformStorageBuffers, boneTransformsOffset, instanceCount]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderMesh");
				X2_SCOPE_PERF("VulkanRenderer::RenderMesh");

				if (s_Data->SelectedDrawCall != -1 && s_Data->DrawCallCount > s_Data->SelectedDrawCall)
					return;

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<MeshSource> meshSource = mesh->GetMeshSource();
				Ref<VulkanVertexBuffer> vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize vertexOffsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, vertexOffsets);

				Ref<VulkanVertexBuffer> vulkanTransformBuffer = transformBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbTransformBuffer = vulkanTransformBuffer->GetVulkanBuffer();
				VkDeviceSize instanceOffsets[1] = { transformOffset };
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vbTransformBuffer, instanceOffsets);

				auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				const auto& submeshes = meshSource->GetSubmeshes();
				const auto& submesh = submeshes[submeshIndex];

				VkDescriptorSet animationDataDS = VK_NULL_HANDLE;
				if (submesh.IsRigged)
				{
					/*VkBuffer boneInfluenceVB = meshSource->GetBoneInfluenceBuffer().As<VulkanVertexBuffer>()->GetVulkanBuffer();
					vkCmdBindVertexBuffers(commandBuffer, 2, 1, &boneInfluenceVB, vertexOffsets);*/

					auto temp = vulkanPipeline->GetSpecification().Shader->AllocateDescriptorSet(2); //  hard coding 2 = animation data.  Yuk.

					VkWriteDescriptorSet writeDescriptorSet;
					writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDescriptorSet.pNext = nullptr;
					writeDescriptorSet.dstSet = temp.DescriptorSets[0];
					writeDescriptorSet.dstBinding = boneTransformStorageBuffers[frameIndex]->GetBinding();
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					writeDescriptorSet.pImageInfo = nullptr;
					writeDescriptorSet.pBufferInfo = &boneTransformStorageBuffers[frameIndex].As<VulkanUniformBuffer>()->GetDescriptorBufferInfo();
					writeDescriptorSet.pTexelBufferView = nullptr;

					vkUpdateDescriptorSets(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), 1, &writeDescriptorSet, 0, nullptr);
					animationDataDS = temp.DescriptorSets[0];
				}

				auto& meshMaterialTable = mesh->GetMaterials();
				uint32_t materialCount = meshMaterialTable->GetMaterialCount();
				// NOTE(Yan): probably should not involve Asset Manager at this stage
				AssetHandle materialHandle = materialTable->HasMaterial(submesh.MaterialIndex) ? materialTable->GetMaterial(submesh.MaterialIndex) : meshMaterialTable->GetMaterial(submesh.MaterialIndex);
				Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);
				Ref<VulkanMaterial> vulkanMaterial = material->GetMaterial().As<VulkanMaterial>();
				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				if (s_Data->SelectedDrawCall != -1 && s_Data->DrawCallCount > s_Data->SelectedDrawCall)
					return;

				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);

				// NOTE: Descriptor Set 1 is owned by the renderer
				std::vector<VkDescriptorSet> descriptorSets = {
						descriptorSet,
						s_Data->ActiveRendererDescriptorSet
				};
				if (animationDataDS)
					descriptorSets.emplace_back(animationDataDS);

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				uint32_t pushConstantOffset = 0;

				if (submesh.IsRigged)
				{
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, sizeof(uint32_t), &boneTransformsOffset);
					pushConstantOffset += 16; // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
				}

				if (uniformStorageBuffer)
				{
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, uniformStorageBuffer.Size, uniformStorageBuffer.Data);
				}

				vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);
				s_Data->DrawCallCount++;
			});
	}

	void VulkanRenderer::RenderMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, const std::vector<Ref<VulkanStorageBuffer>>& boneTransformStorageBuffers, uint32_t boneTransformsOffset, uint32_t instanceCount, Buffer additionalUniforms)
	{
		X2_CORE_ASSERT(mesh);
		X2_CORE_ASSERT(mesh->GetMeshSource());

		Buffer pushConstantBuffer;
		bool isRigged = mesh->GetMeshSource()->IsSubmeshRigged(submeshIndex);

		if (additionalUniforms.Size || isRigged)
		{
			pushConstantBuffer.Allocate(additionalUniforms.Size + (isRigged ? sizeof(uint32_t) : 0));
			if (additionalUniforms.Size)
				pushConstantBuffer.Write(additionalUniforms.Data, additionalUniforms.Size);

			if (isRigged)
				pushConstantBuffer.Write(&boneTransformsOffset, sizeof(uint32_t), additionalUniforms.Size);
		}

		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, vulkanMaterial, transformBuffer, transformOffset, boneTransformStorageBuffers, instanceCount, pushConstantBuffer]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderMeshWithMaterial");
				X2_SCOPE_PERF("VulkanRenderer::RenderMeshWithMaterial");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<MeshSource> meshSource = mesh->GetMeshSource();
				VkBuffer meshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBuffer();
				VkDeviceSize vertexOffsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshVB, vertexOffsets);

				VkBuffer transformVB = transformBuffer.As<VulkanVertexBuffer>()->GetVulkanBuffer();
				VkDeviceSize instanceOffsets[1] = { transformOffset };
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &transformVB, instanceOffsets);

				VkBuffer meshIB = meshSource->GetIndexBuffer().As<VulkanIndexBuffer>()->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, meshIB, 0, VK_INDEX_TYPE_UINT32);

				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				VkDescriptorSet animationDataDS = VK_NULL_HANDLE;

				const auto& submeshes = meshSource->GetSubmeshes();
				const auto& submesh = submeshes[submeshIndex];

				if (submesh.IsRigged)
				{
				/*	Ref<VulkanVertexBuffer> vulkanBoneInfluencesVB = meshSource->GetBoneInfluenceBuffer().As<VulkanVertexBuffer>();
					VkBuffer vbBoneInfluencesBuffer = vulkanBoneInfluencesVB->GetVulkanBuffer();
					vkCmdBindVertexBuffers(commandBuffer, 2, 1, &vbBoneInfluencesBuffer, vertexOffsets);*/

					auto temp = vulkanPipeline->GetSpecification().Shader->AllocateDescriptorSet(1); //  hard coding 1 = animation data.  Yuk.

					VkWriteDescriptorSet writeDescriptorSet;
					writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeDescriptorSet.pNext = nullptr;
					writeDescriptorSet.dstSet = temp.DescriptorSets[0];
					writeDescriptorSet.dstBinding = boneTransformStorageBuffers[frameIndex]->GetBinding();
					writeDescriptorSet.dstArrayElement = 0;
					writeDescriptorSet.descriptorCount = 1;
					writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					writeDescriptorSet.pImageInfo = nullptr;
					writeDescriptorSet.pBufferInfo = &boneTransformStorageBuffers[frameIndex].As<VulkanUniformBuffer>()->GetDescriptorBufferInfo();
					writeDescriptorSet.pTexelBufferView = nullptr;

					vkUpdateDescriptorSets(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), 1, &writeDescriptorSet, 0, nullptr);
					animationDataDS = temp.DescriptorSets[0];
				}

				float lineWidth = vulkanPipeline->GetSpecification().LineWidth;
				if (lineWidth != 1.0f)
					vkCmdSetLineWidth(commandBuffer, lineWidth);

				// Bind descriptor sets describing shader binding points
				// NOTE: Descriptor Set 0 is the material, Descriptor Set 1 (if present) is the animation data
				std::vector<VkDescriptorSet> descriptorSets;
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					descriptorSets.emplace_back(descriptorSet);
				if (animationDataDS)
					descriptorSets.emplace_back(animationDataDS);

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				uint32_t pushConstantOffset = 0;
				if (pushConstantBuffer.Size)
				{
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, pushConstantBuffer.Size, pushConstantBuffer.Data);
					pushConstantOffset += 16;  // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
				}

				if (uniformStorageBuffer)
				{
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, uniformStorageBuffer.Size, uniformStorageBuffer.Data);
				}

				vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);

				pushConstantBuffer.Release();
			});
	}

	void VulkanRenderer::RenderStaticMeshWithMaterial(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<StaticMesh> staticMesh, uint32_t submeshIndex, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount, Buffer additionalUniforms /*= Buffer()*/)
	{
		X2_CORE_ASSERT(staticMesh);
		X2_CORE_ASSERT(staticMesh->GetMeshSource());

		Buffer pushConstantBuffer;
		if (additionalUniforms.Size)
		{
			pushConstantBuffer.Allocate(additionalUniforms.Size);
			if (additionalUniforms.Size)
				pushConstantBuffer.Write(additionalUniforms.Data, additionalUniforms.Size);
		}

		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, staticMesh, submeshIndex, vulkanMaterial, transformBuffer, transformOffset, instanceCount, pushConstantBuffer]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderMeshWithMaterial");
				X2_SCOPE_PERF("VulkanRenderer::RenderMeshWithMaterial");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
				auto vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize vertexOffsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, vertexOffsets);

				Ref<VulkanVertexBuffer> vulkanTransformBuffer = transformBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbTransformBuffer = vulkanTransformBuffer->GetVulkanBuffer();
				VkDeviceSize instanceOffsets[1] = { transformOffset };
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vbTransformBuffer, instanceOffsets);

				auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				float lineWidth = vulkanPipeline->GetSpecification().LineWidth;
				if (lineWidth != 1.0f)
					vkCmdSetLineWidth(commandBuffer, lineWidth);

				// Bind descriptor sets describing shader binding points
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				uint32_t pushConstantOffset = 0;
				if (pushConstantBuffer.Size)
				{
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, pushConstantBuffer.Size, pushConstantBuffer.Data);
					pushConstantOffset += 16; // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
				}

				if (uniformStorageBuffer)
				{
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, uniformStorageBuffer.Size, uniformStorageBuffer.Data);
					pushConstantOffset += uniformStorageBuffer.Size;
				}

				const auto& submeshes = meshSource->GetSubmeshes();
				const auto& submesh = submeshes[submeshIndex];

				vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);

				pushConstantBuffer.Release();
			});
	}

	void VulkanRenderer::RenderQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::mat4& transform)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, vulkanMaterial, transform]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderQuad");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

				auto vulkanMeshVB = s_Data->QuadVertexBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

				auto vulkanMeshIB = s_Data->QuadIndexBuffer.As<VulkanIndexBuffer>();
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();

				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), uniformStorageBuffer.Size, uniformStorageBuffer.Data);
				vkCmdDrawIndexed(commandBuffer, s_Data->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);
			});
	}

	void VulkanRenderer::RenderGeometry(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, Ref<VulkanVertexBuffer> vertexBuffer, Ref<VulkanIndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount /*= 0*/)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		if (indexCount == 0)
			indexCount = indexBuffer->GetCount();

		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, vulkanMaterial, vertexBuffer, indexBuffer, transform, indexCount]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::RenderGeometry");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

				auto vulkanMeshVB = vertexBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

				auto vulkanMeshIB = indexBuffer.As<VulkanIndexBuffer>();
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				const auto& writeDescriptors = RT_RetrieveOrCreateUniformBufferWriteDescriptors(uniformBufferSet, vulkanMaterial);
				vulkanMaterial->RT_UpdateForRendering(writeDescriptors);

				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				if (uniformStorageBuffer)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), uniformStorageBuffer.Size, uniformStorageBuffer.Data);

				vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
			});
	}

	VkDescriptorSet VulkanRenderer::RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo)
	{
		X2_PROFILE_FUNC();

		uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		allocInfo.descriptorPool = s_Data->DescriptorPools[bufferIndex];
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkDescriptorSet result;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &result));
		s_Data->DescriptorPoolAllocationCount[bufferIndex] += allocInfo.descriptorSetCount;
		return result;
	}

#if 0
	void VulkanRenderer::SetUniformBuffer(Ref<UniformBuffer> uniformBuffer, uint32_t set)
	{
		Renderer::Submit([uniformBuffer, set]()
			{


				X2_CORE_ASSERT(set == 1); // Currently we only bind to Renderer-maintaned UBs, which are in descriptor set 1

				Ref<VulkanUniformBuffer> vulkanUniformBuffer = uniformBuffer.As<VulkanUniformBuffer>();

				VkWriteDescriptorSet writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSet.pBufferInfo = &vulkanUniformBuffer->GetDescriptorBufferInfo();
				writeDescriptorSet.dstBinding = uniformBuffer->GetBinding();
				writeDescriptorSet.dstSet = s_Data->RendererDescriptorSet.DescriptorSets[0];

				X2_CORE_WARN("VulkanRenderer - Updating descriptor set (VulkanRenderer::SetUniformBuffer)");
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			});
	}
#endif

	void VulkanRenderer::ClearImage(Ref<VulkanRenderCommandBuffer> commandBuffer, Ref<VulkanImage2D> image)
	{
		Renderer::Submit([commandBuffer, image = image.As<VulkanImage2D>()]
			{
				const auto vulkanCommandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());
				VkImageSubresourceRange subresourceRange{};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = image->GetSpecification().Mips;
				subresourceRange.layerCount = image->GetSpecification().Layers;

				VkClearColorValue clearColor{ 0.f, 0.f, 0.f, 0.f };
				vkCmdClearColorImage(vulkanCommandBuffer, image->GetImageInfo().Image, image->GetSpecification().Usage == ImageUsage::Storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &clearColor, 1, &subresourceRange);
			});
	}

	void VulkanRenderer::CopyImage(Ref<VulkanRenderCommandBuffer> commandBuffer, Ref<VulkanImage2D> sourceImage, Ref<VulkanImage2D> destinationImage)
	{
		Renderer::Submit([commandBuffer, src = sourceImage.As<VulkanImage2D>(), dst = destinationImage.As<VulkanImage2D>()]
			{
				const auto vulkanCommandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());

				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				VkImage srcImage = src->GetImageInfo().Image;
				VkImage dstImage = dst->GetImageInfo().Image;
				glm::uvec2 srcSize = src->GetSize();
				glm::uvec2 dstSize = dst->GetSize();

				VkImageCopy region;
				region.srcOffset = { 0, 0, 0 };
				region.dstOffset = { 0, 0, 0 };
				region.extent = { srcSize.x, srcSize.y, 1 };
				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.baseArrayLayer = 0;
				region.srcSubresource.mipLevel = 0;
				region.srcSubresource.layerCount = 1;
				region.dstSubresource = region.srcSubresource;

				VkImageLayout srcImageLayout = src->GetDescriptorInfo().imageLayout;
				VkImageLayout dstImageLayout = dst->GetDescriptorInfo().imageLayout;

				{
					VkImageMemoryBarrier imageMemoryBarrier{};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageMemoryBarrier.oldLayout = srcImageLayout;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					imageMemoryBarrier.image = srcImage;

					imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
					imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
					imageMemoryBarrier.subresourceRange.layerCount = 1;
					imageMemoryBarrier.subresourceRange.levelCount = 1;

					vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}

				{
					VkImageMemoryBarrier imageMemoryBarrier{};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageMemoryBarrier.oldLayout = dstImageLayout;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					imageMemoryBarrier.image = dstImage;

					imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
					imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
					imageMemoryBarrier.subresourceRange.layerCount = 1;
					imageMemoryBarrier.subresourceRange.levelCount = 1;

					vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}

				vkCmdCopyImage(vulkanCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);


				{
					VkImageMemoryBarrier imageMemoryBarrier{};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					imageMemoryBarrier.newLayout = srcImageLayout;
					imageMemoryBarrier.image = srcImage;

					imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
					imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
					imageMemoryBarrier.subresourceRange.layerCount = 1;
					imageMemoryBarrier.subresourceRange.levelCount = 1;

					vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}

				{
					VkImageMemoryBarrier imageMemoryBarrier{};
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					imageMemoryBarrier.newLayout = dstImageLayout;
					imageMemoryBarrier.image = dstImage;

					imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
					imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
					imageMemoryBarrier.subresourceRange.layerCount = 1;
					imageMemoryBarrier.subresourceRange.levelCount = 1;

					vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &imageMemoryBarrier);
				}
			});
	}

	void VulkanRenderer::DispatchComputeShader(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups, Buffer additionalUniforms)
	{
		auto vulkanMaterial = material.As<VulkanMaterial>();
		auto vulkanPipeline = pipeline.As<VulkanComputePipeline>();
		Buffer pushConstantBuffer;
		if (additionalUniforms.Size)
		{
			pushConstantBuffer.Allocate(additionalUniforms.Size);
			pushConstantBuffer.Write(additionalUniforms.Data, additionalUniforms.Size);
		}

		Renderer::Submit([renderer = this, renderCommandBuffer, vulkanPipeline, vulkanMaterial, uniformBufferSet, storageBufferSet, workGroups, pushConstantBuffer]() mutable
			{
				const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				const VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);

				renderer->RT_BeginGPUPerfMarker(renderCommandBuffer, vulkanPipeline->GetShader()->GetName());

				vulkanPipeline->RT_Begin(renderCommandBuffer);
				if (pushConstantBuffer.Size)
					vulkanPipeline->SetPushConstants(pushConstantBuffer.Data, pushConstantBuffer.Size);
				vulkanPipeline->Dispatch(descriptorSet, workGroups.x, workGroups.y, workGroups.z);
				vulkanPipeline->End();

				renderer->RT_EndGPUPerfMarker(renderCommandBuffer);
				pushConstantBuffer.Release();
			});

	}

	void VulkanRenderer::LightCulling(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanComputePipeline> pipelineCompute, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material, const glm::uvec3& workGroups)
	{
		auto vulkanMaterial = material.As<VulkanMaterial>();
		auto pipeline = pipelineCompute.As<VulkanComputePipeline>();
		Renderer::Submit([renderCommandBuffer, pipeline, vulkanMaterial, uniformBufferSet, storageBufferSet, workGroups]() mutable
			{
				const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				const VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);

				pipeline->RT_Begin(renderCommandBuffer);
				pipeline->Dispatch(descriptorSet, workGroups.x, workGroups.y, workGroups.z);

				VkMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0,
					1, &barrier,
					0, nullptr,
					0, nullptr);

				pipeline->End();
			});
	}

	void VulkanRenderer::SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material)
	{
		SubmitFullscreenQuad(renderCommandBuffer, pipeline, uniformBufferSet, nullptr, material);
	}

	void VulkanRenderer::SubmitFullscreenQuad(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanStorageBufferSet> storageBufferSet, Ref<VulkanMaterial> material)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, vulkanMaterial]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::SubmitFullscreenQuad");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

				auto vulkanMeshVB = s_Data->QuadVertexBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

				auto vulkanMeshIB = s_Data->QuadIndexBuffer.As<VulkanIndexBuffer>();
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				if (uniformStorageBuffer.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, uniformStorageBuffer.Size, uniformStorageBuffer.Data);

				vkCmdDrawIndexed(commandBuffer, s_Data->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);
			});
	}

	void VulkanRenderer::SubmitFullscreenQuadWithOverrides(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanPipeline> pipeline, Ref<VulkanUniformBufferSet> uniformBufferSet, Ref<VulkanMaterial> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides)
	{
		Buffer vertexPushConstantBuffer;
		if (vertexShaderOverrides)
		{
			vertexPushConstantBuffer.Allocate(vertexShaderOverrides.Size);
			vertexPushConstantBuffer.Write(vertexShaderOverrides.Data, vertexShaderOverrides.Size);
		}

		Buffer fragmentPushConstantBuffer;
		if (fragmentShaderOverrides)
		{
			fragmentPushConstantBuffer.Allocate(fragmentShaderOverrides.Size);
			fragmentPushConstantBuffer.Write(fragmentShaderOverrides.Data, fragmentShaderOverrides.Size);
		}

		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, vulkanMaterial, vertexPushConstantBuffer, fragmentPushConstantBuffer]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::SubmitFullscreenQuad");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();

				VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

				auto vulkanMeshVB = s_Data->QuadVertexBuffer.As<VulkanVertexBuffer>();
				VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

				auto vulkanMeshIB = s_Data->QuadIndexBuffer.As<VulkanIndexBuffer>();
				VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
				vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

				VkPipeline pipeline = vulkanPipeline->GetVulkanPipeline();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				if (uniformBufferSet)
				{
					const auto& writeDescriptors = RT_RetrieveOrCreateUniformBufferWriteDescriptors(uniformBufferSet, vulkanMaterial);
					vulkanMaterial->RT_UpdateForRendering(writeDescriptors);
				}
				else
				{
					vulkanMaterial->RT_UpdateForRendering();
				}

				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				if (vertexPushConstantBuffer)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, vertexPushConstantBuffer.Size, vertexPushConstantBuffer.Data);
				if (fragmentPushConstantBuffer)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vertexPushConstantBuffer.Size, fragmentPushConstantBuffer.Size, fragmentPushConstantBuffer.Data);

				vkCmdDrawIndexed(commandBuffer, s_Data->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);

				vertexPushConstantBuffer.Release();
				fragmentPushConstantBuffer.Release();
			});
	}

	void VulkanRenderer::SetSceneEnvironment(Ref<SceneRenderer> sceneRenderer, Ref<Environment> environment, Ref<VulkanImage2D> shadow, Ref<VulkanImage2D> spotShadow, Ref<VulkanImage2D> rayMarchingGrid)
	{
		if (!environment)
			environment = Renderer::GetEmptyEnvironment();

		Renderer::Submit([sceneRenderer, environment, shadow, spotShadow, rayMarchingGrid]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::SetSceneEnvironment");

				const auto shader = Renderer::GetShaderLibrary()->Get("PBR_Static");
				Ref<VulkanShader> pbrShader = shader.As<VulkanShader>();
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();

				if (s_Data->RendererDescriptorSet.find(sceneRenderer.Raw()) == s_Data->RendererDescriptorSet.end())
				{
					const uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
					s_Data->RendererDescriptorSet[sceneRenderer.Raw()].resize(framesInFlight);
					for (uint32_t i = 0; i < framesInFlight; i++)
						s_Data->RendererDescriptorSet.at(sceneRenderer.Raw())[i] = pbrShader->CreateDescriptorSets(1);

				}

				VkDescriptorSet descriptorSet = s_Data->RendererDescriptorSet.at(sceneRenderer.Raw())[bufferIndex].DescriptorSets[0];
				s_Data->ActiveRendererDescriptorSet = descriptorSet;

				std::array<VkWriteDescriptorSet, 6> writeDescriptors;

				Ref<VulkanTextureCube> radianceMap = environment->RadianceMap.As<VulkanTextureCube>();
				Ref<VulkanTextureCube> irradianceMap = environment->IrradianceMap.As<VulkanTextureCube>();

				writeDescriptors[0] = *pbrShader->GetDescriptorSet("u_EnvRadianceTex", 1);
				writeDescriptors[0].dstSet = descriptorSet;
				const auto& radianceMapImageInfo = radianceMap->GetVulkanDescriptorInfo();
				writeDescriptors[0].pImageInfo = &radianceMapImageInfo;

				writeDescriptors[1] = *pbrShader->GetDescriptorSet("u_EnvIrradianceTex", 1);
				writeDescriptors[1].dstSet = descriptorSet;
				const auto& irradianceMapImageInfo = irradianceMap->GetVulkanDescriptorInfo();
				writeDescriptors[1].pImageInfo = &irradianceMapImageInfo;

				writeDescriptors[2] = *pbrShader->GetDescriptorSet("u_BRDFLUTTexture", 1);
				writeDescriptors[2].dstSet = descriptorSet;
				const auto& brdfLutImageInfo = s_Data->BRDFLut.As<VulkanTexture2D>()->GetVulkanDescriptorInfo();
				writeDescriptors[2].pImageInfo = &brdfLutImageInfo;

				writeDescriptors[3] = *pbrShader->GetDescriptorSet("u_ShadowMapTexture", 1);
				writeDescriptors[3].dstSet = descriptorSet;
				const auto& shadowImageInfo = shadow.As<VulkanImage2D>()->GetDescriptorInfo();
				writeDescriptors[3].pImageInfo = &shadowImageInfo;

				writeDescriptors[4] = *pbrShader->GetDescriptorSet("u_SpotShadowTexture", 1);
				writeDescriptors[4].dstSet = descriptorSet;
				const auto& spotShadowImageInfo = spotShadow.As<VulkanImage2D>()->GetDescriptorInfo();
				writeDescriptors[4].pImageInfo = &spotShadowImageInfo;

				writeDescriptors[5] = *pbrShader->GetDescriptorSet("u_VoxelGrid", 1);
				writeDescriptors[5].dstSet = descriptorSet;
				const auto& rayMarchingGridDescInfo = rayMarchingGrid.As<VulkanImage2D>()->GetDescriptorInfo();
				writeDescriptors[5].pImageInfo = &rayMarchingGridDescInfo;

				const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				vkUpdateDescriptorSets(vulkanDevice, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);
			});
	}


	void VulkanRenderer::SetSceneEnvironment(Ref<SceneRenderer> sceneRenderer, Ref<Environment> environment, Ref<VulkanImage2D> shadow, Ref<VulkanImage2D> spotShadow)
	{
		if (!environment)
			environment = Renderer::GetEmptyEnvironment();

		Renderer::Submit([sceneRenderer, environment, shadow, spotShadow]() mutable
			{
				X2_PROFILE_FUNC("VulkanRenderer::SetSceneEnvironment");

				const auto shader = Renderer::GetShaderLibrary()->Get("PBR_Static");
				Ref<VulkanShader> pbrShader = shader.As<VulkanShader>();
				const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();

				if (s_Data->RendererDescriptorSet.find(sceneRenderer.Raw()) == s_Data->RendererDescriptorSet.end())
				{
					const uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
					s_Data->RendererDescriptorSet[sceneRenderer.Raw()].resize(framesInFlight);
					for (uint32_t i = 0; i < framesInFlight; i++)
						s_Data->RendererDescriptorSet.at(sceneRenderer.Raw())[i] = pbrShader->CreateDescriptorSets(1);

				}

				VkDescriptorSet descriptorSet = s_Data->RendererDescriptorSet.at(sceneRenderer.Raw())[bufferIndex].DescriptorSets[0];
				s_Data->ActiveRendererDescriptorSet = descriptorSet;

				std::array<VkWriteDescriptorSet, 5> writeDescriptors;

				Ref<VulkanTextureCube> radianceMap = environment->RadianceMap.As<VulkanTextureCube>();
				Ref<VulkanTextureCube> irradianceMap = environment->IrradianceMap.As<VulkanTextureCube>();

				writeDescriptors[0] = *pbrShader->GetDescriptorSet("u_EnvRadianceTex", 1);
				writeDescriptors[0].dstSet = descriptorSet;
				const auto& radianceMapImageInfo = radianceMap->GetVulkanDescriptorInfo();
				writeDescriptors[0].pImageInfo = &radianceMapImageInfo;

				writeDescriptors[1] = *pbrShader->GetDescriptorSet("u_EnvIrradianceTex", 1);
				writeDescriptors[1].dstSet = descriptorSet;
				const auto& irradianceMapImageInfo = irradianceMap->GetVulkanDescriptorInfo();
				writeDescriptors[1].pImageInfo = &irradianceMapImageInfo;

				writeDescriptors[2] = *pbrShader->GetDescriptorSet("u_BRDFLUTTexture", 1);
				writeDescriptors[2].dstSet = descriptorSet;
				const auto& brdfLutImageInfo = s_Data->BRDFLut.As<VulkanTexture2D>()->GetVulkanDescriptorInfo();
				writeDescriptors[2].pImageInfo = &brdfLutImageInfo;

				writeDescriptors[3] = *pbrShader->GetDescriptorSet("u_ShadowMapTexture", 1);
				writeDescriptors[3].dstSet = descriptorSet;
				const auto& shadowImageInfo = shadow.As<VulkanImage2D>()->GetDescriptorInfo();
				writeDescriptors[3].pImageInfo = &shadowImageInfo;

				writeDescriptors[4] = *pbrShader->GetDescriptorSet("u_SpotShadowTexture", 1);
				writeDescriptors[4].dstSet = descriptorSet;
				const auto& spotShadowImageInfo = spotShadow.As<VulkanImage2D>()->GetDescriptorInfo();
				writeDescriptors[4].pImageInfo = &spotShadowImageInfo;

				const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				vkUpdateDescriptorSets(vulkanDevice, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);
			});
	}

	void VulkanRenderer::BeginFrame()
	{
		Renderer::Submit([]()
			{
				X2_PROFILE_FUNC("VulkanRenderer::BeginFrame");

				VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();

				// Reset descriptor pools here
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				uint32_t bufferIndex = swapChain.GetCurrentBufferIndex();
				vkResetDescriptorPool(device, s_Data->DescriptorPools[bufferIndex], 0);
				memset(s_Data->DescriptorPoolAllocationCount.data(), 0, s_Data->DescriptorPoolAllocationCount.size() * sizeof(uint32_t));

				s_Data->DrawCallCount = 0;

#if 0
				VkCommandBufferBeginInfo cmdBufInfo = {};
				cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				cmdBufInfo.pNext = nullptr;

				VkCommandBuffer drawCommandBuffer = swapChain.GetCurrentDrawCommandBuffer();
				commandBuffer = drawCommandBuffer;
				X2_CORE_ASSERT(commandBuffer);
				VK_CHECK_RESULT(vkBeginCommandBuffer(drawCommandBuffer, &cmdBufInfo));
#endif
			});
	}

	void VulkanRenderer::EndFrame()
	{
#if 0
		Renderer::Submit([]()
			{
				VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
				commandBuffer = nullptr;
			});
#endif
	}

	void VulkanRenderer::RT_InsertGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& color)
	{
		const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(bufferIndex);
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		memcpy(&debugLabel.color, glm::value_ptr(color), sizeof(float) * 4);
		debugLabel.pLabelName = label.c_str();
		fpCmdInsertDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	}

	void VulkanRenderer::RT_BeginGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor)
	{
		const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(bufferIndex);
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		memcpy(&debugLabel.color, glm::value_ptr(markerColor), sizeof(float) * 4);
		debugLabel.pLabelName = label.c_str();
		fpCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	}

	void VulkanRenderer::RT_EndGPUPerfMarker(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(bufferIndex);
		fpCmdEndDebugUtilsLabelEXT(commandBuffer);
	}

	void VulkanRenderer::BeginRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, const Ref<VulkanRenderPass>& renderPass, bool explicitClear)
	{
		Renderer::Submit([renderCommandBuffer, renderPass, explicitClear]()
			{
				X2_PROFILE_SCOPE_DYNAMIC(fmt::format("VulkanRenderer::BeginRenderPass ({})", renderPass->GetSpecification().DebugName).c_str());
				X2_CORE_TRACE_TAG("Renderer", "BeginRenderPass - {}", renderPass->GetSpecification().DebugName);

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				VkDebugUtilsLabelEXT debugLabel{};
				debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				memcpy(&debugLabel.color, glm::value_ptr(renderPass->GetSpecification().MarkerColor), sizeof(float) * 4);
				debugLabel.pLabelName = renderPass->GetSpecification().DebugName.c_str();
				fpCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

				auto fb = renderPass->GetSpecification().TargetFramebuffer;
				Ref<VulkanFramebuffer> framebuffer = fb.As<VulkanFramebuffer>();
				const auto& fbSpec = framebuffer->GetSpecification();

				uint32_t width = framebuffer->GetWidth();
				uint32_t height = framebuffer->GetHeight();

				VkViewport viewport = {};
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRenderPassBeginInfo renderPassBeginInfo = {};
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.pNext = nullptr;
				renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				if (framebuffer->GetSpecification().SwapChainTarget)
				{
					VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
					width = swapChain.GetWidth();
					height = swapChain.GetHeight();
					renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
					renderPassBeginInfo.pNext = nullptr;
					renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
					renderPassBeginInfo.renderArea.offset.x = 0;
					renderPassBeginInfo.renderArea.offset.y = 0;
					renderPassBeginInfo.renderArea.extent.width = width;
					renderPassBeginInfo.renderArea.extent.height = height;
					renderPassBeginInfo.framebuffer = swapChain.GetCurrentFramebuffer();

					viewport.x = 0.0f;
					viewport.y = (float)height;
					viewport.width = (float)width;
					viewport.height = -(float)height;
				}
				else
				{
					width = framebuffer->GetWidth();
					height = framebuffer->GetHeight();
					renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
					renderPassBeginInfo.pNext = nullptr;
					renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
					renderPassBeginInfo.renderArea.offset.x = 0;
					renderPassBeginInfo.renderArea.offset.y = 0;
					renderPassBeginInfo.renderArea.extent.width = width;
					renderPassBeginInfo.renderArea.extent.height = height;
					renderPassBeginInfo.framebuffer = framebuffer->GetVulkanFramebuffer();

					viewport.x = 0.0f;
					viewport.y = 0.0f;
					viewport.width = (float)width;
					viewport.height = (float)height;
				}

				// TODO: Does our framebuffer have a depth attachment?
				const auto& clearValues = framebuffer->GetVulkanClearValues();
				renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
				renderPassBeginInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				if (explicitClear)
				{
					const uint32_t colorAttachmentCount = (uint32_t)framebuffer->GetColorAttachmentCount();
					const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);
					X2_CORE_ASSERT(clearValues.size() == totalAttachmentCount);

					std::vector<VkClearAttachment> attachments(totalAttachmentCount);
					std::vector<VkClearRect> clearRects(totalAttachmentCount);
					for (uint32_t i = 0; i < colorAttachmentCount; i++)
					{
						attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						attachments[i].colorAttachment = i;
						attachments[i].clearValue = clearValues[i];

						clearRects[i].rect.offset = { (int32_t)0, (int32_t)0 };
						clearRects[i].rect.extent = { width, height };
						clearRects[i].baseArrayLayer = 0;
						clearRects[i].layerCount = 1;
					}

					if (framebuffer->HasDepthAttachment())
					{
						attachments[colorAttachmentCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
						attachments[colorAttachmentCount].clearValue = clearValues[colorAttachmentCount];
						clearRects[colorAttachmentCount].rect.offset = { (int32_t)0, (int32_t)0 };
						clearRects[colorAttachmentCount].rect.extent = { width, height };
						clearRects[colorAttachmentCount].baseArrayLayer = 0;
						clearRects[colorAttachmentCount].layerCount = 1;
					}

					vkCmdClearAttachments(commandBuffer, totalAttachmentCount, attachments.data(), totalAttachmentCount, clearRects.data());

				}

				// Update dynamic viewport state
				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

				// Update dynamic scissor state
				VkRect2D scissor = {};
				scissor.extent.width = width;
				scissor.extent.height = height;
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
			});
	}

	void VulkanRenderer::EndRenderPass(Ref<VulkanRenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([renderCommandBuffer]()
			{
				X2_PROFILE_FUNC("VulkanRenderer::EndRenderPass");

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

				vkCmdEndRenderPass(commandBuffer);
				fpCmdEndDebugUtilsLabelEXT(commandBuffer);
			});
	}

	Ref<Environment> VulkanRenderer::CreateEnvironmentMap(const std::string& filepath)
	{
		if (!Renderer::GetConfig().ComputeEnvironmentMaps)
			return Ref<Environment>::Create(Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture());

		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;
		const uint32_t irradianceMapSize = 32;

		Ref<VulkanTexture2D> envEquirect = Ref<VulkanTexture2D>::Create(TextureSpecification(), filepath);
		X2_CORE_ASSERT(envEquirect->GetFormat() == ImageFormat::RGBA32F, "Texture is not HDR!");

		TextureSpecification cubemapSpec;
		cubemapSpec.Format = ImageFormat::RGBA32F;
		cubemapSpec.Width = cubemapSize;
		cubemapSpec.Height = cubemapSize;

		Ref<VulkanTextureCube> envUnfiltered = Ref<VulkanTextureCube>::Create(cubemapSpec);
		Ref<VulkanTextureCube> envFiltered = Ref<VulkanTextureCube>::Create(cubemapSpec);

		// Convert equirectangular to cubemap
		Ref<VulkanShader> equirectangularConversionShader = Renderer::GetShaderLibrary()->Get("EquirectangularToCubeMap");
		Ref<VulkanComputePipeline> equirectangularConversionPipeline = Ref<VulkanComputePipeline>::Create(equirectangularConversionShader);

		Renderer::Submit([equirectangularConversionPipeline, envEquirect, envUnfiltered, cubemapSize]() mutable
			{
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				Ref<VulkanShader> shader = equirectangularConversionPipeline->GetShader();

				std::array<VkWriteDescriptorSet, 2> writeDescriptors;
				auto descriptorSet = shader->CreateDescriptorSets();
				Ref<VulkanTextureCube> envUnfilteredCubemap = envUnfiltered.As<VulkanTextureCube>();
				writeDescriptors[0] = *shader->GetDescriptorSet("o_CubeMap");
				writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
				writeDescriptors[0].pImageInfo = &envUnfilteredCubemap->GetVulkanDescriptorInfo();

				Ref<VulkanTexture2D> envEquirectVK = envEquirect.As<VulkanTexture2D>();
				writeDescriptors[1] = *shader->GetDescriptorSet("u_EquirectangularTex");
				writeDescriptors[1].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
				writeDescriptors[1].pImageInfo = &envEquirectVK->GetVulkanDescriptorInfo();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);
				equirectangularConversionPipeline->Execute(descriptorSet.DescriptorSets.data(), (uint32_t)descriptorSet.DescriptorSets.size(), cubemapSize / 32, cubemapSize / 32, 6);

				envUnfilteredCubemap->GenerateMips(true);
			});

		Ref<VulkanShader> environmentMipFilterShader = Renderer::GetShaderLibrary()->Get("EnvironmentMipFilter");
		Ref<VulkanComputePipeline> environmentMipFilterPipeline = Ref<VulkanComputePipeline>::Create(environmentMipFilterShader);

		Renderer::Submit([environmentMipFilterPipeline, envUnfiltered, envFiltered, cubemapSize]() mutable
			{
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				Ref<VulkanShader> shader = environmentMipFilterPipeline->GetShader();

				Ref<VulkanTextureCube> envFilteredCubemap = envFiltered.As<VulkanTextureCube>();
				VkDescriptorImageInfo imageInfo = envFilteredCubemap->GetVulkanDescriptorInfo();

				uint32_t mipCount = Utils::CalculateMipCount(cubemapSize, cubemapSize);

				std::vector<VkWriteDescriptorSet> writeDescriptors(mipCount * 2);
				std::vector<VkDescriptorImageInfo> mipImageInfos(mipCount);
				auto descriptorSet = shader->CreateDescriptorSets(0, 12);
				for (uint32_t i = 0; i < mipCount; i++)
				{
					VkDescriptorImageInfo& mipImageInfo = mipImageInfos[i];
					mipImageInfo = imageInfo;
					mipImageInfo.imageView = envFilteredCubemap->CreateImageViewSingleMip(i);

					writeDescriptors[i * 2 + 0] = *shader->GetDescriptorSet("outputTexture");
					writeDescriptors[i * 2 + 0].dstSet = descriptorSet.DescriptorSets[i]; // Should this be set inside the shader?
					writeDescriptors[i * 2 + 0].pImageInfo = &mipImageInfo;

					Ref<VulkanTextureCube> envUnfilteredCubemap = envUnfiltered.As<VulkanTextureCube>();
					writeDescriptors[i * 2 + 1] = *shader->GetDescriptorSet("inputTexture");
					writeDescriptors[i * 2 + 1].dstSet = descriptorSet.DescriptorSets[i]; // Should this be set inside the shader?
					writeDescriptors[i * 2 + 1].pImageInfo = &envUnfilteredCubemap->GetVulkanDescriptorInfo();
				}

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);

				environmentMipFilterPipeline->RT_Begin(); // begin compute pass
				const float deltaRoughness = 1.0f / glm::max((float)envFiltered->GetMipLevelCount() - 1.0f, 1.0f);
				for (uint32_t i = 0, size = cubemapSize; i < mipCount; i++, size /= 2)
				{
					uint32_t numGroups = glm::max(1u, size / 32);
					float roughness = i * deltaRoughness;
					roughness = glm::max(roughness, 0.05f);
					environmentMipFilterPipeline->SetPushConstants(&roughness, sizeof(float));
					environmentMipFilterPipeline->Dispatch(descriptorSet.DescriptorSets[i], numGroups, numGroups, 6);
				}
				environmentMipFilterPipeline->End();
			});

		Ref<VulkanShader> environmentIrradianceShader = Renderer::GetShaderLibrary()->Get("EnvironmentIrradiance");
		Ref<VulkanComputePipeline> environmentIrradiancePipeline = Ref<VulkanComputePipeline>::Create(environmentIrradianceShader);

		cubemapSpec.Width = irradianceMapSize;
		cubemapSpec.Height = irradianceMapSize;
		Ref<VulkanTextureCube> irradianceMap = Ref<VulkanTextureCube>::Create(cubemapSpec);

		Renderer::Submit([environmentIrradiancePipeline, irradianceMap, envFiltered]() mutable
			{
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				Ref<VulkanShader> shader = environmentIrradiancePipeline->GetShader();

				Ref<VulkanTextureCube> envFilteredCubemap = envFiltered.As<VulkanTextureCube>();
				Ref<VulkanTextureCube> irradianceCubemap = irradianceMap.As<VulkanTextureCube>();
				auto descriptorSet = shader->CreateDescriptorSets();

				std::array<VkWriteDescriptorSet, 2> writeDescriptors;
				writeDescriptors[0] = *shader->GetDescriptorSet("o_IrradianceMap");
				writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0];
				writeDescriptors[0].pImageInfo = &irradianceCubemap->GetVulkanDescriptorInfo();

				writeDescriptors[1] = *shader->GetDescriptorSet("u_RadianceMap");
				writeDescriptors[1].dstSet = descriptorSet.DescriptorSets[0];
				writeDescriptors[1].pImageInfo = &envFilteredCubemap->GetVulkanDescriptorInfo();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);
				environmentIrradiancePipeline->RT_Begin();
				environmentIrradiancePipeline->SetPushConstants(&Renderer::GetConfig().IrradianceMapComputeSamples, sizeof(uint32_t));
				environmentIrradiancePipeline->Dispatch(descriptorSet.DescriptorSets[0], irradianceMap->GetWidth() / 32, irradianceMap->GetHeight() / 32, 6);
				environmentIrradiancePipeline->End();

				irradianceCubemap->GenerateMips();
			});

		return Ref<Environment>::Create(envUnfiltered, envFiltered, irradianceMap);
	}

	Ref<VulkanTextureCube> VulkanRenderer::CreatePreethamSky(float turbidity, float azimuth, float inclination)
	{
		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;
		const uint32_t irradianceMapSize = 32;

		TextureSpecification cubemapSpec;
		cubemapSpec.Format = ImageFormat::RGBA32F;
		cubemapSpec.Width = cubemapSize;
		cubemapSpec.Height = cubemapSize;

		Ref<VulkanTextureCube> environmentMap = Ref<VulkanTextureCube>::Create(cubemapSpec);

		Ref<VulkanShader> preethamSkyShader = Renderer::GetShaderLibrary()->Get("PreethamSky");
		Ref<VulkanComputePipeline> preethamSkyComputePipeline = Ref<VulkanComputePipeline>::Create(preethamSkyShader);

		glm::vec3 params = { turbidity, azimuth, inclination };
		Renderer::Submit([preethamSkyComputePipeline, environmentMap, cubemapSize, params]() mutable
			{
				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				Ref<VulkanShader> shader = preethamSkyComputePipeline->GetShader();

				std::array<VkWriteDescriptorSet, 1> writeDescriptors;
				auto descriptorSet = shader->CreateDescriptorSets();
				Ref<VulkanTextureCube> envUnfilteredCubemap = environmentMap.As<VulkanTextureCube>();
				writeDescriptors[0] = *shader->GetDescriptorSet("o_CubeMap");
				writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
				writeDescriptors[0].pImageInfo = &envUnfilteredCubemap->GetVulkanDescriptorInfo();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);

				preethamSkyComputePipeline->RT_Begin();
				preethamSkyComputePipeline->SetPushConstants(&params, sizeof(glm::vec3));
				preethamSkyComputePipeline->Dispatch(descriptorSet.DescriptorSets[0], cubemapSize / 32, cubemapSize / 32, 6);
				preethamSkyComputePipeline->End();

				envUnfilteredCubemap->GenerateMips(true);
			});

		return environmentMap;
	}

	uint32_t VulkanRenderer::GetDescriptorAllocationCount(uint32_t frameIndex)
	{
		return s_Data->DescriptorPoolAllocationCount[frameIndex];
	}

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
			VkImageSubresourceRange subresourceRange)
		{
			VkImageMemoryBarrier imageMemoryBarrier{};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			imageMemoryBarrier.srcAccessMask = srcAccessMask;
			imageMemoryBarrier.dstAccessMask = dstAccessMask;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			vkCmdPipelineBarrier(
				cmdbuffer,
				srcStageMask,
				dstStageMask,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask)
		{
			// Create an image barrier object
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			// Source layouts (old)
			// Source access mask controls actions that have to be finished on the old layout
			// before it will be transitioned to the new layout
			switch (oldImageLayout)
			{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				imageMemoryBarrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Target layouts (new)
			// Destination access mask controls the dependency for the new image layout
			switch (newImageLayout)
			{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (imageMemoryBarrier.srcAccessMask == 0)
				{
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Put barrier inside setup command buffer
			vkCmdPipelineBarrier(
				cmdbuffer,
				srcStageMask,
				dstStageMask,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageAspectFlags aspectMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask)
		{
			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = aspectMask;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;
			SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
		}

	}

}
