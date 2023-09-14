#include "Precompiled.h"
#include "VulkanShader.h"

#include "ShaderCompiler/VulkanShaderCompiler.h"

#include <filesystem>

#include "X2/Renderer/Renderer.h"
#include "X2/Utilities/StringUtils.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "VulkanShaderUtils.h"

#include "X2/Core/Hash.h"

#include "X2/ImGui/ImGui.h"

#include "X2/Renderer/ShaderPack.h"

namespace X2 {

	VulkanShader::VulkanShader(const std::string& path, bool forceCompile, bool disableOptimization)
		: m_AssetPath(path), m_DisableOptimization(disableOptimization)
	{
		// TODO: This should be more "general"
		size_t found = path.find_last_of("/\\");
		m_Name = found != std::string::npos ? path.substr(found + 1) : path;
		found = m_Name.find_last_of('.');
		m_Name = found != std::string::npos ? m_Name.substr(0, found) : m_Name;

		Reload(forceCompile);
	}

	void VulkanShader::Release()
	{
		auto& pipelineCIs = m_PipelineShaderStageCreateInfos;
		Renderer::SubmitResourceFree([pipelineCIs, materialSets = m_ShaderMtDescSets, layouts = m_DescriptorSetLayouts]()
			{
				const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

				for (const auto& ci : pipelineCIs)
					if (ci.module)
						vkDestroyShaderModule(vulkanDevice, ci.module, nullptr);

				for (auto& materialSet : materialSets)
					if (materialSet.second.Pool)
						vkDestroyDescriptorPool(vulkanDevice, materialSet.second.Pool, nullptr);
				
				for (auto& layout : layouts)
					vkDestroyDescriptorSetLayout(vulkanDevice, layout, nullptr);

			});

		for (auto& ci : pipelineCIs)
			ci.module = nullptr;

		m_PipelineShaderStageCreateInfos.clear();
		m_ShaderMtDescSets.clear();
		m_DescriptorSetLayouts.clear();
		m_TypeCounts.clear();
	}

	VulkanShader::~VulkanShader()
	{
		VkDevice device = VulkanContext::Get()->GetDevice()->GetVulkanDevice();
		Renderer::SubmitResourceFree([device,shaderstageInfos = m_PipelineShaderStageCreateInfos, materialSets = m_ShaderMtDescSets, layouts = m_DescriptorSetLayouts]()
			{
				for (const auto& ci : shaderstageInfos)
					if (ci.module)
						vkDestroyShaderModule(device, ci.module, nullptr);

				for (auto& materialSet : materialSets)
					if (materialSet.second.Pool)
						vkDestroyDescriptorPool(device, materialSet.second.Pool, nullptr);

				for (auto& layout : layouts)
					vkDestroyDescriptorSetLayout(device, layout, nullptr);

			});
	}

	void VulkanShader::RT_Reload(const bool forceCompile)
	{
		if (!VulkanShaderCompiler::TryRecompile(this))
		{
			X2_CORE_FATAL("Failed to recompile shader!");
		}
	}

	void VulkanShader::Reload(bool forceCompile)
	{
		Renderer::Submit([instance = this, forceCompile]() mutable
			{
				instance->RT_Reload(forceCompile);
			});
	}

	size_t VulkanShader::GetHash() const
	{
		return Hash::GenerateFNVHash(m_AssetPath.string());
	}

	void VulkanShader::LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		m_ShaderData = shaderData;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		m_PipelineShaderStageCreateInfos.clear();
		std::string moduleName;
		for (auto [stage, data] : shaderData)
		{
			X2_CORE_ASSERT(data.size());
			VkShaderModuleCreateInfo moduleCreateInfo{};

			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
			moduleCreateInfo.pCode = data.data();

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SHADER_MODULE, fmt::format("{}:{}", m_Name, ShaderUtils::ShaderStageToString(stage)), shaderModule);

