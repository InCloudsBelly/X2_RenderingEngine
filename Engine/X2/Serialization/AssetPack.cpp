#include "Precompiled.h"
#include "AssetPack.h"

//#include "X2/Core/Platform.h"

#include "X2/Asset/AssetManager.h"
#include "X2/Scene/Scene.h"
#include "X2/Scene/SceneSerializer.h"
#include "X2/Scene/Prefab.h"

//#include "X2/Audio/AudioEvents/AudioCommandRegistry.h"
//#include "X2/Editor/NodeGraphEditor/NodeGraphAsset.h"

namespace X2 {

	AssetPack::AssetPack(const std::filesystem::path& path)
		: m_Path(path)
	{
	}

	void AssetPack::AddAsset(Ref<Asset> asset)
	{
#if 0
		if (!asset)
			return;
		X2_CORE_ASSERT(asset);

		AssetHandle handle = asset->Handle;

		auto& assetMap = m_File.Index.Assets;
		if (assetMap.find(handle) != assetMap.end())
		{
			X2_CORE_WARN_TAG("AssetPack", "Asset already present in asset pack");
			return;
		}

		AssetPackFile::AssetInfo& info = assetMap[handle];
		info.Type = (uint16_t)asset->GetAssetType();
#endif
	}

	void AssetPack::Serialize()
	{
		//m_Serializer.Serialize(m_Path, m_File);
	}

	Ref<Scene> AssetPack::LoadScene(AssetHandle sceneHandle)
	{
		auto it = m_File.Index.Scenes.find(sceneHandle);
		if (it == m_File.Index.Scenes.end())
			return nullptr;

		const AssetPackFile::SceneInfo& sceneInfo = it->second;

		FileStreamReader stream(m_Path);
		Ref<Scene> scene = AssetImporter::DeserializeSceneFromAssetPack(stream, sceneInfo);
		scene->Handle = sceneHandle;
		return scene;
	}

	Ref<Asset> AssetPack::LoadAsset(AssetHandle sceneHandle, AssetHandle assetHandle)
	{
		const AssetPackFile::AssetInfo* assetInfo = nullptr;

		bool foundAsset = false;
		if (sceneHandle)
		{
			// Fast(er) path
			auto it = m_File.Index.Scenes.find(sceneHandle);
			if (it != m_File.Index.Scenes.end())
			{
				const AssetPackFile::SceneInfo& sceneInfo = it->second;
				auto assetIt = sceneInfo.Assets.find(assetHandle);
				if (assetIt != sceneInfo.Assets.end())
				{
					foundAsset = true;
					assetInfo = &assetIt->second;
				}
			}
		}

		if (!foundAsset)
		{
			// Slow(er) path
			for (const auto& [handle, sceneInfo] : m_File.Index.Scenes)
			{
				auto assetIt = sceneInfo.Assets.find(assetHandle);
				if (assetIt != sceneInfo.Assets.end())
				{
					assetInfo = &assetIt->second;
					break;
				}
			}

			if (!assetInfo)
				return nullptr;
		}

		FileStreamReader stream(m_Path);
		Ref<Asset> asset = AssetImporter::DeserializeFromAssetPack(stream, *assetInfo);
		//X2_CORE_VERIFY(asset);
		if (!asset)
			return nullptr;

		asset->Handle = assetHandle;
		return asset;
	}

	bool AssetPack::IsAssetHandleValid(AssetHandle assetHandle) const
	{
		return m_AssetHandleIndex.find(assetHandle) != m_AssetHandleIndex.end();
	}

	bool AssetPack::IsAssetHandleValid(AssetHandle sceneHandle, AssetHandle assetHandle) const
	{
		auto sceneIterator = m_File.Index.Scenes.find(sceneHandle);
		if (sceneIterator == m_File.Index.Scenes.end())
			return false;

		const auto& sceneInfo = sceneIterator->second;
		return sceneInfo.Assets.find(assetHandle) != sceneInfo.Assets.end();
	}

