#include "Precompiled.h"
#include "ApplicationSettings.h"
#include "X2/Utilities/FileSystem.h"

#include <yaml-cpp/yaml.h>

#include <sstream>
#include <filesystem>

namespace X2 {

	static std::filesystem::path s_EditorSettingsPath;

	ApplicationSettings& ApplicationSettings::Get()
	{
		static ApplicationSettings s_Settings;
		return s_Settings;
	}

	void ApplicationSettingsSerializer::Init()
	{
		s_EditorSettingsPath = std::filesystem::absolute("Config");

		if (!FileSystem::Exists(s_EditorSettingsPath))
			FileSystem::CreateDirectory(s_EditorSettingsPath);
		s_EditorSettingsPath /= "ApplicationSettings.yaml";

		LoadSettings();
	}

#define X2_ENTER_GROUP(name) currentGroup = rootNode[name];
#define X2_READ_VALUE(name, type, var, defaultValue) var = currentGroup[name].as<type>(defaultValue)

	void ApplicationSettingsSerializer::LoadSettings()
	{
		// Generate default settings file if one doesn't exist
		if (!FileSystem::Exists(s_EditorSettingsPath))
		{
			SaveSettings();
			return;
		}

		std::ifstream stream(s_EditorSettingsPath);
		X2_CORE_VERIFY(stream);
		std::stringstream ss;
		ss << stream.rdbuf();

		YAML::Node data = YAML::Load(ss.str());
		if (!data["ApplicationSettings"])
			return;

		YAML::Node rootNode = data["ApplicationSettings"];
		YAML::Node currentGroup = rootNode;

		auto& settings = ApplicationSettings::Get();

		X2_ENTER_GROUP("X2");
		{
			X2_READ_VALUE("AdvancedMode", bool, settings.AdvancedMode, false);
		}

	
		X2_ENTER_GROUP("ContentBrowser");
		{
			X2_READ_VALUE("ShowAssetTypes", bool, settings.ContentBrowserShowAssetTypes, true);
			X2_READ_VALUE("ThumbnailSize", int, settings.ContentBrowserThumbnailSize, 128);
		}

		stream.close();
	}

#define X2_BEGIN_GROUP(name)\
		out << YAML::Key << name << YAML::Value << YAML::BeginMap;
#define X2_END_GROUP() out << YAML::EndMap;

#define X2_SERIALIZE_VALUE(name, value) out << YAML::Key << name << YAML::Value << value;

	void ApplicationSettingsSerializer::SaveSettings()
	{
		const auto& settings = ApplicationSettings::Get();

		YAML::Emitter out;
		out << YAML::BeginMap;
		X2_BEGIN_GROUP("ApplicationSettings");
		{
			X2_BEGIN_GROUP("X2");
			{
				X2_SERIALIZE_VALUE("AdvancedMode", settings.AdvancedMode);
			}
			X2_END_GROUP();

			X2_BEGIN_GROUP("ContentBrowser");
			{
				X2_SERIALIZE_VALUE("ShowAssetTypes", settings.ContentBrowserShowAssetTypes);
				X2_SERIALIZE_VALUE("ThumbnailSize", settings.ContentBrowserThumbnailSize);
			}
			X2_END_GROUP();
		}
		X2_END_GROUP();
		out << YAML::EndMap;

		std::ofstream fout(s_EditorSettingsPath);
		fout << out.c_str();
		fout.close();
	}


}
