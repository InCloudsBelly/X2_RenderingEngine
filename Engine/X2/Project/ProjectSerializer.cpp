#include "Precompiled.h"
#include "ProjectSerializer.h"
//#include "X2/Physics/3D/PhysicsSystem.h"
//#include "X2/Physics/3D/PhysicsLayer.h"
//#include "X2/Audio/AudioEngine.h"

#include "X2/Utilities/YAMLSerializationHelpers.h"
#include "X2/Utilities/SerializationMacros.h"
#include "X2/Utilities/StringUtils.h"

#include "yaml-cpp/yaml.h"

#include <filesystem>

namespace X2 {

	ProjectSerializer::ProjectSerializer(Ref<Project> project)
		: m_Project(project)
	{
	}

	void ProjectSerializer::Serialize(const std::filesystem::path& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << m_Project->m_Config.Name;
			out << YAML::Key << "AssetDirectory" << YAML::Value << m_Project->m_Config.AssetDirectory;
			out << YAML::Key << "AssetRegistry" << YAML::Value << m_Project->m_Config.AssetRegistryPath;
			//out << YAML::Key << "AudioCommandsRegistryPath" << YAML::Value << m_Project->m_Config.AudioCommandsRegistryPath;
			out << YAML::Key << "MeshPath" << YAML::Value << m_Project->m_Config.MeshPath;
			out << YAML::Key << "MeshSourcePath" << YAML::Value << m_Project->m_Config.MeshSourcePath;
			//out << YAML::Key << "AnimationPath" << YAML::Value << m_Project->m_Config.AnimationPath;

			//out << YAML::Key << "ScriptModulePath" << YAML::Value << m_Project->m_Config.ScriptModulePath;
			out << YAML::Key << "DefaultNamespace" << YAML::Value << m_Project->m_Config.DefaultNamespace;

			out << YAML::Key << "StartScene" << YAML::Value << m_Project->m_Config.StartScene;
			out << YAML::Key << "AutomaticallyReloadAssembly" << YAML::Value << m_Project->m_Config.AutomaticallyReloadAssembly;
			out << YAML::Key << "AutoSave" << YAML::Value << m_Project->m_Config.EnableAutoSave;
			out << YAML::Key << "AutoSaveInterval" << YAML::Value << m_Project->m_Config.AutoSaveIntervalSeconds;

		/*	out << YAML::Key << "Audio" << YAML::Value;
			{
				out << YAML::BeginMap;
				auto userConfig = MiniAudioEngine::Get().GetUserConfiguration();
				X2_SERIALIZE_PROPERTY(FileStreamingDurationThreshold, userConfig.FileStreamingDurationThreshold, out);
				out << YAML::EndMap;
			}*/

//			out << YAML::Key << "Physics" << YAML::Value;
//			{
//				out << YAML::BeginMap;
//
//				const auto& physicsSettings = PhysicsSystem::GetSettings();
//
//				out << YAML::Key << "FixedTimestep" << YAML::Value << physicsSettings.FixedTimestep;
//				out << YAML::Key << "Gravity" << YAML::Value << physicsSettings.Gravity;
//				out << YAML::Key << "BroadphaseAlgorithm" << YAML::Value << (uint32_t)physicsSettings.BroadphaseAlgorithm;
//				if (physicsSettings.BroadphaseAlgorithm != BroadphaseType::AutomaticBoxPrune)
//				{
//					out << YAML::Key << "WorldBoundsMin" << YAML::Value << physicsSettings.WorldBoundsMin;
//					out << YAML::Key << "WorldBoundsMax" << YAML::Value << physicsSettings.WorldBoundsMax;
//					out << YAML::Key << "WorldBoundsSubdivisions" << YAML::Value << physicsSettings.WorldBoundsSubdivisions;
//				}
//				out << YAML::Key << "FrictionModel" << YAML::Value << (int)physicsSettings.FrictionModel;
//				out << YAML::Key << "SolverPositionIterations" << YAML::Value << physicsSettings.SolverIterations;
//				out << YAML::Key << "SolverVelocityIterations" << YAML::Value << physicsSettings.SolverVelocityIterations;
//
//#ifdef X2_DEBUG
//				out << YAML::Key << "DebugOnPlay" << YAML::Value << physicsSettings.DebugOnPlay;
//				out << YAML::Key << "DebugType" << YAML::Value << (int)physicsSettings.DebugType;
//#endif
//
//				// > 1 because of the Default layer
//				if (PhysicsLayerManager::GetLayerCount() > 1)
//				{
//					out << YAML::Key << "Layers";
//					out << YAML::Value << YAML::BeginSeq;
//					for (const auto& layer : PhysicsLayerManager::GetLayers())
//					{
//						// Never serialize the Default layer
//						if (layer.LayerID == 0)
//							continue;
//
//						out << YAML::BeginMap;
//						out << YAML::Key << "Name" << YAML::Value << layer.Name;
//						out << YAML::Key << "CollidesWithSelf" << YAML::Value << layer.CollidesWithSelf;
//						out << YAML::Key << "CollidesWith" << YAML::Value;
//						out << YAML::BeginSeq;
//						for (const auto& collidingLayer : PhysicsLayerManager::GetLayerCollisions(layer.LayerID))
//						{
//							out << YAML::BeginMap;
//							out << YAML::Key << "Name" << YAML::Value << collidingLayer.Name;
//							out << YAML::EndMap;
//						}
//						out << YAML::EndSeq;
//						out << YAML::EndMap;
//					}
//					out << YAML::EndSeq;
//				}
//
//				out << YAML::EndMap;
//			}

			out << YAML::Key << "Log" << YAML::Value;
			{
				out << YAML::BeginMap;
				auto& tags = Log::EnabledTags();
				for (auto& [name, details] : tags)
				{
					if (!name.empty()) // Don't serialize untagged log
					{
						out << YAML::Key << name << YAML::Value;
						out << YAML::BeginMap;
						{
							out << YAML::Key << "Enabled" << YAML::Value << details.Enabled;
							out << YAML::Key << "LevelFilter" << YAML::Value << Log::LevelToString(details.LevelFilter);
							out << YAML::EndMap;
						}

					}
				}
				out << YAML::EndMap;
			}

			out << YAML::EndMap;
		}
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();

