#pragma once

#include "Scene.h"

#include "X2/Serialization/FileStream.h"
#include "X2/Asset/AssetSerializer.h"

namespace YAML {
	class Emitter;
	class Node;
}

namespace X2 {

	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void Serialize(const std::filesystem::path& filepath);
		void SerializeToYAML(YAML::Emitter& out);
		bool DeserializeFromYAML(const std::string& yamlString);
		void SerializeRuntime(const std::filesystem::path& filepath);

		bool Deserialize(const std::filesystem::path& filepath);
		bool DeserializeRuntime(const std::filesystem::path& filepath);

		bool SerializeToAssetPack(FileStreamWriter& stream, AssetSerializationInfo& outInfo);
		bool DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::SceneInfo& sceneInfo);

		bool DeserializeReferencedPrefabs(const std::filesystem::path& filepath, std::unordered_set<AssetHandle>& outPrefabs);
	public:
		static void SerializeEntity(YAML::Emitter& out, Entity entity);
		static void DeserializeEntities(YAML::Node& entitiesNode, Ref<Scene> scene);
	public:
		inline static std::string_view FileFilter = "X2 Scene (*.hscene)\0*.hscene\0";
		inline static std::string_view DefaultExtension = ".hscene";

	private:
		Ref<Scene> m_Scene;
	};

}
