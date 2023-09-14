#include "Precompiled.h"
#include "RuntimeAssetManager.h"

#include "X2/Core/Debug/Profiler.h"
#include "X2/Core/Application.h"
#include "X2/Core/Timer.h"

#include "X2/Asset/AssetImporter.h"

namespace X2 {

	RuntimeAssetManager::RuntimeAssetManager()
	{
		AssetImporter::Init();
	}

	RuntimeAssetManager::~RuntimeAssetManager()
	{
	}

	AssetType RuntimeAssetManager::GetAssetType(AssetHandle assetHandle)
	{
		Ref<Asset> asset = GetAsset(assetHandle);
		if (!asset)
			return AssetType::None;

		return asset->GetAssetType();
	}

	Ref<Asset> RuntimeAssetManager::GetAsset(AssetHandle assetHandle)
	{
		X2_PROFILE_FUNC();
		X2_SCOPE_PERF("AssetManager::GetAsset");

		if (IsMemoryAsset(assetHandle))
			return m_MemoryAssets[assetHandle];

		Ref<Asset> asset = nullptr;
		bool isLoaded = IsAssetLoaded(assetHandle);
		if (isLoaded)
		{
			asset = m_LoadedAssets[assetHandle];
		}
		else
		{
			// Needs load
			asset = m_AssetPack->LoadAsset(m_ActiveScene, assetHandle);
			if (!asset)
				return nullptr;

			m_LoadedAssets[assetHandle] = asset;
		}
		return asset;
	}

	void RuntimeAssetManager::AddMemoryOnlyAsset(Ref<Asset> asset)
	{
		m_MemoryAssets[asset->Handle] = asset;
	}

	bool RuntimeAssetManager::ReloadData(AssetHandle assetHandle)
	{
		Ref<Asset> asset = m_AssetPack->LoadAsset(m_ActiveScene, assetHandle);
		if (asset)
			m_LoadedAssets[assetHandle] = asset;

		return asset.get();
	}

	bool RuntimeAssetManager::IsAssetHandleValid(AssetHandle assetHandle)
	{
		if (assetHandle == 0)
			return false;

		return IsMemoryAsset(assetHandle) || (m_AssetPack && m_AssetPack->IsAssetHandleValid(assetHandle));
	}

	bool RuntimeAssetManager::IsMemoryAsset(AssetHandle handle)
	{
		return m_MemoryAssets.find(handle) != m_MemoryAssets.end();
	}

	bool RuntimeAssetManager::IsAssetLoaded(AssetHandle handle)
	{
		return m_LoadedAssets.find(handle) != m_LoadedAssets.end();
	}

	std::unordered_set<AssetHandle> RuntimeAssetManager::GetAllAssetsWithType(AssetType type)
	{
		std::unordered_set<AssetHandle> result;
		X2_CORE_VERIFY(false, "Not implemented");
		return result;
	}

	Ref<Scene> RuntimeAssetManager::LoadScene(AssetHandle handle)
	{
		Ref<Scene> scene = m_AssetPack->LoadScene(handle);
		if (scene)
			m_ActiveScene = handle;

		return scene;
	}

}