		m_Project->OnSerialized();
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		//PhysicsLayerManager::ClearLayers();
		//PhysicsLayerManager::AddLayer("Default");

		std::ifstream stream(filepath);
		X2_CORE_ASSERT(stream);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Project"])
			return false;

		YAML::Node rootNode = data["Project"];
		if (!rootNode["Name"])
			return false;

		auto& config = m_Project->m_Config;
		config.Name = rootNode["Name"].as<std::string>();

		config.AssetDirectory = rootNode["AssetDirectory"].as<std::string>();
		config.AssetRegistryPath = rootNode["AssetRegistry"].as<std::string>();

		std::filesystem::path projectPath = filepath;
		config.ProjectFileName = projectPath.filename().string();
		config.ProjectDirectory = projectPath.parent_path().string();

		/*if (rootNode["AudioCommandsRegistryPath"])
			config.AudioCommandsRegistryPath = rootNode["AudioCommandsRegistryPath"].as<std::string>();*/

		config.StartScene = rootNode["StartScene"].as<std::string>("");

		config.MeshPath = rootNode["MeshPath"].as<std::string>();
		config.MeshSourcePath = rootNode["MeshSourcePath"].as<std::string>();

		//config.AnimationPath = rootNode["AnimationPath"].as<std::string>("Assets/Animation");

		//config.ScriptModulePath = rootNode["ScriptModulePath"].as<std::string>(config.ScriptModulePath);

		config.DefaultNamespace = rootNode["DefaultNamespace"].as<std::string>(config.Name);
		config.AutomaticallyReloadAssembly = rootNode["AutomaticallyReloadAssembly"].as<bool>(true);

