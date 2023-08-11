#include "Precompiled.h"
#include "AssetSerializer.h"

#include "AssetManager.h"
#include "TextureRuntimeSerializer.h"


#include "X2/Scene/Prefab.h"
#include "X2/Scene/SceneSerializer.h"

#include "X2/Renderer/MaterialAsset.h"
#include "X2/Renderer/Mesh.h"
#include "X2/Renderer/Renderer.h"
#include "X2/Renderer/SceneEnvironment.h"
#include "X2/Renderer/UI/Font.h"

#include "X2/Utilities/FileSystem.h"
#include "X2/Utilities/SerializationMacros.h"
#include "X2/Utilities/StringUtils.h"
#include "X2/Utilities/YAMLSerializationHelpers.h"


#include "yaml-cpp/yaml.h"

namespace X2 {

	//////////////////////////////////////////////////////////////////////////////////
	// TextureSerializer
	//////////////////////////////////////////////////////////////////////////////////

	bool TextureSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		asset = Ref<VulkanTexture2D>::Create(TextureSpecification(), Project::GetEditorAssetManager()->GetFileSystemPathString(metadata));
		asset->Handle = metadata.Handle;

		bool result = asset.As<VulkanTexture2D>()->Loaded();

		if (!result)
			asset->SetFlag(AssetFlag::Invalid, true);

		return result;
	}

	bool TextureSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	{
		outInfo.Offset = stream.GetStreamPosition();

		auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
		Ref<VulkanTexture2D> texture = AssetManager::GetAsset<VulkanTexture2D>(handle);
		outInfo.Size = TextureRuntimeSerializer::SerializeTexture2DToFile(texture, stream);
		return true;
	}

	Ref<Asset> TextureSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	{
		X2_CORE_WARN("TextureSerializer::DeserializeFromAssetPack");

		stream.SetStreamPosition(assetInfo.PackedOffset);
		return TextureRuntimeSerializer::DeserializeTexture2D(stream);
	}

	//////////////////////////////////////////////////////////////////////////////////
	// FontSerializer
	//////////////////////////////////////////////////////////////////////////////////

	bool FontSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		asset = Ref<Font>::Create(Project::GetEditorAssetManager()->GetFileSystemPathString(metadata));
		asset->Handle = metadata.Handle;

#if 0
		// TODO(Yan): we should probably handle fonts not loading correctly
		bool result = asset.As<Font>()->Loaded();
		if (!result)
			asset->SetFlag(AssetFlag::Invalid, true);