			VkPipelineShaderStageCreateInfo& shaderStage = m_PipelineShaderStageCreateInfos.emplace_back();
			shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStage.stage = stage;
			shaderStage.module = shaderModule;
			shaderStage.pName = "main";
		}
	}



	void VulkanShader::CreateDescriptors()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		//////////////////////////////////////////////////////////////////////
		// Descriptor Pool
		//////////////////////////////////////////////////////////////////////

		m_TypeCounts.clear();
		for (uint32_t set = 0; set < m_ReflectionData.ShaderDescriptorSets.size(); set++)
		{
			auto& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[set];

			if (shaderDescriptorSet.UniformBuffers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.UniformBuffers.size());
			}
			if (shaderDescriptorSet.StorageBuffers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.StorageBuffers.size());
			}
			if (shaderDescriptorSet.ImageSamplers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.ImageSamplers.size());
			}
			if (shaderDescriptorSet.SeparateTextures.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.SeparateTextures.size());
			}
			if (shaderDescriptorSet.SeparateSamplers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.SeparateSamplers.size());
			}
			if (shaderDescriptorSet.StorageImages.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.StorageImages.size());
			}

#if 0
			// TODO: Move this to the centralized renderer
			VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.pNext = nullptr;
			descriptorPoolInfo.poolSizeCount = m_TypeCounts.size();
			descriptorPoolInfo.pPoolSizes = m_TypeCounts.data();
			descriptorPoolInfo.maxSets = 1;

			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_DescriptorPool));
