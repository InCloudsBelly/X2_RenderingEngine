#include "Precompiled.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "MeshSerializer.h"
//#include "GraphSerializer.h"


namespace X2 {

	void AssetImporter::Init()
	{
		s_Serializers.clear();
		s_Serializers[AssetType::Prefab] = CreateScope<PrefabSerializer>();
		s_Serializers[AssetType::Texture] = CreateScope<TextureSerializer>();
		s_Serializers[AssetType::Mesh] = CreateScope<MeshSerializer>();
		s_Serializers[AssetType::StaticMesh] = CreateScope<StaticMeshSerializer>();
		s_Serializers[AssetType::MeshSource] = CreateScope<MeshSourceSerializer>();
		s_Serializers[AssetType::Material] = CreateScope<MaterialAssetSerializer>();
		s_Serializers[AssetType::EnvMap] = CreateScope<EnvironmentSerializer>();
		s_Serializers[AssetType::Scene] = CreateScope<SceneAssetSerializer>();
		s_Serializers[AssetType::Font] = CreateScope<FontSerializer>();
	}

	void AssetImporter::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset)
	{
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			X2_CORE_WARN("There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return;
		}

		s_Serializers[asset->GetAssetType()]->Serialize(metadata, asset);
	}

	void AssetImporter::Serialize(const Ref<Asset>& asset)
	{
		const AssetMetadata& metadata = Project::GetEditorAssetManager()->GetMetadata(asset->Handle);
		Serialize(metadata, asset);
	}

	bool AssetImporter::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset)
	{
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			X2_CORE_WARN("There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return false;
		}

		// X2_CORE_TRACE("AssetImporter::TryLoadData - {}", metadata.FilePath);
		return s_Serializers[metadata.Type]->TryLoadData(metadata, asset);
	}

	bool AssetImporter::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo)
	{
		if (!AssetManager::IsAssetHandleValid(handle))
			return false;

		const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			X2_CORE_WARN("There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return false;
		}

		return s_Serializers[metadata.Type]->SerializeToAssetPack(handle, stream, outInfo);
	}

	Ref<Asset> AssetImporter::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo)
	{
		AssetType assetType = (AssetType)assetInfo.Type;
		if (s_Serializers.find(assetType) == s_Serializers.end())
			return nullptr;

		return s_Serializers[assetType]->DeserializeFromAssetPack(stream, assetInfo);
	}

	Ref<Scene> AssetImporter::DeserializeSceneFromAssetPack(FileStreamReader& stream, const AssetPackFile::SceneInfo& sceneInfo)
	{
		AssetType assetType = AssetType::Scene;
		if (s_Serializers.find(assetType) == s_Serializers.end())
			return nullptr;

		SceneAssetSerializer* sceneAssetSerializer = (SceneAssetSerializer*)s_Serializers[assetType].get();
		return sceneAssetSerializer->DeserializeSceneFromAssetPack(stream, sceneInfo);
	}

	std::unordered_map<AssetType, Scope<AssetSerializer>> AssetImporter::s_Serializers;

}