	Buffer AssetPack::ReadAppBinary()
	{
		FileStreamReader stream(m_Path);
		stream.SetStreamPosition(m_File.Index.PackedAppBinaryOffset);
		Buffer buffer;
		stream.ReadBuffer(buffer);
		X2_CORE_VERIFY(m_File.Index.PackedAppBinarySize == (buffer.Size + sizeof(uint32_t)));
		return buffer;
	}

	uint64_t AssetPack::GetBuildVersion()
	{
		return m_File.Header.BuildVersion;
	}

	Ref<AssetPack> AssetPack::CreateFromActiveProject(std::atomic<float>& progress)
	{
#define DEBUG_PRINT 1

		// Need to find all scenes and see which assets they use

		AssetPackFile assetPackFile;
		//assetPackFile.Header.BuildVersion = Platform::GetCurrentDateTimeU64();
		assetPackFile.Header.BuildVersion = 12;

		progress = 0.0f;

		std::unordered_set<AssetHandle> fullAssetList;
		const AssetRegistry& registry = Project::GetEditorAssetManager()->GetAssetRegistry();

		uint32_t sceneCount = 0;
		for (const auto& [handle, metadata] : registry)
		{
			if (metadata.Type == AssetType::Scene)
				sceneCount++;
		}

		float progressIncrement = 0.5f / (float)sceneCount;

		//// Audio Command Registry
		//std::unordered_set<AssetHandle> audioAssets = AudioCommandRegistry::GetAllAssets();
		//fullAssetList.insert(audioAssets.begin(), audioAssets.end());

		//// Sound Graphs
		//std::unordered_set<AssetHandle> soundGraphs = AssetManager::GetAllAssetsWithType<SoundGraphAsset>();
		//fullAssetList.insert(soundGraphs.begin(), soundGraphs.end());

		//// Audio "files"
		//std::unordered_set<AssetHandle> audioFiles = AssetManager::GetAllAssetsWithType<AudioFile>();
		//fullAssetList.insert(audioFiles.begin(), audioFiles.end());

		for (const auto& [handle, metadata] : registry)
		{
			if (metadata.Type == AssetType::Scene)
			{
				Ref<Scene> scene = CreateRef<Scene>("AssetPack", true, false);
				SceneSerializer serializer(scene.get());
				X2_CORE_TRACE("Deserializing Scene: {}", metadata.FilePath);
				if (serializer.Deserialize(Project::GetAssetDirectory() / metadata.FilePath))
				{
					std::unordered_set<AssetHandle> sceneAssetList = scene->GetAssetList();
					X2_CORE_TRACE("  Scene has {} used assets", sceneAssetList.size());

					std::unordered_set<AssetHandle> sceneAssetListWithoutPrefabs = sceneAssetList;
					for (AssetHandle assetHandle : sceneAssetListWithoutPrefabs)
					{
						const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(assetHandle);
						if (metadata.Type == AssetType::Prefab)
						{
							Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(assetHandle);
							std::unordered_set<AssetHandle> childPrefabAssetList = prefab->GetAssetList(true);
							sceneAssetList.insert(childPrefabAssetList.begin(), childPrefabAssetList.end());
						}
					}

				/*	sceneAssetList.insert(audioAssets.begin(), audioAssets.end());
					sceneAssetList.insert(soundGraphs.begin(), soundGraphs.end());
					sceneAssetList.insert(audioFiles.begin(), audioFiles.end());*/

					AssetPackFile::SceneInfo& sceneInfo = assetPackFile.Index.Scenes[handle];
					for (AssetHandle assetHandle : sceneAssetList)
					{
						AssetPackFile::AssetInfo& assetInfo = sceneInfo.Assets[assetHandle];
						const auto& assetMetadata = Project::GetEditorAssetManager()->GetMetadata(assetHandle);
						assetInfo.Type = (uint16_t)assetMetadata.Type;

						// For AnimationController asset, we need to make sure each of the "states" that it refers
						// to has been loaded in order that we can then serialize it to asset pack correctly.
						// (as otherwise the lazy-loaded skeleton assets don't end up in the asset pack)
						
						//if (assetMetadata.Type == AssetType::AnimationController) {
						//	Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(assetHandle);
						//	for (size_t stateIndex = 0; stateIndex < controller->GetNumStates(); ++stateIndex)
						//	{
						//		const auto& state = controller->GetAnimationState(stateIndex);

						//		// GetAnimation() will either load skeleton from DCC, or push given skeleton into the animation asset
						//		const auto& anim = state->GetAnimationAsset()->GetAnimation(state->GetAnimationIndex(), controller->GetSkeletonAsset()->GetSkeleton());
						//	}
						//}
					}

					fullAssetList.insert(sceneAssetList.begin(), sceneAssetList.end());
				}
				else
				{
					X2_CONSOLE_LOG_ERROR("Failed to deserialize Scene: {} ({})", metadata.FilePath, handle);
				}
				progress = progress + progressIncrement;
			}
		}

#if 0
		// Make sure all Prefab-referenced assets are included
		for (AssetHandle handle : fullAssetList)
		{
			const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
			if (metadata.Type == AssetType::Prefab)
			{
				Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(handle);
				std::unordered_set<AssetHandle> childPrefabAssetList = prefab->GetAssetList(true);
				fullAssetList.insert(childPrefabAssetList.begin(), childPrefabAssetList.end());
			}
		}
#endif

		X2_CONSOLE_LOG_INFO("Project contains {} used assets", fullAssetList.size());

#if DEBUG_PRINT
		X2_CORE_TRACE("Complete AssetPack:");

		for (AssetHandle handle : fullAssetList)
		{
			const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
			X2_CORE_TRACE("{}: {} ({})", Utils::AssetTypeToString(metadata.Type), metadata.FilePath, metadata.Handle);
		}
#endif

		Buffer appBinary = FileSystem::ReadBytes(Project::GetScriptModuleFilePath());

		AssetPackSerializer::Serialize(Project::GetAssetDirectory() / "AssetPack.hap", assetPackFile, appBinary, progress);
		progress = 1.0f;
		return nullptr;
	}

