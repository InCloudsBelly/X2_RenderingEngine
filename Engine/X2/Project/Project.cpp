#include "Precompiled.h"
#include "Project.h"

#include "X2/Asset/AssetManager.h"
//#include "Hazel/Audio/AudioEngine.h"
//#include "Hazel/Audio/AudioEvents/AudioCommandRegistry.h"

//#include "Hazel/Physics/3D/PhysicsSystem.h"

namespace X2 {

	Project::Project()
	{
		//m_AudioCommands = Ref<AudioCommandRegistry>::Create();
	}

	Project::~Project()
	{
	}

	void Project::SetActive(Ref<Project> project)
	{
		if (s_ActiveProject)
		{
			s_AssetManager = nullptr;
			//PhysicsSystem::Shutdown();
			//AudioCommandRegistry::Shutdown();
		}

		s_ActiveProject = project;
		if (s_ActiveProject)
		{
			s_AssetManager = Ref<EditorAssetManager>::Create();
			//PhysicsSystem::Init();

			//MiniAudioEngine::Get().OnProjectLoaded();

			// AudioCommandsRegistry must be deserialized after AssetManager,
			// otheriwse all of the command action targets are going to be invalid and cleared!
			//WeakRef<AudioCommandRegistry> audioCommands = s_ActiveProject->m_AudioCommands;
			//if (!runtime)
			//AudioCommandRegistry::Init(audioCommands);
		}
	}

	void Project::SetActiveRuntime(Ref<Project> project, Ref<AssetPack> assetPack)
	{
		if (s_ActiveProject)
		{
			s_AssetManager = nullptr;
			//PhysicsSystem::Shutdown();
			//AudioCommandRegistry::Shutdown();
		}

		s_ActiveProject = project;
		if (s_ActiveProject)
		{
			s_AssetManager = Ref<RuntimeAssetManager>::Create();
			Project::GetRuntimeAssetManager()->SetAssetPack(assetPack);

			//PhysicsSystem::Init();

			// AudioCommandsRegistry must be deserialized after AssetManager,
			// otheriwse all of the command action targets are going to be invalid and cleared!
			//WeakRef<AudioCommandRegistry> audioCommands = s_ActiveProject->m_AudioCommands;
			//if (!runtime)
			//MiniAudioEngine::Get().OnProjectLoaded();
			//AudioCommandRegistry::Init(audioCommands);
		}
	}

	void Project::OnSerialized()
	{
		//m_AudioCommands->WriteRegistryToFile(std::filesystem::path(m_Config.ProjectDirectory) / m_Config.AudioCommandsRegistryPath);
	}

	void Project::OnDeserialized()
	{
	}

}