#endif

		return true;
	}

	bool FontSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	{
		outInfo.Offset = stream.GetStreamPosition();

		Ref<Font> font = AssetManager::GetAsset<Font>(handle);
		auto path = Project::GetEditorAssetManager()->GetFileSystemPath(handle);
		stream.WriteString(font->GetName());
		Buffer fontData = FileSystem::ReadBytes(path);
		stream.WriteBuffer(fontData);

		outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
		return true;
	}

	Ref<Asset> FontSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	{
		stream.SetStreamPosition(assetInfo.PackedOffset);

		std::string name;
		stream.ReadString(name);
		Buffer fontData;
		stream.ReadBuffer(fontData);

		return Ref<Font>::Create(name, fontData);;
	}

	//////////////////////////////////////////////////////////////////////////////////
	// MaterialAssetSerializer
	//////////////////////////////////////////////////////////////////////////////////

	void MaterialAssetSerializer::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset) const
	{
		Ref<MaterialAsset> materialAsset = asset.As<MaterialAsset>();

		std::string yamlString = SerializeToYAML(materialAsset);

		std::ofstream fout(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
		fout << yamlString;
	}

	bool MaterialAssetSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		std::ifstream stream(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
		if (!stream.is_open())
			return false;

		std::stringstream strStream;
		strStream << stream.rdbuf();

		Ref<MaterialAsset> materialAsset;
		bool success = DeserializeFromYAML(strStream.str(), materialAsset);
		if (!success)
			return false;

		asset = materialAsset;
		asset->Handle = metadata.Handle;
		return true;
	}

	bool MaterialAssetSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	{
		Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(handle);

		std::string yamlString = SerializeToYAML(materialAsset);
		outInfo.Offset = stream.GetStreamPosition();
		stream.WriteString(yamlString);
		outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
		return true;
	}

	Ref<Asset> MaterialAssetSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	{
		stream.SetStreamPosition(assetInfo.PackedOffset);
		std::string yamlString;
		stream.ReadString(yamlString);

		Ref<MaterialAsset> materialAsset;
		bool result = DeserializeFromYAML(yamlString, materialAsset);
		if (!result)
			return nullptr;

		return materialAsset;
	}

	std::string MaterialAssetSerializer::SerializeToYAML(Ref<MaterialAsset> materialAsset) const
	{
		YAML::Emitter out;
		out << YAML::BeginMap; // Material
		out << YAML::Key << "Material" << YAML::Value;
		{
			out << YAML::BeginMap;

			// TODO(Yan): this should have shader UUID when that's a thing
			//            right now only supports PBR or Transparent shaders
			Ref<VulkanShader> transparentShader = Renderer::GetShaderLibrary()->Get("PBR_Transparent");
			bool transparent = materialAsset->GetMaterial()->GetShader() == transparentShader;
			X2_SERIALIZE_PROPERTY(Transparent, transparent, out);

			X2_SERIALIZE_PROPERTY(AlbedoColor, materialAsset->GetAlbedoColor(), out);
			X2_SERIALIZE_PROPERTY(Emission, materialAsset->GetEmission(), out);
			if (!transparent)
			{
				X2_SERIALIZE_PROPERTY(UseNormalMap, materialAsset->IsUsingNormalMap(), out);
				X2_SERIALIZE_PROPERTY(Metalness, materialAsset->GetMetalness(), out);
				X2_SERIALIZE_PROPERTY(Roughness, materialAsset->GetRoughness(), out);
			}
			else
			{
				X2_SERIALIZE_PROPERTY(Transparency, materialAsset->GetTransparency(), out);
			}

			{
				Ref<VulkanTexture2D> albedoMap = materialAsset->GetAlbedoMap();
				bool hasAlbedoMap = albedoMap ? !albedoMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
				AssetHandle albedoMapHandle = hasAlbedoMap ? albedoMap->Handle : AssetHandle(0);
				X2_SERIALIZE_PROPERTY(AlbedoMap, albedoMapHandle, out);
			}
			if (!transparent)
			{
				{
					Ref<VulkanTexture2D> normalMap = materialAsset->GetNormalMap();
					bool hasNormalMap = normalMap ? !normalMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle normalMapHandle = hasNormalMap ? normalMap->Handle : AssetHandle(0);
					X2_SERIALIZE_PROPERTY(NormalMap, normalMapHandle, out);
				}

				{
					Ref<VulkanTexture2D> metallicRoughnessMap = materialAsset->GetMetallicRoughnessMap();
					bool hasMetallicRoughnessMap = metallicRoughnessMap ? !metallicRoughnessMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle metalnessMapHandle = hasMetallicRoughnessMap ? metallicRoughnessMap->Handle : AssetHandle(0);
					X2_SERIALIZE_PROPERTY(MetallicRoughnessMap, metalnessMapHandle, out);
				}

				{
					Ref<VulkanTexture2D> emissionMap = materialAsset->GetEmissionMap();
					bool hasEmissionMap = emissionMap ? !emissionMap.EqualsObject(Renderer::GetBlackTexture()) : false;
					AssetHandle emissionMapHandle = hasEmissionMap ? emissionMap->Handle : AssetHandle(0);
					X2_SERIALIZE_PROPERTY(EmissionMap, emissionMapHandle, out);
				}

				/*{
					Ref<VulkanTexture2D> metalnessMap = materialAsset->GetMetalnessMap();
					bool hasMetalnessMap = metalnessMap ? !metalnessMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle metalnessMapHandle = hasMetalnessMap ? metalnessMap->Handle : AssetHandle(0);
					X2_SERIALIZE_PROPERTY(MetalnessMap, metalnessMapHandle, out);
				}
				{
					Ref<VulkanTexture2D> roughnessMap = materialAsset->GetRoughnessMap();
					bool hasRoughnessMap = roughnessMap ? !roughnessMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle roughnessMapHandle = hasRoughnessMap ? roughnessMap->Handle : AssetHandle(0);
					X2_SERIALIZE_PROPERTY(RoughnessMap, roughnessMapHandle, out);
				}*/
			}

			X2_SERIALIZE_PROPERTY(MaterialFlags, materialAsset->GetMaterial()->GetFlags(), out);

			out << YAML::EndMap;
		}
		out << YAML::EndMap; // Material

		return std::string(out.c_str());
	}

	bool MaterialAssetSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<MaterialAsset>& targetMaterialAsset) const
	{
		YAML::Node root = YAML::Load(yamlString);
		YAML::Node materialNode = root["Material"];

		bool transparent = false;
		X2_DESERIALIZE_PROPERTY(Transparent, transparent, materialNode, false);

		targetMaterialAsset = Ref<MaterialAsset>::Create(transparent);

		X2_DESERIALIZE_PROPERTY(AlbedoColor, targetMaterialAsset->GetAlbedoColor(), materialNode, glm::vec3(0.8f));
		X2_DESERIALIZE_PROPERTY(Emission, targetMaterialAsset->GetEmission(), materialNode, 0.0f);

		if (!transparent)
		{
			targetMaterialAsset->SetUseNormalMap(materialNode["UseNormalMap"] ? materialNode["UseNormalMap"].as<bool>() : false);
			X2_DESERIALIZE_PROPERTY(Metalness, targetMaterialAsset->GetMetalness(), materialNode, 0.0f);
			X2_DESERIALIZE_PROPERTY(Roughness, targetMaterialAsset->GetRoughness(), materialNode, 0.5f);
		}
		else
		{
			X2_DESERIALIZE_PROPERTY(Transparency, targetMaterialAsset->GetTransparency(), materialNode, 1.0f);
		}

		AssetHandle albedoMap, normalMap, metallicRoughnessMap, emissionMap;//metalnessMap, roughnessMap;
		X2_DESERIALIZE_PROPERTY(AlbedoMap, albedoMap, materialNode, (AssetHandle)0);
		if (!transparent)
		{
			X2_DESERIALIZE_PROPERTY(NormalMap, normalMap, materialNode, (AssetHandle)0);
			X2_DESERIALIZE_PROPERTY(MetallicRoughnessMap, metallicRoughnessMap, materialNode, (AssetHandle)0);
			X2_DESERIALIZE_PROPERTY(EmissionMap, emissionMap, materialNode, (AssetHandle)0);

			//X2_DESERIALIZE_PROPERTY(RoughnessMap, roughnessMap, materialNode, (AssetHandle)0);
		}
		if (albedoMap)
		{
			if (AssetManager::IsAssetHandleValid(albedoMap))
				targetMaterialAsset->SetAlbedoMap(AssetManager::GetAsset<VulkanTexture2D>(albedoMap));
		}
		if (normalMap)
		{
			if (AssetManager::IsAssetHandleValid(normalMap))
				targetMaterialAsset->SetNormalMap(AssetManager::GetAsset<VulkanTexture2D>(normalMap));
		}
		if (metallicRoughnessMap)
		{
			if (AssetManager::IsAssetHandleValid(metallicRoughnessMap))
				targetMaterialAsset->SetMetallicRoughnessMap(AssetManager::GetAsset<VulkanTexture2D>(metallicRoughnessMap));
		}
		if (emissionMap)
		{
			if (AssetManager::IsAssetHandleValid(emissionMap))
				targetMaterialAsset->SetEmissionMap(AssetManager::GetAsset<VulkanTexture2D>(emissionMap));
		}
		/*if (metalnessMap)
		{
			if (AssetManager::IsAssetHandleValid(metalnessMap))
				targetMaterialAsset->SetMetalnessMap(AssetManager::GetAsset<VulkanTexture2D>(metalnessMap));
		}
		if (roughnessMap)
		{
			if (AssetManager::IsAssetHandleValid(roughnessMap))
				targetMaterialAsset->SetRoughnessMap(AssetManager::GetAsset<VulkanTexture2D>(roughnessMap));
		}*/

	/*	X2_DESERIALIZE_PROPERTY(MaterialFlags, roughnessMap, materialNode, (AssetHandle)0);
		if (materialNode["MaterialFlags"])
			targetMaterialAsset->GetMaterial()->SetFlags(materialNode["MaterialFlags"].as<uint32_t>());*/

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////
	// EnvironmentSerializer
	//////////////////////////////////////////////////////////////////////////////////

	bool EnvironmentSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		asset = Renderer::CreateEnvironmentMap(Project::GetEditorAssetManager()->GetFileSystemPathString(metadata));
		asset->Handle = metadata.Handle;
		return true;
	}

	bool EnvironmentSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	{
		outInfo.Offset = stream.GetStreamPosition();

		Ref<Environment> environment = AssetManager::GetAsset<Environment>(handle);

		uint64_t size = TextureRuntimeSerializer::SerializeToFile(environment->RawEnvMap, stream);
		size = TextureRuntimeSerializer::SerializeToFile(environment->RadianceMap, stream);
		size = TextureRuntimeSerializer::SerializeToFile(environment->IrradianceMap, stream);

		// Serialize as just generic VulkanTextureCube maybe?
		struct EnvironmentMapMetadata
		{
			uint64_t RadianceMapOffset, RadianceMapSize;
			uint64_t IrradianceMapOffset, IrradianceMapSize;
		};

		outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
		return true;
	}

	Ref<Asset> EnvironmentSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	{
		stream.SetStreamPosition(assetInfo.PackedOffset);
		Ref<VulkanTextureCube> rawEnvMap = TextureRuntimeSerializer::DeserializeTextureCube(stream);
		Ref<VulkanTextureCube> radianceMap = TextureRuntimeSerializer::DeserializeTextureCube(stream);
		Ref<VulkanTextureCube> irradianceMap = TextureRuntimeSerializer::DeserializeTextureCube(stream);
		return Ref<Environment>::Create(rawEnvMap, radianceMap, irradianceMap);
	}


	//////////////////////////////////////////////////////////////////////////////////
	// PrefabSerializer
	//////////////////////////////////////////////////////////////////////////////////

	void PrefabSerializer::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset) const
	{
		Ref<Prefab> prefab = asset.As<Prefab>();

		std::string yamlString = SerializeToYAML(prefab);

		std::ofstream fout(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
		fout << yamlString;
	}

	bool PrefabSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		std::ifstream stream(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
		if (!stream.is_open())
			return false;

		std::stringstream strStream;
		strStream << stream.rdbuf();

		Ref<Prefab> prefab = Ref<Prefab>::Create();
		bool success = DeserializeFromYAML(strStream.str(), prefab);
		if (!success)
			return false;

		asset = prefab;
		asset->Handle = metadata.Handle;
		return true;
	}

	bool PrefabSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	{
		Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(handle);

		std::string yamlString = SerializeToYAML(prefab);
		outInfo.Offset = stream.GetStreamPosition();
		stream.WriteString(yamlString);
		outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
		return true;
	}

	Ref<Asset> PrefabSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	{
		stream.SetStreamPosition(assetInfo.PackedOffset);
		std::string yamlString;
		stream.ReadString(yamlString);

		Ref<Prefab> prefab = Ref<Prefab>::Create();
		bool result = DeserializeFromYAML(yamlString, prefab);
		if (!result)
			return nullptr;

		return prefab;
	}

	std::string PrefabSerializer::SerializeToYAML(Ref<Prefab> prefab) const
	{
		YAML::Emitter out;

		out << YAML::BeginMap;
		out << YAML::Key << "Prefab";
		out << YAML::Value << YAML::BeginSeq;

		prefab->m_Scene->m_Registry.each([&](auto entityID)
			{
				Entity entity = { entityID, prefab->m_Scene.Raw() };
				if (!entity || !entity.HasComponent<IDComponent>())
					return;

				SceneSerializer::SerializeEntity(out, entity);
			});

		out << YAML::EndSeq;
		out << YAML::EndMap;

		return std::string(out.c_str());
	}

	bool PrefabSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<Prefab> prefab) const
	{
		YAML::Node data = YAML::Load(yamlString);
		if (!data["Prefab"])
			return false;

		YAML::Node prefabNode = data["Prefab"];
		SceneSerializer::DeserializeEntities(prefabNode, prefab->m_Scene);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////
	// SceneAssetSerializer
	//////////////////////////////////////////////////////////////////////////////////

	void SceneAssetSerializer::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset) const
	{
		SceneSerializer serializer(asset.As<Scene>());
		serializer.Serialize(Project::GetEditorAssetManager()->GetFileSystemPath(metadata).string());
	}

	bool SceneAssetSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		asset = Ref<Scene>::Create("SceneAsset", false, false);
		asset->Handle = metadata.Handle;
		return true;
	}

	bool SceneAssetSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	{
		Ref<Scene> scene = Ref<Scene>::Create("AssetPackTemp", true, false);
		const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
		SceneSerializer serializer(scene);
		if (serializer.Deserialize(Project::GetAssetDirectory() / metadata.FilePath))
		{
			return serializer.SerializeToAssetPack(stream, outInfo);
		}
		return false;
	}

	Ref<Asset> SceneAssetSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	{
		X2_CORE_VERIFY(false); // Not implemented
		return nullptr;
	}

	Ref<Scene> SceneAssetSerializer::DeserializeSceneFromAssetPack(FileStreamReader& stream, const AssetPackFile::SceneInfo& sceneInfo) const
	{
		Ref<Scene> scene = Ref<Scene>::Create();
		SceneSerializer serializer(scene);
		if (serializer.DeserializeFromAssetPack(stream, sceneInfo))
			return scene;

		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////////////
	// MeshColliderSerializer
	//////////////////////////////////////////////////////////////////////////////////

	//void MeshColliderSerializer::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset) const
	//{
	//	Ref<MeshColliderAsset> meshCollider = asset.As<MeshColliderAsset>();

	//	std::string yamlString = SerializeToYAML(meshCollider);

	//	std::ofstream fout(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
	//	fout << yamlString;
	//}

	//bool MeshColliderSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	//{
	//	std::ifstream stream(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
	//	if (!stream.is_open())
	//		return false;

	//	std::stringstream strStream;
	//	strStream << stream.rdbuf();

	//	if (strStream.rdbuf()->in_avail() == 0)
	//		return false;

	//	Ref<MeshColliderAsset> meshCollider = Ref<MeshColliderAsset>::Create();
	//	bool result = DeserializeFromYAML(strStream.str(), meshCollider);
	//	if (!result)
	//		return false;

	//	asset = meshCollider;
	//	asset->Handle = metadata.Handle;
	//	return true;
	//}

	//bool MeshColliderSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	//{
	//	Ref<MeshColliderAsset> meshCollider = AssetManager::GetAsset<MeshColliderAsset>(handle);

	//	std::string yamlString = SerializeToYAML(meshCollider);
	//	outInfo.Offset = stream.GetStreamPosition();
	//	stream.WriteString(yamlString);
	//	outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
	//	return true;
	//}

	//Ref<Asset> MeshColliderSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	//{
	//	stream.SetStreamPosition(assetInfo.PackedOffset);
	//	std::string yamlString;
	//	stream.ReadString(yamlString);

	//	Ref<MeshColliderAsset> meshCollider = Ref<MeshColliderAsset>::Create();
	//	bool result = DeserializeFromYAML(yamlString, meshCollider);
	//	if (!result)
	//		return nullptr;

	//	return meshCollider;
	//}

	//std::string MeshColliderSerializer::SerializeToYAML(Ref<MeshColliderAsset> meshCollider) const
	//{
	//	YAML::Emitter out;

	//	out << YAML::BeginMap;
	//	out << YAML::Key << "ColliderMesh" << YAML::Value << meshCollider->ColliderMesh;
	//	out << YAML::Key << "Material" << YAML::Value << meshCollider->Material;
	//	out << YAML::Key << "EnableVertexWelding" << YAML::Value << meshCollider->EnableVertexWelding;
	//	out << YAML::Key << "VertexWeldTolerance" << YAML::Value << meshCollider->VertexWeldTolerance;
	//	out << YAML::Key << "FlipNormals" << YAML::Value << meshCollider->FlipNormals;
	//	out << YAML::Key << "CheckZeroAreaTriangles" << YAML::Value << meshCollider->CheckZeroAreaTriangles;
	//	out << YAML::Key << "AreaTestEpsilon" << YAML::Value << meshCollider->AreaTestEpsilon;
	//	out << YAML::Key << "ShiftVerticesToOrigin" << YAML::Value << meshCollider->ShiftVerticesToOrigin;
	//	out << YAML::Key << "AlwaysShareShape" << YAML::Value << meshCollider->AlwaysShareShape;
	//	out << YAML::Key << "CollisionComplexity" << YAML::Value << (uint8_t)meshCollider->CollisionComplexity;
	//	out << YAML::Key << "ColliderScale" << YAML::Value << meshCollider->ColliderScale;
	//	out << YAML::Key << "PreviewScale" << YAML::Value << meshCollider->PreviewScale;
	//	out << YAML::EndMap;

	//	return std::string(out.c_str());
	//}

	//bool MeshColliderSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<MeshColliderAsset> targetMeshCollider) const
	//{
	//	YAML::Node data = YAML::Load(yamlString);

	//	targetMeshCollider->ColliderMesh = data["ColliderMesh"].as<AssetHandle>(0);
	//	targetMeshCollider->Material = data["Material"].as<AssetHandle>(0);
	//	targetMeshCollider->EnableVertexWelding = data["EnableVertexWelding"].as<bool>(true);
	//	targetMeshCollider->VertexWeldTolerance = glm::clamp<float>(data["VertexWeldTolerance"].as<float>(0.1f), 0.05f, 1.0f);
	//	targetMeshCollider->FlipNormals = data["FlipNormals"].as<bool>(false);
	//	targetMeshCollider->CheckZeroAreaTriangles = data["CheckZeroAreaTriangles"].as<bool>(false);
	//	targetMeshCollider->AreaTestEpsilon = glm::max(0.06f, data["AreaTestEpsilon"].as<float>(0.06f));
	//	targetMeshCollider->ShiftVerticesToOrigin = data["ShiftVerticesToOrigin"].as<bool>(false);
	//	targetMeshCollider->AlwaysShareShape = data["AlwaysShareShape"].as<bool>(false);
	//	targetMeshCollider->CollisionComplexity = (ECollisionComplexity)data["CollisionComplexity"].as<uint8_t>(0);
	//	targetMeshCollider->ColliderScale = data["ColliderScale"].as<glm::vec3>(glm::vec3(1.0f));
	//	targetMeshCollider->PreviewScale = data["PreviewScale"].as<glm::vec3>(glm::vec3(1.0f));

	//	return true;
	//}

	//////////////////////////////////////////////////////////////////////////////////
	// ScriptFileSerializer
	//////////////////////////////////////////////////////////////////////////////////

	//void ScriptFileSerializer::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset) const
	//{
	//	std::ofstream stream(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
	//	X2_CORE_VERIFY(stream.is_open());

	//	std::ifstream templateStream("Resources/Templates/NewClassTemplate.cs");
	//	X2_CORE_VERIFY(templateStream.is_open());

	//	std::stringstream templateStrStream;
	//	templateStrStream << templateStream.rdbuf();
	//	std::string templateString = templateStrStream.str();

	//	templateStream.close();

	//	auto replaceTemplateToken = [&templateString](const char* token, const std::string& value)
	//	{
	//		size_t pos = 0;
	//		while ((pos = templateString.find(token, pos)) != std::string::npos)
	//		{
	//			templateString.replace(pos, strlen(token), value);
	//			pos += strlen(token);
	//		}
	//	};

	//	auto scriptFileAsset = asset.As<ScriptFileAsset>();
	//	replaceTemplateToken("$NAMESPACE_NAME$", scriptFileAsset->GetClassNamespace());
	//	replaceTemplateToken("$CLASS_NAME$", scriptFileAsset->GetClassName());

	//	stream << templateString;
	//	stream.close();
	//}

	//bool ScriptFileSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	//{
	//	asset = Ref<ScriptFileAsset>::Create();
	//	asset->Handle = metadata.Handle;
	//	return true;
	//}

	//bool ScriptFileSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo) const
	//{
	//	X2_CORE_VERIFY(false); // Not implemented

	//	outInfo.Offset = stream.GetStreamPosition();

	//	// Write 64 FFs (dummy data)
	//	for (uint32_t i = 0; i < 16; i++)
	//		stream.WriteRaw<uint32_t>(0xffffffff);

	//	outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
	//	return true;
	//}

	//Ref<Asset> ScriptFileSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo) const
	//{
	//	X2_CORE_VERIFY(false); // Not implemented
	//	return nullptr;
	//}

}
