#pragma once

#include "X2/Asset/Asset.h"
#include "X2/Asset/AssetTypes.h"

#include <unordered_set>
#include <unordered_map>

namespace X2 {

	//////////////////////////////////////////////////////////////////
	// AssetManagerBase //////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////
	// Implementation in RuntimeAssetManager and EditorAssetManager //
	// Static wrapper in AssetManager ////////////////////////////////
	//////////////////////////////////////////////////////////////////
	class AssetManagerBase 
	{
	public:
		AssetManagerBase() = default;
		virtual ~AssetManagerBase() = default;

		virtual AssetType GetAssetType(AssetHandle assetHandle) = 0;
		virtual Ref<Asset> GetAsset(AssetHandle assetHandle) = 0;
		virtual void AddMemoryOnlyAsset(Ref<Asset> asset) = 0;
		virtual bool ReloadData(AssetHandle assetHandle) = 0;
		virtual bool IsAssetHandleValid(AssetHandle assetHandle) = 0;
		virtual bool IsMemoryAsset(AssetHandle handle) = 0;
		virtual bool IsAssetLoaded(AssetHandle handle) = 0;

		virtual std::unordered_set<AssetHandle> GetAllAssetsWithType(AssetType type) = 0;
		virtual const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() = 0;
		virtual const std::unordered_map<AssetHandle, Ref<Asset>>& GetMemoryOnlyAssets() = 0;
	};

}
