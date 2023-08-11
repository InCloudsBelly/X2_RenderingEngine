#pragma once

#include "X2/Asset/Asset.h"
#include "X2/Asset/AssetSerializer.h"

#include "X2/Serialization/AssetPack.h"
#include "X2/Serialization/AssetPackFile.h"
#include "X2/Serialization/FileStream.h"

namespace X2 {

	class MeshRuntimeSerializer
	{
	public:
		bool SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo);
		Ref<Asset> DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo);
	};

}
