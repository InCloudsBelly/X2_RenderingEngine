#pragma once

#include "X2/Core/Ref.h"
#include "X2/Asset/AssetManager/EditorAssetManager.h"
#include "X2/Asset/AssetManager/RuntimeAssetManager.h"

#include <filesystem>

namespace X2 {

	//class AudioCommandRegistry;

	struct ProjectConfig
	{
		std::string Name;

		std::string AssetDirectory;
		std::string AssetRegistryPath;

		std::string AudioCommandsRegistryPath = "Assets/AudioCommandsRegistry.hzr";

		std::string MeshPath;
		std::string MeshSourcePath;

		std::string AnimationPath;

		std::string ScriptModulePath = "Assets/Scripts/Binaries";
		std::string DefaultNamespace;

		std::string StartScene;

		bool AutomaticallyReloadAssembly;

		bool EnableAutoSave = false;
		int AutoSaveIntervalSeconds = 300;

		// Not serialized
		std::string ProjectFileName;
		std::string ProjectDirectory;
	};

	class Project 
	{
	public:
		Project();
		~Project();

		const ProjectConfig& GetConfig() const { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(Ref<Project> project);
		static void SetActiveRuntime(Ref<Project> project, Ref<AssetPack> assetPack);

		inline static Ref<AssetManagerBase> GetAssetManager() { return s_AssetManager; }
		inline static Ref<EditorAssetManager> GetEditorAssetManager() { return std::dynamic_pointer_cast<EditorAssetManager>(s_AssetManager); }
		inline static Ref<RuntimeAssetManager> GetRuntimeAssetManager() { return std::dynamic_pointer_cast<RuntimeAssetManager>(s_AssetManager); }

		static const std::string& GetProjectName()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return s_ActiveProject->GetConfig().Name;
		}

		static std::filesystem::path GetProjectDirectory()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return s_ActiveProject->GetConfig().ProjectDirectory;
		}

		static std::filesystem::path GetAssetDirectory()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AssetDirectory;
		}

		static std::filesystem::path GetAssetRegistryPath()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AssetRegistryPath;
		}

		static std::filesystem::path GetMeshPath()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().MeshPath;
		}

		static std::filesystem::path GetAnimationPath()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AnimationPath;
		}

		static std::filesystem::path GetAudioCommandsRegistryPath()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AudioCommandsRegistryPath;
		}

		static std::filesystem::path GetScriptModulePath()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().ScriptModulePath;
		}

		static std::filesystem::path GetScriptModuleFilePath()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return GetScriptModulePath() / fmt::format("{0}.dll", GetProjectName());
		}

		static std::filesystem::path GetCacheDirectory()
		{
			X2_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / "Cache";
		}
	private:
		void OnSerialized();
		void OnDeserialized();

	private:
		ProjectConfig m_Config;
		//Ref<AudioCommandRegistry> m_AudioCommands;
		inline static Ref<AssetManagerBase> s_AssetManager;

		friend class ProjectSettingsWindow;
		friend class ProjectSerializer;

		inline static Ref<Project> s_ActiveProject;
	};

}
