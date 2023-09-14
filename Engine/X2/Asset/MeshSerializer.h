#pragma once

#include "X2/Asset/AssetSerializer.h"
#include "X2/Serialization/FileStream.h"
#include "X2/Renderer/Mesh.h"

namespace X2 {

	class MeshSourceSerializer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Asset* asset) const override {}
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const override;

		virtual bool SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const;
		virtual Ref<Asset> DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const;
	};

	class MeshSerializer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Asset* asset) const override;
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const override;

		virtual bool SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const;
		virtual Ref<Asset> DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const;
	private:
		std::string SerializeToYAML(Mesh* mesh) const;
		bool DeserializeFromYAML(const std::string& yamlString, Ref<Mesh>& targetMesh) const;
	};

	class StaticMeshSerializer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Asset* asset) const override;
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const override;

		virtual bool SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const;
		virtual Ref<Asset> DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const;
	private:
		std::string SerializeToYAML(StaticMesh* staticMesh) const;
		bool DeserializeFromYAML(const std::string& yamlString, Ref<StaticMesh>& targetStaticMesh) const;
	};

}
