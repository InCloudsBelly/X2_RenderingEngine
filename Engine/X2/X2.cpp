#include "EntryPoint.h"

#include "EditorLayer.h"
#include "X2/Utilities/FileSystem.h"

#include <Shlobj.h>

class X2Application : public X2::Application
{
public:
	X2Application(const X2::ApplicationSpecification& specification, std::string_view projectPath)
		: Application(specification), m_ProjectPath(projectPath), m_UserPreferences(X2::Ref<X2::UserPreferences>::Create())
	{
		if (projectPath.empty())
			m_ProjectPath = std::string(PROJECT_ROOT) + "Project/X2.xproj";
	}

	virtual void OnInit() override
	{
		// Persistent Storage
		{
			PWSTR roamingFilePath;
			HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &roamingFilePath);
			X2_CORE_ASSERT(result == S_OK);
			std::wstring filepath = roamingFilePath;
			std::replace(filepath.begin(), filepath.end(), L'\\', L'/');
			m_PersistentStoragePath = filepath + L"/X2";

			if (!std::filesystem::exists(m_PersistentStoragePath))
				std::filesystem::create_directory(m_PersistentStoragePath);
		}

		// User Preferences
		{
			X2::UserPreferencesSerializer serializer(m_UserPreferences);
			if (!std::filesystem::exists(m_PersistentStoragePath / "UserPreferences.yaml"))
				serializer.Serialize(m_PersistentStoragePath / "UserPreferences.yaml");
			else
				serializer.Deserialize(m_PersistentStoragePath / "UserPreferences.yaml");

			if (!m_ProjectPath.empty())
				m_UserPreferences->StartupProject = m_ProjectPath;
			else if (!m_UserPreferences->StartupProject.empty())
				m_ProjectPath = m_UserPreferences->StartupProject;
		}

		{
			std::filesystem::path workingDirectory = std::filesystem::current_path();

			if (workingDirectory.stem().string() == "X2")
				workingDirectory = workingDirectory.parent_path();

			X2::FileSystem::SetEnvironmentVariable("X2_DIR", workingDirectory.string());
		}

		PushLayer(new X2::EditorLayer(m_UserPreferences));
	}

private:
	std::string m_ProjectPath;
	std::filesystem::path m_PersistentStoragePath;
	X2::Ref<X2::UserPreferences> m_UserPreferences;
};

X2::Application* X2::CreateApplication(int argc, char** argv)
{
	std::string_view projectPath;
	if (argc > 1)
		projectPath = argv[1];

	X2::ApplicationSpecification specification;
	specification.Name = "X2";
	specification.WindowWidth = 1600;
	specification.WindowHeight = 900;
	specification.StartMaximized = true;
	specification.VSync = true;
	specification.RenderConfig.ShaderPackPath = "Resources/ShaderPack.xsp";


	specification.CoreThreadingPolicy = ThreadingPolicy::SingleThreaded;

	return new X2Application(specification, projectPath);
}
