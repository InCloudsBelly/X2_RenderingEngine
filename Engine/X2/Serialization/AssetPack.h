#pragma once

#include <filesystem>
#include <map>

#include "X2/Core/UUID.h"
#include "X2/Asset/Asset.h"
#include "X2/Scene/Scene.h"

#include "AssetPackSerializer.h"
#include "AssetPackFile.h"

namespace X2 {

	class AssetPack 
	{
	public:
		AssetPack() = default;
		AssetPack(const std::filesystem::path& path);

		void AddAsset(Ref<Asset> asset);

		void Serialize();

		Ref<Scene> LoadScene(AssetHandle sceneHandle);
		Ref<Asset> LoadAsset(AssetHandle sceneHandle, AssetHandle assetHandle);

		bool IsAssetHandleValid(AssetHandle assetHandle) const;
		bool IsAssetHandleValid(AssetHandle sceneHandle, AssetHandle assetHandle) const;

		Buffer ReadAppBinary();
		uint64_t GetBuildVersion();

		// This will create a complete asset pack from ALL referenced assets
		// in currently active project. This should change in the future to
		// take in a Ref<Project> or something when the AssetManager becomes
		// non-static, but since it is static at the moment this is what we get
		static Ref<AssetPack> CreateFromActiveProject(std::atomic<float>& progress);
		static Ref<AssetPack> Load(const std::filesystem::path& path);
		static Ref<AssetPack> LoadActiveProject();
	private:
		std::filesystem::path m_Path;
		AssetPackFile m_File;

		AssetPackSerializer m_Serializer;

		std::unordered_set<AssetHandle> m_AssetHandleIndex;
	};

}