	Ref<AssetPack> AssetPack::LoadActiveProject()
	{
		return Load(Project::GetAssetDirectory() / "AssetPack.hap");
	}

	Ref<AssetPack> AssetPack::Load(const std::filesystem::path& path)
	{
		Ref<AssetPack> assetPack = CreateRef<AssetPack>();
		assetPack->m_Path = path;
		bool success = AssetPackSerializer::DeserializeIndex(assetPack->m_Path, assetPack->m_File);
		X2_CORE_VERIFY(success);
		if (!success)
			return nullptr;

		// Populate asset handle index
		const auto& index = assetPack->m_File.Index;
		for (const auto& [sceneHandle, sceneInfo] : index.Scenes)
		{
			assetPack->m_AssetHandleIndex.insert(sceneHandle);
			for (const auto& [assetHandle, assetInfo] : sceneInfo.Assets)
			{
				assetPack->m_AssetHandleIndex.insert(assetHandle);
			}
		}

		// Debug log
#ifndef X2_DIST
		{
			X2_CORE_INFO("-----------------------------------------------------");
			X2_CORE_INFO("AssetPack Dump {}", assetPack->m_Path);
			X2_CORE_INFO("-----------------------------------------------------");
			std::unordered_map<AssetType, uint32_t> typeCounts;
			std::unordered_set<AssetHandle> duplicatePreventionSet;
			for (const auto& [sceneHandle, sceneInfo] : index.Scenes)
			{
				X2_CORE_INFO("Scene {}:", sceneHandle);
				for (const auto& [assetHandle, assetInfo] : sceneInfo.Assets)
				{
					X2_CORE_INFO("  {} - {}", Utils::AssetTypeToString((AssetType)assetInfo.Type), assetHandle);

					if (duplicatePreventionSet.find(assetHandle) == duplicatePreventionSet.end())
					{
						duplicatePreventionSet.insert(assetHandle);
						typeCounts[(AssetType)assetInfo.Type]++;
					}
				}
			}
			X2_CORE_INFO("-----------------------------------------------------");
			X2_CORE_INFO("Summary:");
			for (const auto& [type, count] : typeCounts)
			{
				X2_CORE_INFO("  {} {}", count, Utils::AssetTypeToString(type));
			}
			X2_CORE_INFO("-----------------------------------------------------");
		}
#endif

		//__debugbreak();
		return assetPack;
	}

}