		config.EnableAutoSave = rootNode["AutoSave"].as<bool>(false);
		config.AutoSaveIntervalSeconds = rootNode["AutoSaveInterval"].as<int>(300);

//		// Audio
//		auto audioNode = rootNode["Audio"];
//		if (audioNode)
//		{
//			auto userConfig = MiniAudioEngine::Get().GetUserConfiguration();
//			X2_DESERIALIZE_PROPERTY(FileStreamingDurationThreshold, userConfig.FileStreamingDurationThreshold, audioNode, userConfig.FileStreamingDurationThreshold);
//			MiniAudioEngine::Get().SetUserConfiguration(userConfig);
//		}
//
//		// Physics
//		auto physicsNode = rootNode["Physics"];
//		if (physicsNode)
//		{
//			auto& physicsSettings = PhysicsSystem::GetSettings();
//
//			physicsSettings.FixedTimestep = physicsNode["FixedTimestep"] ? physicsNode["FixedTimestep"].as<float>() : 1.0f / 100.0f;
//			physicsSettings.Gravity = physicsNode["Gravity"] ? physicsNode["Gravity"].as<glm::vec3>() : glm::vec3{ 0.0f, -9.81f, 0.0f };
//			physicsSettings.BroadphaseAlgorithm = physicsNode["BroadphaseAlgorithm"] ? (BroadphaseType)physicsNode["BroadphaseAlgorithm"].as<uint32_t>() : BroadphaseType::AutomaticBoxPrune;
//			if (physicsSettings.BroadphaseAlgorithm != BroadphaseType::AutomaticBoxPrune)
//			{
//				physicsSettings.WorldBoundsMin = physicsNode["WorldBoundsMin"] ? physicsNode["WorldBoundsMin"].as<glm::vec3>() : glm::vec3(-100.0f);
//				physicsSettings.WorldBoundsMax = physicsNode["WorldBoundsMax"] ? physicsNode["WorldBoundsMax"].as<glm::vec3>() : glm::vec3(100.0f);
//				physicsSettings.WorldBoundsSubdivisions = physicsNode["WorldBoundsSubdivisions"] ? physicsNode["WorldBoundsSubdivisions"].as<uint32_t>() : 2;
//			}
//			physicsSettings.FrictionModel = physicsNode["FrictionModel"] ? (FrictionType)physicsNode["FrictionModel"].as<int>() : FrictionType::Patch;
//			physicsSettings.SolverIterations = physicsNode["SolverPositionIterations"] ? physicsNode["SolverPositionIterations"].as<uint32_t>() : 8;
//			physicsSettings.SolverVelocityIterations = physicsNode["SolverVelocityIterations"] ? physicsNode["SolverVelocityIterations"].as<uint32_t>() : 2;
//
//#ifdef X2_DEBUG
//			physicsSettings.DebugOnPlay = physicsNode["DebugOnPlay"] ? physicsNode["DebugOnPlay"].as<bool>() : true;
//			physicsSettings.DebugType = physicsNode["DebugType"] ? (PhysicsDebugType)physicsNode["DebugType"].as<int>() : PhysicsDebugType::LiveDebug;
//#endif
//
//			auto physicsLayers = physicsNode["Layers"];
//
//			if (!physicsLayers)
//				physicsLayers = physicsNode["PhysicsLayers"]; // Temporary fix since I accidentially serialized physics layers under the wrong name until now... Will remove this in a month or so once most projects have been changed to the proper name
//
//			if (physicsLayers)
//			{
//				for (auto layer : physicsLayers)
//					PhysicsLayerManager::AddLayer(layer["Name"].as<std::string>(), false);
//
//				for (auto layer : physicsLayers)
//				{
//					PhysicsLayer& layerInfo = PhysicsLayerManager::GetLayer(layer["Name"].as<std::string>());
//					layerInfo.CollidesWithSelf = layer["CollidesWithSelf"].as<bool>(true);
//
//					auto collidesWith = layer["CollidesWith"];
//					if (collidesWith)
//					{
//						for (auto collisionLayer : collidesWith)
//						{
//							const auto& otherLayer = PhysicsLayerManager::GetLayer(collisionLayer["Name"].as<std::string>());
//							PhysicsLayerManager::SetLayerCollision(layerInfo.LayerID, otherLayer.LayerID, true);
//						}
//					}
//				}
//			}
//		}

		// Log
		auto logNode = rootNode["Log"];
		if (logNode)
		{
			auto& tags = Log::EnabledTags();
			for (auto node : logNode)
			{
				std::string name = node.first.as<std::string>();
				auto& details = tags[name];
				details.Enabled = node.second["Enabled"].as<bool>();
				details.LevelFilter = Log::LevelFromString(node.second["LevelFilter"].as<std::string>());
			}
		}

		m_Project->OnDeserialized();
		return true;
	}

}
