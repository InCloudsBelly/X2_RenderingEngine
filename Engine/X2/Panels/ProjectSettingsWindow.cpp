#include "Precompiled.h"
#include "ProjectSettingsWindow.h"

#include "X2/ImGui/ImGui.h"

#include "X2/Project/ProjectSerializer.h"
#include "X2/Asset/AssetManager.h"

#include "X2/Core/Input.h"
#include "X2/Renderer/Renderer.h"


namespace X2 {

	ImFont* g_BoldFont = nullptr;

	static bool s_SerializeProject = false;

	ProjectSettingsWindow::ProjectSettingsWindow()
	{
		memset(m_NewLayerNameBuffer, 0, 255);
	}

	ProjectSettingsWindow::~ProjectSettingsWindow()
	{
	}

	void ProjectSettingsWindow::OnImGuiRender(bool& isOpen)
	{
		if (m_Project == nullptr)
		{
			isOpen = false;
			return;
		}

		if (ImGui::Begin("Project Settings", &isOpen))
		{
			RenderGeneralSettings();
			RenderRendererSettings();
			RenderLogSettings();

		}

		ImGui::End();

		if (s_SerializeProject)
		{
			ProjectSerializer serializer(m_Project);
			serializer.Serialize(m_Project->m_Config.ProjectDirectory + "/" + m_Project->m_Config.ProjectFileName);
			s_SerializeProject = false;
		}
	}

	void ProjectSettingsWindow::OnProjectChanged(const Ref<Project>& project)
	{
		m_Project = project;
		m_DefaultScene = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath(project->GetConfig().StartScene);
	}

	void ProjectSettingsWindow::RenderGeneralSettings()
	{
		ImGui::PushID("GeneralSettings");
		if (UI::PropertyGridHeader("General"))
		{
			UI::BeginPropertyGrid();

			{
				UI::ScopedDisable disable;
				UI::Property("Name", m_Project->m_Config.Name);
				UI::Property("Asset Directory", m_Project->m_Config.AssetDirectory);
				UI::Property("Asset Registry Path", m_Project->m_Config.AssetRegistryPath);
				UI::Property("Mesh Path", m_Project->m_Config.MeshPath);
				UI::Property("Mesh Source Path", m_Project->m_Config.MeshSourcePath);
				UI::Property("Project Directory", m_Project->m_Config.ProjectDirectory);
			}

			if (UI::Property("Auto-save active scene", m_Project->m_Config.EnableAutoSave))
				s_SerializeProject = true;

			if (UI::PropertySlider("Auto save interval (seconds)", m_Project->m_Config.AutoSaveIntervalSeconds, 60, 7200)) // 1 minute to 2 hours allowed range for auto-save.  Somewhat arbitrary...
				s_SerializeProject = true;

			if (UI::PropertyAssetReference<Scene>("Startup Scene", m_DefaultScene))
			{
				const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(m_DefaultScene);
				if (metadata.IsValid())
				{
					m_Project->m_Config.StartScene = metadata.FilePath.string();
					s_SerializeProject = true;
				}
			}

			UI::EndPropertyGrid();
			ImGui::TreePop();
		}
		else
			UI::ShiftCursorY(-(ImGui::GetStyle().ItemSpacing.y + 1.0f));

		ImGui::PopID();
	}

	void ProjectSettingsWindow::RenderRendererSettings()
	{
		ImGui::PushID("RendererSettings");
		if (UI::PropertyGridHeader("Renderer", false))
		{
			UI::BeginPropertyGrid();

			auto& rendererConfig = Renderer::GetConfig();

			UI::Property("Compute HDR Environment Maps", rendererConfig.ComputeEnvironmentMaps);

			static const char* mapSizes[] = { "128", "256", "512", "1024", "2048", "4096" };
			int32_t currentEnvMapSize = (int32_t)glm::log2((float)rendererConfig.EnvironmentMapResolution) - 7;
			if (UI::PropertyDropdown("Environment Map Size", mapSizes, 6, &currentEnvMapSize))
			{
				rendererConfig.EnvironmentMapResolution = (uint32_t)glm::pow(2, currentEnvMapSize + 7);
			}

			int32_t currentIrradianceMapSamples = (int32_t)glm::log2((float)rendererConfig.IrradianceMapComputeSamples) - 7;
			if (UI::PropertyDropdown("Irradiance Map Compute Samples", mapSizes, 6, &currentIrradianceMapSamples))
			{
				rendererConfig.IrradianceMapComputeSamples = (uint32_t)glm::pow(2, currentIrradianceMapSamples + 7);
			}

			UI::EndPropertyGrid();
			ImGui::TreePop();
		}
		else
			UI::ShiftCursorY(-(ImGui::GetStyle().ItemSpacing.y + 1.0f));

		ImGui::PopID();
	}

	void ProjectSettingsWindow::RenderLogSettings()
	{
		ImGui::PushID("LogSettings");
		if (UI::PropertyGridHeader("Log", false))
		{
			UI::BeginPropertyGrid(3);
			auto& tags = Log::EnabledTags();
			for (auto& [name, details] : tags)
			{
				// Don't show untagged log
				if (!name.empty())
				{
					ImGui::PushID(name.c_str());

					UI::Property(name.c_str(), details.Enabled);

					const char* levelStrings[] = { "Trace", "Info", "Warn", "Error", "Fatal" };
					int currentLevel = (int)details.LevelFilter;
					if (UI::PropertyDropdownNoLabel("LevelFilter", levelStrings, 5, &currentLevel))
						details.LevelFilter = (Log::Level)currentLevel;

					ImGui::PopID();
				}
			}
			UI::EndPropertyGrid();
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

