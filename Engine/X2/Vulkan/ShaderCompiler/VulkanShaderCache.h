#pragma once

#include <filesystem>
#include <map>

#include "VulkanShaderCompiler.h"

namespace X2 {

	class VulkanShaderCache
	{
	public:
		static VkShaderStageFlagBits HasChanged(VulkanShaderCompiler* shader);
	private:
		static void Serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache);
		static void Deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache);
	};

}
