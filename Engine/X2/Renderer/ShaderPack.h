#pragma once

#include "X2/Vulkan/VulkanShader.h"

#include "X2/Serialization/Serialization.h"
#include "X2/Serialization/ShaderPackFile.h"

#include <filesystem>

namespace X2 {

	class ShaderPack : public RefCounted
	{
	public:
		ShaderPack() = default;
		ShaderPack(const std::filesystem::path& path);

		bool IsLoaded() const { return m_Loaded; }
		bool Contains(std::string_view name) const;

		Ref<VulkanShader> LoadShader(std::string_view name);

		static Ref<ShaderPack> CreateFromLibrary(Ref<ShaderLibrary> shaderLibrary, const std::filesystem::path& path);
	private:
		bool m_Loaded = false;
		ShaderPackFile m_File;
		std::filesystem::path m_Path;
	};

}
