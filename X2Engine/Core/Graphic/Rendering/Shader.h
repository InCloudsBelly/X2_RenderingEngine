#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include <map>
#include "Utils/SpirvReflect.h"
#include "Core/IO/Asset/AssetBase.h"
#include "glm/glm.hpp"


class RenderPassBase;
enum class ShaderSlotType
{
	UNIFORM_BUFFER,
	STORAGE_BUFFER,
	UNIFORM_TEXEL_BUFFER,
	STORAGE_TEXEL_BUFFER,
	TEXTURE2D,
	TEXTURE2D_WITH_INFO,
	STORAGE_TEXTURE2D,
	STORAGE_TEXTURE2D_WITH_INFO,
	TEXTURE_CUBE,
	INPUT_ATTACHMENT
};
class Shader final : public AssetBase
{
public:
	enum class ShaderType
	{
		GRAPHIC,
		COMPUTE,
	};
	struct SlotDescriptor
	{
		std::string name{};
		bool isArray{};
		uint32_t setIndex{};
		ShaderSlotType slotType{};
		VkDescriptorSetLayout vkDescriptorSetLayout{};
		std::vector<VkDescriptorType> vkDescriptorTypes{};
	};
	struct ShaderSettings
	{
		std::string renderPass{};
		std::string subpass{};
		std::vector<std::string> shaderPaths{};
		VkCullModeFlags cullMode{};
		VkBool32 blendEnable{};
		VkBlendFactor srcColorBlendFactor{};
		VkBlendFactor dstColorBlendFactor{};
		VkBlendOp colorBlendOp{};
		VkBlendFactor srcAlphaBlendFactor{};
		VkBlendFactor dstAlphaBlendFactor{};
		VkBlendOp alphaBlendOp{};
		VkColorComponentFlags colorWriteMask{};
		VkBool32 depthTestEnable{};
		VkBool32 depthWriteEnable{};
		VkCompareOp depthCompareOp{};

		NLOHMANN_DEFINE_TYPE_INTRUSIVE
		(
			ShaderSettings,
			shaderPaths,
			cullMode,
			blendEnable,
			srcColorBlendFactor,
			dstColorBlendFactor,
			colorBlendOp,
			srcAlphaBlendFactor,
			dstAlphaBlendFactor,
			alphaBlendOp,
			colorWriteMask,
			renderPass,
			subpass,
			depthTestEnable,
			depthWriteEnable,
			depthCompareOp
		);
	};
private:
	struct VertexData
	{
		glm::vec3 position{};
		glm::vec2 texCoords{};
		glm::vec3 normal{};
		glm::vec3 tangent{};
		glm::vec3 bitangent{};
	};

	struct ShaderModuleWrapper
	{
		VkShaderStageFlagBits stage{};
		VkShaderModule shaderModule{};
		SpvReflectShaderModule reflectModule{};
	};
	struct PipelineData
	{
		std::map<std::string, std::vector<char>> spirvs{};
		std::vector<ShaderModuleWrapper> shaderModuleWrappers{};
		std::vector< VkPipelineShaderStageCreateInfo> stageInfos{};
		std::vector< VkPushConstantRange> pushConstantRanges{};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		VkVertexInputBindingDescription vertexInputBindingDescription{};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions{};

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		VkViewport viewport{};
		VkRect2D scissor{};
		VkPipelineViewportStateCreateInfo viewportState{};
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		VkPipelineMultisampleStateCreateInfo multisampling{};
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		std::vector< VkPipelineColorBlendAttachmentState> pColorBlendAttachments;
		VkPipelineColorBlendStateCreateInfo colorBlending{};

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{};

	};
private:
	ShaderSettings m_shaderSettings;
	std::map<std::string, SlotDescriptor> m_slotDescriptors;
	VkPipeline m_Pipeline;
	VkPipelineLayout m_PipelineLayout;
	ShaderType m_shaderType;
	std::vector<std::string> m_attachmentNames;
	RenderPassBase* m_renderPass;

	void parseShaderData(PipelineData& pipelineData);
	void loadSpirvs(PipelineData& pipelineData);
	void createShaderModules(PipelineData& pipelineData);
	void populateShaderStages(PipelineData& pipelineData);
	void populateVertexInputState(PipelineData& pipelineData);
	void checkAttachmentOutputState(PipelineData& pipelineData);
	void populateGraphicPipelineSettings(PipelineData& pipelineData);
	void createDescriptorLayouts(PipelineData& pipelineData);
	void populateDescriptorLayouts(PipelineData& pipelineData);
	void createGraphicPipeline(PipelineData& pipelineData);
	void createComputePipeline(PipelineData& pipelineData);
	void destroyData(PipelineData& pipelineData);
	
	void onLoad(CommandBuffer* transferCommandBuffer);
public:

	const std::map<std::string, SlotDescriptor>* getSlotDescriptors();
	VkPipeline& getPipeline();
	VkPipelineLayout& getPipelineLayout();
	const ShaderSettings* getSettings();
	ShaderType getShaderType();
	Shader();
	~Shader();
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&&) = delete;
	Shader& operator=(Shader&&) = delete;
};
