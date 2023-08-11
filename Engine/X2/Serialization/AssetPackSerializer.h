#pragma once
#include <filesystem>

#include "AssetPackFile.h"
#include "X2/Core/Buffer.h"

namespace X2 {

	class AssetPackSerializer
	{
	public:
		static void Serialize(const std::filesystem::path& path, AssetPackFile& file, Buffer appBinary, std::atomic<float>& progress);
		static bool DeserializeIndex(const std::filesystem::path& path, AssetPackFile& file);
	private:
		static uint64_t CalculateIndexTableSize(const AssetPackFile& file);
	};

}
