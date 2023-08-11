#pragma once

#include "X2/Vulkan/VulkanTexture.h"

#include <filesystem>

namespace X2 {

	class TextureImporter
	{
	public:
		static Buffer ToBufferFromFile(const std::filesystem::path& path, ImageFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
		static Buffer ToBufferFromMemory(Buffer buffer, ImageFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
	private:
		const std::filesystem::path m_Path;
	};
}
