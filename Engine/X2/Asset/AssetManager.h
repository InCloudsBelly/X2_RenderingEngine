#pragma once

#include "X2/Project/Project.h"

#include "X2/Utilities/FileSystem.h"
#include "X2/Utilities/StringUtils.h"

#include "X2/Core/Hash.h"
#include "X2/Core/Application.h"
#include "X2/Core/Timer.h"

#include "AssetImporter.h"
#include "AssetRegistry.h"


#include <map>
#include <unordered_map>
#include <unordered_set>

namespace X2 {

	class AssetManager
	{
	public:
		using AssetsChangeEventFn = std::function<void(const std::vector<FileSystemChangedEvent>&)>;
	public:
		static bool IsAssetHandleValid(AssetHandle assetHandle) { return Project::GetAssetManager()->IsAssetHandleValid(assetHandle); }

		static bool ReloadData(AssetHandle assetHandle) { return Project::GetAssetManager()->ReloadData(assetHandle); }

		static AssetType GetAssetType(AssetHandle assetHandle) { return Project::GetAssetManager()->GetAssetType(assetHandle); }

		template<typename T>
		static Ref<T> GetAsset(AssetHandle assetHandle)
		{
			//static std::mutex mutex;
			//std::scoped_lock<std::mutex> lock(mutex);

			Ref<Asset> asset = Project::GetAssetManager()->GetAsset(assetHandle);
			return std::dynamic_pointer_cast<T>(asset);
		}

		template<typename T>
		static std::unordered_set<AssetHandle> GetAllAssetsWithType()
		{
			return Project::GetAssetManager()->GetAllAssetsWithType(T::GetStaticType());
		}

		static const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() { return Project::GetAssetManager()->GetLoadedAssets(); }
		static const std::unordered_map<AssetHandle, Ref<Asset>>& GetMemoryOnlyAssets() { return Project::GetAssetManager()->GetMemoryOnlyAssets(); }

		template<typename TAsset, typename... TArgs>
		static AssetHandle CreateMemoryOnlyAsset(TArgs&&... args)
		{
			static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

			Ref<TAsset> asset = CreateRef<TAsset>(std::forward<TArgs>(args)...);
			asset->Handle = AssetHandle(); // NOTE(Yan): should handle generation happen here?

			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}

		template<typename TAsset, typename... TArgs>
		static AssetHandle CreateMemoryOnlyRendererAsset(TArgs&&... args)
		{
			static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

			Ref<TAsset> asset = CreateRef<TAsset>(std::forward<TArgs>(args)...);
			asset->Handle = AssetHandle();

			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}

		template<typename TAsset, typename... TArgs>
		static Ref<TAsset> CreateMemoryOnlyAssetReturnAsset(TArgs&&... args)
		{
			static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

			Ref<TAsset> asset = CreateRef<TAsset>(std::forward<TArgs>(args)...);
			asset->Handle = AssetHandle();

			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset;
		}

		template<typename TAsset, typename... TArgs>
		static AssetHandle CreateMemoryOnlyAssetWithHandle(AssetHandle handle, TArgs&&... args)
		{
			static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

			Ref<TAsset> asset = CreateRef<TAsset>(std::forward<TArgs>(args)...);
			asset->Handle = handle;

			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}

		template<typename TAsset>
		static AssetHandle AddMemoryOnlyAsset(Ref<TAsset> asset)
		{
			static_assert(std::is_base_of<Asset, TAsset>::value, "AddMemoryOnlyAsset only works for types derived from Asset");
			asset->Handle = AssetHandle(); // NOTE(Yan): should handle generation happen here?

			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}

		static bool IsMemoryAsset(AssetHandle handle) { return Project::GetAssetManager()->IsMemoryAsset(handle); }
	};

}
