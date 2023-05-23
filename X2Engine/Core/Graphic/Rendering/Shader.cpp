#include "Shader.h"
#include <filesystem>
#include <fstream>
#include <set>
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
//#include "Utils/Log.h"
#include "Core/Graphic/Rendering/RenderPassBase.h"

#include <stdexcept>

void Shader::parseShaderData(PipelineData& pipelineData)
{
	std::ifstream input_file(getPath());

	if (!input_file.is_open())
		throw(std::runtime_error("Failed to open shader file."));

	std::string text = std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
	nlohmann::json j = nlohmann::json::parse(text);
	this->m_shaderSettings = j.get<ShaderSettings>();
}

void Shader::loadSpirvs(PipelineData& pipelineData)
{
	for (const auto& spirvPath : m_shaderSettings.shaderPaths)
	{
		std::ifstream file(spirvPath, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			throw(std::runtime_error("Failed to open spv file: " + spirvPath + " ."));

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		pipelineData.spirvs.emplace(spirvPath, std::move(buffer));
	}
}

void Shader::createShaderModules(PipelineData& pipelineData)
{
	std::set<VkShaderStageFlagBits> stageSet = std::set<VkShaderStageFlagBits>();

	for (auto& spirvPair : pipelineData.spirvs)
	{
		SpvReflectShaderModule reflectModule = {};
		SpvReflectResult result = spvReflectCreateShaderModule(spirvPair.second.size(), reinterpret_cast<const uint32_t*>(spirvPair.second.data()), &reflectModule);


		if (result != SPV_REFLECT_RESULT_SUCCESS)
			throw(std::runtime_error("Failed to create shader reflect module."));

		if (stageSet.count(static_cast<VkShaderStageFlagBits>(reflectModule.shader_stage)))
		{
			spvReflectDestroyShaderModule(&reflectModule);
		}
		else
		{
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = spirvPair.second.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(spirvPair.second.data());

			VkShaderModule shaderModule;

			if (vkCreateShaderModule(Instance::getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
				throw(std::runtime_error("Failed to create shader module."));

			pipelineData.shaderModuleWrappers.emplace_back(ShaderModuleWrapper{ static_cast<VkShaderStageFlagBits>(reflectModule.shader_stage), shaderModule , reflectModule });
		}
	}
}

void Shader::populateShaderStages(PipelineData& pipelineData)
{
	pipelineData.stageInfos.resize(pipelineData.shaderModuleWrappers.size());
	size_t i = 0;
	for (auto& warp : pipelineData.shaderModuleWrappers)
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = warp.stage;
		shaderStageInfo.module = warp.shaderModule;
		shaderStageInfo.pName = warp.reflectModule.entry_point_name;

		pipelineData.stageInfos[i++] = shaderStageInfo;
	}
}

void Shader::populateVertexInputState(PipelineData& pipelineData)
{
	pipelineData.vertexInputBindingDescription.binding = 0;
	pipelineData.vertexInputBindingDescription.stride = sizeof(VertexData);
	pipelineData.vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	ShaderModuleWrapper vertexShaderWarp;
	bool containsVertexShader = false;
	for (size_t i = 0; i < pipelineData.shaderModuleWrappers.size(); i++)
	{
		if (pipelineData.shaderModuleWrappers[i].stage == VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT)
		{
			vertexShaderWarp = pipelineData.shaderModuleWrappers[i];
			containsVertexShader = true;
			break;
		}
	}
	if (!containsVertexShader)
		throw(std::runtime_error("Failed to find vertex shader."));

	uint32_t inputCount = 0;
	SpvReflectResult result = spvReflectEnumerateInputVariables(&vertexShaderWarp.reflectModule, &inputCount, NULL);
	if (result != SPV_REFLECT_RESULT_SUCCESS)
		throw(std::runtime_error("Failed to enumerate shader input variables."));


	std::vector<SpvReflectInterfaceVariable*> input_vars(inputCount);
	result = spvReflectEnumerateInputVariables(&vertexShaderWarp.reflectModule, &inputCount, input_vars.data());
	if (result != SPV_REFLECT_RESULT_SUCCESS)
		throw(std::runtime_error("Failed to enumerate shader input variables."));


	pipelineData.vertexInputAttributeDescriptions.resize(inputCount);
	for (size_t i_var = 0; i_var < input_vars.size(); ++i_var) {
		const SpvReflectInterfaceVariable& refl_var = *(input_vars[i_var]);
		// ignore built-in variables
		if (refl_var.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) continue;

		std::string reflectInputName = refl_var.name;
		std::transform(reflectInputName.begin(), reflectInputName.end(), reflectInputName.begin(), ::tolower);
		VkVertexInputAttributeDescription attr_desc{};

		if (reflectInputName.find("position") != std::string::npos)
		{
			attr_desc.location = refl_var.location;
			attr_desc.binding = 0;
			attr_desc.format = static_cast<VkFormat>(refl_var.format);
			attr_desc.offset = offsetof(VertexData, VertexData::position);
		}
		else if (reflectInputName.find("texcoord") != std::string::npos)
		{
			attr_desc.location = refl_var.location;
			attr_desc.binding = 0;
			attr_desc.format = static_cast<VkFormat>(refl_var.format);
			attr_desc.offset = offsetof(VertexData, VertexData::texCoords);
		}
		else if (reflectInputName.find("normal") != std::string::npos)
		{
			attr_desc.location = refl_var.location;
			attr_desc.binding = 0;
			attr_desc.format = static_cast<VkFormat>(refl_var.format);
			attr_desc.offset = offsetof(VertexData, VertexData::normal);
		}
		else if (reflectInputName.find("tangent") != std::string::npos)
		{
			attr_desc.location = refl_var.location;
			attr_desc.binding = 0;
			attr_desc.format = static_cast<VkFormat>(refl_var.format);
			attr_desc.offset = offsetof(VertexData, VertexData::tangent);
		}
		else if (reflectInputName.find("bitangent") != std::string::npos)
		{
			attr_desc.location = refl_var.location;
			attr_desc.binding = 0;
			attr_desc.format = static_cast<VkFormat>(refl_var.format);
			attr_desc.offset = offsetof(VertexData, VertexData::bitangent);
		}

		pipelineData.vertexInputAttributeDescriptions[i_var] = attr_desc;
	}
	std::sort(std::begin(pipelineData.vertexInputAttributeDescriptions), std::end(pipelineData.vertexInputAttributeDescriptions),
		[](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b)
		{
			return a.location < b.location;
		}
	);

	pipelineData.vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelineData.vertexInputInfo.vertexBindingDescriptionCount = 1;
	pipelineData.vertexInputInfo.pVertexBindingDescriptions = &pipelineData.vertexInputBindingDescription;
	pipelineData.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(pipelineData.vertexInputAttributeDescriptions.size());
	pipelineData.vertexInputInfo.pVertexAttributeDescriptions = pipelineData.vertexInputAttributeDescriptions.data();
}

void Shader::checkAttachmentOutputState(PipelineData& pipelineData)
{
	ShaderModuleWrapper fragmentShaderWrapper;
	bool containsFragmentShader = false;
	for (size_t i = 0; i < pipelineData.shaderModuleWrappers.size(); i++)
	{
		if (pipelineData.shaderModuleWrappers[i].stage == VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			fragmentShaderWrapper = pipelineData.shaderModuleWrappers[i];
			containsFragmentShader = true;
			break;
		}
	}
	if(!containsFragmentShader)
		throw(std::runtime_error("Failed to find fragment shader."));

	uint32_t ioutputCount = 0;
	SpvReflectResult result = spvReflectEnumerateOutputVariables(&fragmentShaderWrapper.reflectModule, &ioutputCount, NULL);
	if (result != SPV_REFLECT_RESULT_SUCCESS)
		throw(std::runtime_error("Failed to enumerate shader input variables."));

	std::vector<SpvReflectInterfaceVariable*> output_vars(ioutputCount);
	result = spvReflectEnumerateOutputVariables(&fragmentShaderWrapper.reflectModule, &ioutputCount, output_vars.data());
	if (result != SPV_REFLECT_RESULT_SUCCESS)
		throw(std::runtime_error("Failed to enumerate shader input variables."));


	auto& colorAttachments = *this->m_renderPass->getColorAttachmentIndexMap(m_shaderSettings.subpass);
	for (size_t i_var = 0; i_var < output_vars.size(); ++i_var)
	{
		const SpvReflectInterfaceVariable& refl_var = *(output_vars[i_var]);
		if (refl_var.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) continue;

		auto iterator = colorAttachments.find(refl_var.name);
		if(iterator == std::end(colorAttachments) || iterator->second != refl_var.location)
			throw(std::runtime_error("Failed to find right output attachment."));

		m_attachmentNames.emplace_back(refl_var.name);
	}
}

void Shader::populateGraphicPipelineSettings(PipelineData& pipelineData)
{
	pipelineData.inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineData.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineData.inputAssembly.primitiveRestartEnable = VK_FALSE;

	pipelineData.viewport.x = 0.0f;
	pipelineData.viewport.y = 0.0f;
	pipelineData.viewport.width = 100;
	pipelineData.viewport.height = 100;
	pipelineData.viewport.minDepth = 0.0f;
	pipelineData.viewport.maxDepth = 1.0f;

	pipelineData.scissor.offset = { 0, 0 };
	pipelineData.scissor.extent = { 100, 100 };

	pipelineData.viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineData.viewportState.viewportCount = 1;
	pipelineData.viewportState.pViewports = &pipelineData.viewport;
	pipelineData.viewportState.scissorCount = 1;
	pipelineData.viewportState.pScissors = &pipelineData.scissor;

	pipelineData.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineData.rasterizer.depthClampEnable = VK_FALSE;
	pipelineData.rasterizer.rasterizerDiscardEnable = VK_FALSE;
	pipelineData.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineData.rasterizer.lineWidth = 1.0f;
	pipelineData.rasterizer.cullMode = m_shaderSettings.cullMode;
	pipelineData.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineData.rasterizer.depthBiasEnable = VK_FALSE;

	pipelineData.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineData.multisampling.sampleShadingEnable = VK_FALSE;
	pipelineData.multisampling.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;

	pipelineData.depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineData.depthStencil.depthTestEnable = m_shaderSettings.depthTestEnable;
	pipelineData.depthStencil.depthWriteEnable = m_shaderSettings.depthWriteEnable;
	pipelineData.depthStencil.depthCompareOp = m_shaderSettings.depthCompareOp;
	pipelineData.depthStencil.depthBoundsTestEnable = VK_FALSE;
	pipelineData.depthStencil.stencilTestEnable = VK_FALSE;

	pipelineData.colorBlendAttachment.blendEnable = m_shaderSettings.blendEnable;
	pipelineData.colorBlendAttachment.srcColorBlendFactor = m_shaderSettings.srcColorBlendFactor;
	pipelineData.colorBlendAttachment.dstColorBlendFactor = m_shaderSettings.dstColorBlendFactor;
	pipelineData.colorBlendAttachment.colorBlendOp = m_shaderSettings.colorBlendOp;
	pipelineData.colorBlendAttachment.srcAlphaBlendFactor = m_shaderSettings.srcAlphaBlendFactor;
	pipelineData.colorBlendAttachment.dstAlphaBlendFactor = m_shaderSettings.dstAlphaBlendFactor;
	pipelineData.colorBlendAttachment.alphaBlendOp = m_shaderSettings.alphaBlendOp;
	pipelineData.colorBlendAttachment.colorWriteMask = m_shaderSettings.colorWriteMask;
	pipelineData.pColorBlendAttachments = std::vector< VkPipelineColorBlendAttachmentState>(m_attachmentNames.size(), pipelineData.colorBlendAttachment);

	pipelineData.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineData.colorBlending.logicOpEnable = VK_FALSE;
	pipelineData.colorBlending.logicOp = VK_LOGIC_OP_COPY;
	pipelineData.colorBlending.attachmentCount = static_cast<uint32_t>(m_attachmentNames.size());
	pipelineData.colorBlending.pAttachments = pipelineData.pColorBlendAttachments.data();
	pipelineData.colorBlending.blendConstants[0] = 0.0f;
	pipelineData.colorBlending.blendConstants[1] = 0.0f;
	pipelineData.colorBlending.blendConstants[2] = 0.0f;
	pipelineData.colorBlending.blendConstants[3] = 0.0f;
}

void Shader::createDescriptorLayouts(PipelineData& pipelineData)
{
	std::map<uint32_t, SlotDescriptor> slotLayoutMap = std::map<uint32_t, SlotDescriptor>();
	std::map<uint32_t, std::map<uint32_t, VkDescriptorSetLayoutBinding>> setBindings = std::map<uint32_t, std::map<uint32_t, VkDescriptorSetLayoutBinding>>();
	for (const auto& shaderModuleWrapper : pipelineData.shaderModuleWrappers)
	{
		///push_constant
		{
			uint32_t pushConstantSize = 0;
			SpvReflectResult result = spvReflectEnumeratePushConstantBlocks(&shaderModuleWrapper.reflectModule, &pushConstantSize, nullptr);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);
			std::vector< SpvReflectBlockVariable*> blockVariables = std::vector< SpvReflectBlockVariable*>(pushConstantSize, nullptr);
			result = spvReflectEnumeratePushConstantBlocks(&shaderModuleWrapper.reflectModule, &pushConstantSize, blockVariables.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			for (auto blockVariable : blockVariables)
			{
				VkPushConstantRange range{};
				range.stageFlags = shaderModuleWrapper.stage;
				range.offset = blockVariable->offset;
				range.size = blockVariable->size;

				pipelineData.pushConstantRanges.push_back(range);
			}
		}

		uint32_t count = 0;
		SpvReflectResult result = spvReflectEnumerateDescriptorSets(&shaderModuleWrapper.reflectModule, &count, NULL);
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		std::vector<SpvReflectDescriptorSet*> reflectSets(count);
		result = spvReflectEnumerateDescriptorSets(&shaderModuleWrapper.reflectModule, &count, reflectSets.data());
		assert(result == SPV_REFLECT_RESULT_SUCCESS);
		for (size_t i_set = 0; i_set < reflectSets.size(); ++i_set)
		{
			const SpvReflectDescriptorSet& refl_set = *(reflectSets[i_set]);

			if (!setBindings.count(refl_set.set))
			{
				setBindings.emplace(refl_set.set, std::map<uint32_t, VkDescriptorSetLayoutBinding>());
			}
			std::map<uint32_t, VkDescriptorSetLayoutBinding>& bindings = setBindings[refl_set.set];

			for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding)
			{
				const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);

				if (bindings.count(refl_binding.binding))
				{
					bindings[refl_binding.binding].stageFlags |= static_cast<VkShaderStageFlagBits>(shaderModuleWrapper.reflectModule.shader_stage);
				}
				else
				{
					VkDescriptorSetLayoutBinding layout_binding = {};
					layout_binding.binding = refl_binding.binding;
					layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
					layout_binding.descriptorCount = 1;
					for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
					{
						layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
					}
					layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(shaderModuleWrapper.reflectModule.shader_stage);

					bindings[refl_binding.binding] = layout_binding;

					if (refl_binding.binding == 0)
					{
						SlotDescriptor newSlotLayout = SlotDescriptor();
						newSlotLayout.name = refl_binding.name;
						newSlotLayout.setIndex = refl_set.set;
						newSlotLayout.isArray = refl_binding.array.dims_count > 0;
						if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
						{
							newSlotLayout.slotType = ShaderSlotType::UNIFORM_BUFFER;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)
						{
							newSlotLayout.slotType = ShaderSlotType::STORAGE_BUFFER;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
						{
							newSlotLayout.slotType = ShaderSlotType::UNIFORM_TEXEL_BUFFER;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
						{
							newSlotLayout.slotType = ShaderSlotType::STORAGE_TEXEL_BUFFER;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && refl_binding.image.dim == SpvDim::SpvDim2D)
						{
							newSlotLayout.slotType = ShaderSlotType::TEXTURE2D;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE && refl_binding.image.dim == SpvDim::SpvDim2D)
						{
							newSlotLayout.slotType = ShaderSlotType::STORAGE_TEXTURE2D;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && refl_binding.image.dim == SpvDim::SpvDimCube)
						{
							newSlotLayout.slotType = ShaderSlotType::TEXTURE_CUBE;
						}
						else if (refl_binding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT && refl_binding.image.dim == SpvDim::SpvDim2D)
						{
							newSlotLayout.slotType = ShaderSlotType::INPUT_ATTACHMENT;
						}
						else
						{
							throw(std::runtime_error("Failed to find right output attachment."));
						}

						slotLayoutMap.emplace(refl_set.set, newSlotLayout);
					}

				}
			}

		}

	}

	for (auto& setBindingPair : setBindings)
	{
		std::vector< VkDescriptorSetLayoutBinding> bindings = std::vector< VkDescriptorSetLayoutBinding>(setBindingPair.second.size());
		SlotDescriptor& slotDescriptor = slotLayoutMap[setBindingPair.first];
		slotDescriptor.vkDescriptorTypes.resize(setBindingPair.second.size());

		for (size_t i = 0; i < bindings.size(); i++)
		{
			bindings[i] = setBindingPair.second[static_cast<uint32_t>(i)];
			slotDescriptor.vkDescriptorTypes[i] = setBindingPair.second[static_cast<uint32_t>(i)].descriptorType;
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if(vkCreateDescriptorSetLayout(Instance::getDevice(), &layoutInfo, nullptr, &slotDescriptor.vkDescriptorSetLayout) != VK_SUCCESS)
			throw(std::runtime_error("Failed to create descriptor set layout."));

		{

			const auto& binding = setBindingPair.second[static_cast<uint32_t>(0)];
			if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::UNIFORM_BUFFER;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::STORAGE_BUFFER;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::STORAGE_TEXEL_BUFFER;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::UNIFORM_TEXEL_BUFFER;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && slotDescriptor.slotType == ShaderSlotType::TEXTURE2D && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::TEXTURE2D;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && slotDescriptor.slotType == ShaderSlotType::TEXTURE2D && setBindingPair.second.size() == 2 && setBindingPair.second[1].descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				slotDescriptor.slotType = ShaderSlotType::TEXTURE2D_WITH_INFO;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && slotDescriptor.slotType == ShaderSlotType::STORAGE_TEXTURE2D && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::STORAGE_TEXTURE2D;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && slotDescriptor.slotType == ShaderSlotType::STORAGE_TEXTURE2D && setBindingPair.second.size() == 2 && setBindingPair.second[1].descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				slotDescriptor.slotType = ShaderSlotType::STORAGE_TEXTURE2D_WITH_INFO;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && slotDescriptor.slotType == ShaderSlotType::TEXTURE_CUBE && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::TEXTURE_CUBE;
			}
			else if (binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT && slotDescriptor.slotType == ShaderSlotType::INPUT_ATTACHMENT && setBindingPair.second.size() == 1)
			{
				slotDescriptor.slotType = ShaderSlotType::INPUT_ATTACHMENT;
			}
			else
			{
				throw(std::runtime_error("Failed to parse descriptor type."));
			}
		}
		m_slotDescriptors.emplace(slotDescriptor.name, slotDescriptor);
	}
}

void Shader::populateDescriptorLayouts(PipelineData& pipelineData)
{
	pipelineData.descriptorSetLayouts.resize(m_slotDescriptors.size());
	for (auto& slotDescriptorPair : m_slotDescriptors)
	{
		pipelineData.descriptorSetLayouts[slotDescriptorPair.second.setIndex] = slotDescriptorPair.second.vkDescriptorSetLayout;
	}
}

void Shader::createGraphicPipeline(PipelineData& pipelineData)
{
	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT, 				//ÉèÖÃ¶¯Ì¬×´Ì¬  ÊÓ¿Ú¾ØÕó
		VK_DYNAMIC_STATE_SCISSOR, 				//²Ã¼ô¾ØÕó
	};
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
	pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	pipelineDynamicStateCreateInfo.flags = 0;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pipelineData.descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = pipelineData.descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pipelineData.pushConstantRanges.size());
	pipelineLayoutInfo.pPushConstantRanges = pipelineData.pushConstantRanges.data();

	if(vkCreatePipelineLayout(Instance::getDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout)!= VK_SUCCESS)
		throw(std::runtime_error("Failed to create pipeline layout."));

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(pipelineData.stageInfos.size());
	pipelineInfo.pStages = pipelineData.stageInfos.data();
	pipelineInfo.pVertexInputState = &pipelineData.vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &pipelineData.inputAssembly;
	pipelineInfo.pViewportState = &pipelineData.viewportState;
	pipelineInfo.pRasterizationState = &pipelineData.rasterizer;
	pipelineInfo.pMultisampleState = &pipelineData.multisampling;
	pipelineInfo.pDepthStencilState = &pipelineData.depthStencil;
	pipelineInfo.pColorBlendState = &pipelineData.colorBlending;
	pipelineInfo.layout = m_PipelineLayout;
	pipelineInfo.renderPass = this->m_renderPass->getRenderPass();
	pipelineInfo.subpass = this->m_renderPass->getSubPassIndex(m_shaderSettings.subpass);
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.pDynamicState = &pipelineDynamicStateCreateInfo;

	if (vkCreateGraphicsPipelines(Instance::getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create pipeline."));
}

void Shader::createComputePipeline(PipelineData& pipelineData)
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pipelineData.descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = pipelineData.descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pipelineData.pushConstantRanges.size());
	pipelineLayoutInfo.pPushConstantRanges = pipelineData.pushConstantRanges.data();

	if (vkCreatePipelineLayout(Instance::getDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create pipeline layout."));

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.flags = 0;
	pipelineInfo.stage = pipelineData.stageInfos[0];
	pipelineInfo.layout = m_PipelineLayout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	if (vkCreateComputePipelines(Instance::getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		throw(std::runtime_error("Failed to create pipeline."));
}

void Shader::destroyData(PipelineData& pipelineData)
{
	for (auto& warp : pipelineData.shaderModuleWrappers)
	{
		vkDestroyShaderModule(Instance::getDevice(), warp.shaderModule, nullptr);
		spvReflectDestroyShaderModule(&warp.reflectModule);
	}
}

void Shader::onLoad(CommandBuffer* transferCommandBuffer)
{
	PipelineData pipelineData = PipelineData();

	parseShaderData(pipelineData);

	loadSpirvs(pipelineData);

	createShaderModules(pipelineData);

	populateShaderStages(pipelineData);

	if (pipelineData.stageInfos.size() == 1 && pipelineData.stageInfos[0].stage == VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT)
	{
		m_shaderType = ShaderType::COMPUTE;

		createDescriptorLayouts(pipelineData);
		populateDescriptorLayouts(pipelineData);

		createComputePipeline(pipelineData);
	}
	else
	{
		m_shaderType = ShaderType::GRAPHIC;

		this->m_renderPass = Instance::getRenderPassManager().loadRenderPass(this->m_shaderSettings.renderPass);

		populateVertexInputState(pipelineData);
		checkAttachmentOutputState(pipelineData);

		createDescriptorLayouts(pipelineData);
		populateDescriptorLayouts(pipelineData);

		populateGraphicPipelineSettings(pipelineData);
		createGraphicPipeline(pipelineData);
	}

	destroyData(pipelineData);
}

const std::map<std::string, Shader::SlotDescriptor>* Shader::getSlotDescriptors()
{
	return &m_slotDescriptors;
}

VkPipeline& Shader::getPipeline()
{
	return m_Pipeline;
}

VkPipelineLayout& Shader::getPipelineLayout()
{
	return m_PipelineLayout;
}

const Shader::ShaderSettings* Shader::getSettings()
{
	return &m_shaderSettings;
}

Shader::ShaderType Shader::getShaderType()
{
	return m_shaderType;
}

Shader::Shader()
	: AssetBase(true)
	, m_shaderSettings()
	, m_slotDescriptors()
	, m_Pipeline(VK_NULL_HANDLE)
	, m_PipelineLayout(VK_NULL_HANDLE)
	, m_attachmentNames()
	, m_renderPass(nullptr)
{
}

Shader::~Shader()
{
	if (m_shaderType == ShaderType::GRAPHIC)
	{
		Instance::getRenderPassManager().unloadRenderPass(this->m_shaderSettings.renderPass);
	}

	vkDestroyPipeline(Instance::getDevice(), m_Pipeline, nullptr);
	vkDestroyPipelineLayout(Instance::getDevice(), m_PipelineLayout, nullptr);

	for (const auto& slotDescriptorPair : m_slotDescriptors)
	{
		vkDestroyDescriptorSetLayout(Instance::getDevice(), slotDescriptorPair.second.vkDescriptorSetLayout, nullptr);
	}

}