#endif

			//////////////////////////////////////////////////////////////////////
			// Descriptor Set Layout
			//////////////////////////////////////////////////////////////////////


			std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
			for (auto& [binding, uniformBuffer] : shaderDescriptorSet.UniformBuffers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				layoutBinding.descriptorCount = 1;
				layoutBinding.stageFlags = uniformBuffer.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[uniformBuffer.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = 1;
				set.dstBinding = layoutBinding.binding;
			}

			for (auto& [binding, storageBuffer] : shaderDescriptorSet.StorageBuffers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				layoutBinding.descriptorCount = 1;
				layoutBinding.stageFlags = storageBuffer.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;
				X2_CORE_ASSERT(shaderDescriptorSet.UniformBuffers.find(binding) == shaderDescriptorSet.UniformBuffers.end(), "Binding is already present!");

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[storageBuffer.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = 1;
				set.dstBinding = layoutBinding.binding;
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.ImageSamplers)
			{
				auto& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				layoutBinding.descriptorCount = imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				X2_CORE_ASSERT(shaderDescriptorSet.UniformBuffers.find(binding) == shaderDescriptorSet.UniformBuffers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.StorageBuffers.find(binding) == shaderDescriptorSet.StorageBuffers.end(), "Binding is already present!");

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = imageSampler.ArraySize;
				set.dstBinding = layoutBinding.binding;
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.SeparateTextures)
			{
				auto& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				layoutBinding.descriptorCount = imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				X2_CORE_ASSERT(shaderDescriptorSet.UniformBuffers.find(binding) == shaderDescriptorSet.UniformBuffers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.ImageSamplers.find(binding) == shaderDescriptorSet.ImageSamplers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.StorageBuffers.find(binding) == shaderDescriptorSet.StorageBuffers.end(), "Binding is already present!");

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = imageSampler.ArraySize;
				set.dstBinding = layoutBinding.binding;
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.SeparateSamplers)
			{
				auto& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				layoutBinding.descriptorCount = imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				X2_CORE_ASSERT(shaderDescriptorSet.UniformBuffers.find(binding) == shaderDescriptorSet.UniformBuffers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.ImageSamplers.find(binding) == shaderDescriptorSet.ImageSamplers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.StorageBuffers.find(binding) == shaderDescriptorSet.StorageBuffers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.SeparateTextures.find(binding) == shaderDescriptorSet.SeparateTextures.end(), "Binding is already present!");

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = imageSampler.ArraySize;
				set.dstBinding = layoutBinding.binding;
			}

			for (auto& [bindingAndSet, imageSampler] : shaderDescriptorSet.StorageImages)
			{
				auto& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				layoutBinding.descriptorCount = imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;

				uint32_t binding = bindingAndSet & 0xffffffff;
				//uint32_t descriptorSet = (bindingAndSet >> 32);
				layoutBinding.binding = binding;

				X2_CORE_ASSERT(shaderDescriptorSet.UniformBuffers.find(binding) == shaderDescriptorSet.UniformBuffers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.StorageBuffers.find(binding) == shaderDescriptorSet.StorageBuffers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.ImageSamplers.find(binding) == shaderDescriptorSet.ImageSamplers.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.SeparateTextures.find(binding) == shaderDescriptorSet.SeparateTextures.end(), "Binding is already present!");
				X2_CORE_ASSERT(shaderDescriptorSet.SeparateSamplers.find(binding) == shaderDescriptorSet.SeparateSamplers.end(), "Binding is already present!");

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = 1;
				set.dstBinding = layoutBinding.binding;
			}

			VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
			descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorLayout.pNext = nullptr;
			descriptorLayout.bindingCount = (uint32_t)(layoutBindings.size());
			descriptorLayout.pBindings = layoutBindings.data();

			X2_CORE_INFO_TAG("Renderer", "Creating descriptor set {0} with {1} ubo's, {2} ssbo's, {3} samplers, {4} separate textures, {5} separate samplers and {6} storage images", set,
				shaderDescriptorSet.UniformBuffers.size(),
				shaderDescriptorSet.StorageBuffers.size(),
				shaderDescriptorSet.ImageSamplers.size(),
				shaderDescriptorSet.SeparateTextures.size(),
				shaderDescriptorSet.SeparateSamplers.size(),
				shaderDescriptorSet.StorageImages.size());
			if (set >= m_DescriptorSetLayouts.size())
				m_DescriptorSetLayouts.resize((size_t)(set + 1));
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_DescriptorSetLayouts[set]));
		}
	}

	VulkanShader::ShaderMaterialDescriptorSet VulkanShader::AllocateDescriptorSet(uint32_t set)
	{
		X2_CORE_ASSERT(set < m_DescriptorSetLayouts.size());
		ShaderMaterialDescriptorSet result;

		if (m_ReflectionData.ShaderDescriptorSets.empty())
			return result;

		// TODO: remove
		result.Pool = nullptr;

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_DescriptorSetLayouts[set];
		VkDescriptorSet descriptorSet = VulkanRenderer::RT_AllocateDescriptorSet(allocInfo);
		X2_CORE_ASSERT(descriptorSet);
		result.DescriptorSets.push_back(descriptorSet);
		return result;
	}

	VulkanShader::ShaderMaterialDescriptorSet VulkanShader::CreateOrGetDescriptorSets(uint32_t set)
	{
		if (m_ShaderMtDescSets.find(set) == m_ShaderMtDescSets.end())
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			X2_CORE_ASSERT(m_TypeCounts.find(set) != m_TypeCounts.end());

			// TODO: Move this to the centralized renderer
			VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.pNext = nullptr;
			descriptorPoolInfo.poolSizeCount = (uint32_t)m_TypeCounts.at(set).size();
			descriptorPoolInfo.pPoolSizes = m_TypeCounts.at(set).data();
			descriptorPoolInfo.maxSets = 1;

			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_ShaderMtDescSets[set].Pool));

			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_ShaderMtDescSets[set].Pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &m_DescriptorSetLayouts[set];

			m_ShaderMtDescSets[set].DescriptorSets.emplace_back();
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, m_ShaderMtDescSets[set].DescriptorSets.data()));
		}
		return m_ShaderMtDescSets[set];
	}

	VulkanShader::ShaderMaterialDescriptorSet VulkanShader::CreateOrGetDescriptorSets(uint32_t set, uint32_t numberOfSets)
	{
		if (m_ShaderMtDescSets.find(set) == m_ShaderMtDescSets.end())
		{

			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> poolSizes;
			for (uint32_t set = 0; set < m_ReflectionData.ShaderDescriptorSets.size(); set++)
			{
				auto& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[set];
				if (!shaderDescriptorSet) // Empty descriptor set
					continue;

				if (shaderDescriptorSet.UniformBuffers.size())
				{
					VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
					typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.UniformBuffers.size() * numberOfSets;
				}
				if (shaderDescriptorSet.StorageBuffers.size())
				{
					VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
					typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.StorageBuffers.size() * numberOfSets;
				}
				if (shaderDescriptorSet.ImageSamplers.size())
				{
					VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
					typeCount.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					uint32_t descriptorSetCount = 0;
					for (auto&& [binding, imageSampler] : shaderDescriptorSet.ImageSamplers)
						descriptorSetCount += imageSampler.ArraySize;

					typeCount.descriptorCount = descriptorSetCount * numberOfSets;
				}
				if (shaderDescriptorSet.SeparateTextures.size())
				{
					VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
					typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					uint32_t descriptorSetCount = 0;
					for (auto&& [binding, imageSampler] : shaderDescriptorSet.SeparateTextures)
						descriptorSetCount += imageSampler.ArraySize;

					typeCount.descriptorCount = descriptorSetCount * numberOfSets;
				}
				if (shaderDescriptorSet.SeparateTextures.size())
				{
					VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
					typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLER;
					uint32_t descriptorSetCount = 0;
					for (auto&& [binding, imageSampler] : shaderDescriptorSet.SeparateSamplers)
						descriptorSetCount += imageSampler.ArraySize;

					typeCount.descriptorCount = descriptorSetCount * numberOfSets;
				}
				if (shaderDescriptorSet.StorageImages.size())
				{
					VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
					typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.StorageImages.size() * numberOfSets;
				}

			}

			X2_CORE_ASSERT(poolSizes.find(set) != poolSizes.end());

			// TODO: Move this to the centralized renderer
			VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.pNext = nullptr;
			descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.at(set).size();
			descriptorPoolInfo.pPoolSizes = poolSizes.at(set).data();
			descriptorPoolInfo.maxSets = numberOfSets;

			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_ShaderMtDescSets[set].Pool));

			m_ShaderMtDescSets[set].DescriptorSets.resize(numberOfSets);

			for (uint32_t i = 0; i < numberOfSets; i++)
			{
				VkDescriptorSetAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorPool = m_ShaderMtDescSets[set].Pool;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &m_DescriptorSetLayouts[set];

				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_ShaderMtDescSets[set].DescriptorSets[i]));
			}
		}
		return  m_ShaderMtDescSets[set];
	}

	const VkWriteDescriptorSet* VulkanShader::GetDescriptorSet(const std::string& name, uint32_t set) const
	{
		X2_CORE_ASSERT(set < m_ReflectionData.ShaderDescriptorSets.size());
		X2_CORE_ASSERT(m_ReflectionData.ShaderDescriptorSets[set]);
		if (m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets.find(name) == m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets.end())
		{
			X2_CORE_WARN_TAG("Renderer", "VulkanShader {0} does not contain requested descriptor set {1}", m_Name, name);
			return nullptr;
		}
		return &m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets.at(name);
	}

	std::vector<VkDescriptorSetLayout> VulkanShader::GetAllDescriptorSetLayouts()
	{
		std::vector<VkDescriptorSetLayout> result;
		result.reserve(m_DescriptorSetLayouts.size());
		for (auto& layout : m_DescriptorSetLayouts)
			result.emplace_back(layout);

		return result;
	}

	const std::unordered_map<std::string, ShaderResourceDeclaration>& VulkanShader::GetResources() const
	{
		return m_ReflectionData.Resources;
	}

	void VulkanShader::AddShaderReloadedCallback(const ShaderReloadedCallback& callback)
	{
	}

	bool VulkanShader::TryReadReflectionData(StreamReader* serializer)
	{
		uint32_t shaderDescriptorSetCount;
		serializer->ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			auto& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			serializer->ReadMap(descriptorSet.UniformBuffers);
			serializer->ReadMap(descriptorSet.StorageBuffers);
			serializer->ReadMap(descriptorSet.ImageSamplers);
			serializer->ReadMap(descriptorSet.StorageImages);
			serializer->ReadMap(descriptorSet.SeparateTextures);
			serializer->ReadMap(descriptorSet.SeparateSamplers);
			serializer->ReadMap(descriptorSet.WriteDescriptorSets);
		}

		serializer->ReadMap(m_ReflectionData.Resources);
		serializer->ReadMap(m_ReflectionData.ConstantBuffers);
		serializer->ReadArray(m_ReflectionData.PushConstantRanges);

		return true;
	}

	void VulkanShader::SerializeReflectionData(StreamWriter* serializer)
	{
		serializer->WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());
		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer->WriteMap(descriptorSet.UniformBuffers);
			serializer->WriteMap(descriptorSet.StorageBuffers);
			serializer->WriteMap(descriptorSet.ImageSamplers);
			serializer->WriteMap(descriptorSet.StorageImages);
			serializer->WriteMap(descriptorSet.SeparateTextures);
			serializer->WriteMap(descriptorSet.SeparateSamplers);
			serializer->WriteMap(descriptorSet.WriteDescriptorSets);
		}

		serializer->WriteMap(m_ReflectionData.Resources);
		serializer->WriteMap(m_ReflectionData.ConstantBuffers);
		serializer->WriteArray(m_ReflectionData.PushConstantRanges);
	}

	void VulkanShader::SetReflectionData(const ReflectionData& reflectionData)
	{
		m_ReflectionData = reflectionData;
	}


	ShaderLibrary::ShaderLibrary()
	{
	}

	ShaderLibrary::~ShaderLibrary()
	{
	}

	void ShaderLibrary::Add(const X2::Ref<VulkanShader>& shader)
	{
		auto& name = shader->GetName();
		X2_CORE_ASSERT(m_Shaders.find(name) == m_Shaders.end());
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Load(std::string_view path, bool forceCompile, bool disableOptimization)
	{
		Ref<VulkanShader> shader;
		if (!forceCompile && m_ShaderPack)
		{
			if (m_ShaderPack->Contains(path))
				shader = m_ShaderPack->LoadShader(path);
		}
		else
		{
			// Try compile from source
			// Unavailable at runtime
			shader = VulkanShaderCompiler::Compile(path, forceCompile, disableOptimization);
		}

		auto& name = shader->GetName();
		X2_CORE_ASSERT(m_Shaders.find(name) == m_Shaders.end());
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Load(std::string_view name, const std::string& path)
	{
		X2_CORE_ASSERT(m_Shaders.find(std::string(name)) == m_Shaders.end());
		m_Shaders[std::string(name)] = CreateRef<VulkanShader>(path);
	}

	void ShaderLibrary::LoadShaderPack(const std::filesystem::path& path)
	{
		m_ShaderPack = CreateRef<ShaderPack>(std::string(PROJECT_ROOT) + path.string());
		if (!m_ShaderPack->IsLoaded())
		{
			m_ShaderPack = nullptr;
			X2_CORE_ERROR("Could not load shader pack: {}", path.string());
		}
	}

	const Ref<VulkanShader>& ShaderLibrary::Get(const std::string& name) const
	{
		X2_CORE_ASSERT(m_Shaders.find(name) != m_Shaders.end(), name);
		return m_Shaders.at(name);
	}

	ShaderUniform::ShaderUniform(std::string name, const ShaderUniformType type, const uint32_t size, const uint32_t offset)
		: m_Name(std::move(name)), m_Type(type), m_Size(size), m_Offset(offset)
	{
	}

	constexpr std::string_view ShaderUniform::UniformTypeToString(const ShaderUniformType type)
	{
		if (type == ShaderUniformType::Bool)
		{
			return std::string("Boolean");
		}
		else if (type == ShaderUniformType::Int)
		{
			return std::string("Int");
		}
		else if (type == ShaderUniformType::Float)
		{
			return std::string("Float");
		}

		return std::string("None");
	}

}
