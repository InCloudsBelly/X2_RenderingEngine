#pragma once

#include "X2/Serialization/FileStream.h"

#include "X2/Vulkan/VulkanTexture.h"

namespace X2 {

	class TextureRuntimeSerializer
	{
	public:
		struct Texture2DMetadata
		{
			uint32_t Width, Height;
			uint16_t Format;
			uint8_t Mips;
		};
	public:
		static uint64_t SerializeToFile(Ref<VulkanTextureCube> textureCube, FileStreamWriter& stream);
		static Ref<VulkanTextureCube> DeserializeTextureCube(FileStreamReader& stream);

		static uint64_t SerializeTexture2DToFile(const std::filesystem::path& filepath, FileStreamWriter& stream);
		static uint64_t SerializeTexture2DToFile(Ref<VulkanTexture2D> texture, FileStreamWriter& stream);
		static uint64_t SerializeTexture2DToFile(Buffer imageBuffer, const Texture2DMetadata& metadata, FileStreamWriter& stream);
		static Ref<VulkanTexture2D> DeserializeTexture2D(FileStreamReader& stream);
	};

}
