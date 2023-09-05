#include "EditorLayer.h"

#include "X2/Scene/Prefab.h"
#include "X2/ImGui/ImGuizmo.h"
#include "X2/Renderer/Renderer2D.h"
#include "X2/Renderer/ShaderPack.h"
//#include "X2/Script/ScriptEngine.h"
//#include "X2/Script/ScriptBuilder.h"
#include "X2/Editor/AssetEditorPanel.h"
#include "X2/Editor/SelectionManager.h"
//#include "X2/Audio/Editor/AudioEventsEditor.h"
//#include "X2/Editor/ScriptEngineDebugPanel.h"

#include "X2/Core/Event/EditorEvent.h"
#include "X2/Core/Debug/Profiler.h"

#include "X2/Project/Project.h"
#include "X2/Project/ProjectSerializer.h"

#include <filesystem>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "X2/Utilities/FileSystem.h"
#include "X2/Math/Math.h"


//#include "X2/Physics/3D/PhysicsSystem.h"
//#include "X2/Physics/3D/Debug/PhysXDebugger.h"
//#include "X2/Physics/3D/PhysicsLayer.h"
//#include "X2/Audio/AudioEngine.h"
//#include "Panels/PhysicsStatsPanel.h"

#include "Panels/AssetManagerPanel.h"
#include "Panels/MaterialsPanel.h"
#include "Panels/SceneRendererPanel.h"

#include "X2/Vulkan/VulkanRenderer.h"

//#include "X2/Audio/AudioEvents/AudioCommandRegistry.h"

#include "imgui_internal.h"
#include "X2/ImGui/ImGui.h"

#include "X2/Serialization/AssetPack.h"

#include "X2/Asset/TextureRuntimeSerializer.h"

#include "GLFW/include/GLFW/glfw3.h"

namespace X2 {

#define MAX_PROJECT_NAME_LENGTH 255
#define MAX_PROJECT_FILEPATH_LENGTH 512

	static char* s_ProjectNameBuffer = new char[MAX_PROJECT_NAME_LENGTH];
	static char* s_OpenProjectFilePathBuffer = new char[MAX_PROJECT_FILEPATH_LENGTH];
	static char* s_NewProjectFilePathBuffer = new char[MAX_PROJECT_FILEPATH_LENGTH];

#define SCENE_HIERARCHY_PANEL_ID "SceneHierarchyPanel"
#define ECS_DEBUG_PANEL_ID "ECSDebugPanel"
#define CONSOLE_PANEL_ID "EditorConsolePanel"
#define CONTENT_BROWSER_PANEL_ID "ContentBrowserPanel"
#define PROJECT_SETTINGS_PANEL_ID "ProjectSettingsPanel"
//#define PHYSICS_DEBUG_PANEL_ID "PhysicsDebugPanel"
#define ASSET_MANAGER_PANEL_ID "AssetManagerPanel"
#define MATERIALS_PANEL_ID "MaterialsPanel"
//#define AUDIO_EVENTS_EDITOR_PANEL_ID "AudioEventsEditor"
#define APPLICATION_SETTINGS_PANEL_ID "ApplicationSettingsPanel"
//#define SCRIPT_ENGINE_DEBUG_PANEL_ID "ScriptEngineDebugPanel"
#define SCENE_RENDERER_PANEL_ID "SceneRendererPanel"

	static std::filesystem::path s_ProjectSolutionPath = "";

	EditorLayer::EditorLayer(const Ref<UserPreferences>& userPreferences)
		: m_UserPreferences(userPreferences), m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f), m_SecondEditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f)
	{
		for (auto it = m_UserPreferences->RecentProjects.begin(); it != m_UserPreferences->RecentProjects.end(); )
		{
			if (!std::filesystem::exists(it->second.FilePath))
				it = m_UserPreferences->RecentProjects.erase(it);
			else
				it++;
		}

	}

	EditorLayer::~EditorLayer()
	{
	}

	auto operator < (const ImVec2& lhs, const ImVec2& rhs)
	{
		return lhs.x < rhs.x&& lhs.y < rhs.y;
	}

	void EditorLayer::OnAttach()
	{
		using namespace glm;

		memset(s_ProjectNameBuffer, 0, MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);

		// Should we ever want to actually show editor layer panels in X2::Runtime
		// then these lines need to be added to RuntimeLayer::Attach()
		EditorResources::Init();

		/////////// Configure Panels ///////////
		m_PanelManager = CreateScope<PanelManager>();

		Ref<SceneHierarchyPanel> sceneHierarchyPanel = m_PanelManager->AddPanel<SceneHierarchyPanel>(PanelCategory::View, SCENE_HIERARCHY_PANEL_ID, "Scene Hierarchy", true, m_EditorScene, SelectionContext::Scene);
		sceneHierarchyPanel->SetEntityDeletedCallback([this](Entity entity) { OnEntityDeleted(entity); });
		sceneHierarchyPanel->SetMeshAssetConvertCallback([this](Entity entity, Ref<MeshSource> meshSource) { OnCreateMeshFromMeshSource(entity, meshSource); });
		sceneHierarchyPanel->SetInvalidMetadataCallback([this](Entity entity, AssetHandle handle) { SceneHierarchyInvalidMetadataCallback(entity, handle); });
		sceneHierarchyPanel->AddEntityContextMenuPlugin([this](Entity entity) { SceneHierarchySetEditorCameraTransform(entity); });

		Ref<ContentBrowserPanel> contentBrowser = m_PanelManager->AddPanel<ContentBrowserPanel>(PanelCategory::View, CONTENT_BROWSER_PANEL_ID, "Content Browser", true);
		contentBrowser->RegisterItemActivateCallbackForType(AssetType::Scene, [this](const AssetMetadata& metadata)
			{
				OpenScene(metadata);
			});

	/*	contentBrowser->RegisterItemActivateCallbackForType(AssetType::ScriptFile, [this](const AssetMetadata& metadata)
			{
				FileSystem::OpenExternally(Project::GetEditorAssetManager()->GetFileSystemPath(metadata));
			});*/

		/*contentBrowser->RegisterAssetCreatedCallback([this](const AssetMetadata& metadata)
			{
				if (metadata.Type == AssetType::ScriptFile)
					RegenerateProjectScriptSolution(Project::GetProjectDirectory());
			});

		contentBrowser->RegisterAssetDeletedCallback([this](const AssetMetadata& metadata)
			{
				if (metadata.Type == AssetType::ScriptFile)
					RegenerateProjectScriptSolution(Project::GetProjectDirectory());
			});*/

		m_PanelManager->AddPanel<ProjectSettingsWindow>(PanelCategory::Edit, PROJECT_SETTINGS_PANEL_ID, "Project Settings", false);
		//m_PanelManager->AddPanel<AudioEventsEditor>(PanelCategory::Edit, AUDIO_EVENTS_EDITOR_PANEL_ID, "Audio Events", false);
		m_PanelManager->AddPanel<ApplicationSettingsPanel>(PanelCategory::Edit, APPLICATION_SETTINGS_PANEL_ID, "Application Settings", false);
		m_PanelManager->AddPanel<ECSDebugPanel>(PanelCategory::View, ECS_DEBUG_PANEL_ID, "ECS Debug", false, m_EditorScene);
		m_PanelManager->AddPanel<EditorConsolePanel>(PanelCategory::View, CONSOLE_PANEL_ID, "Log", true);
		m_PanelManager->AddPanel<MaterialsPanel>(PanelCategory::View, MATERIALS_PANEL_ID, "Materials", true);
		//m_PanelManager->AddPanel<PhysicsStatsPanel>(PanelCategory::View, PHYSICS_DEBUG_PANEL_ID, "Physics Stats", false);
		m_PanelManager->AddPanel<AssetManagerPanel>(PanelCategory::View, ASSET_MANAGER_PANEL_ID, "Asset Manager", false);
		//m_PanelManager->AddPanel<ScriptEngineDebugPanel>(PanelCategory::View, SCRIPT_ENGINE_DEBUG_PANEL_ID, "Script Engine Debug", false);

		Ref<SceneRendererPanel> sceneRendererPanel = m_PanelManager->AddPanel<SceneRendererPanel>(PanelCategory::View, SCENE_RENDERER_PANEL_ID, "Scene Renderer", true);

		////////////////////////////////////////////////////////

		m_Renderer2D = Ref<Renderer2D>::Create();

		if (!m_UserPreferences->StartupProject.empty())
			OpenProject(m_UserPreferences->StartupProject);
		else
			X2_CORE_VERIFY(false, "No project provided!");

		if (!Project::GetActive())
			EmptyProject();

		if (false)
		{
			Ref<AssetPack> assetPack = AssetPack::LoadActiveProject();
			Ref<Scene> scene = assetPack->LoadScene(461541584939857254);
			m_CurrentScene = scene;
			m_EditorScene = scene;
			// NOTE(Peter): We set the scene context to nullptr here to make sure all old script entities have been destroyed
			//ScriptEngine::SetSceneContext(nullptr, nullptr);

			UpdateWindowTitle(scene->GetName());
			m_PanelManager->SetSceneContext(m_EditorScene);
			//ScriptEngine::SetSceneContext(m_EditorScene, m_ViewportRenderer);
			//MiniAudioEngine::SetSceneContext(m_EditorScene);
			AssetEditorPanel::SetSceneContext(m_EditorScene);
		}

		//AssetManager::UnloadAllAssetPacks();
		//AssetManager::AddAssetPack(assetPack);

		m_ViewportRenderer = Ref<SceneRenderer>::Create(m_CurrentScene);
		m_SecondViewportRenderer = Ref<SceneRenderer>::Create(m_CurrentScene);
		m_FocusedRenderer = m_ViewportRenderer;
		sceneRendererPanel->SetContext(m_FocusedRenderer);

		AssetEditorPanel::RegisterDefaultEditors();

		m_Renderer2D->SetLineWidth(m_LineWidth);
		m_ViewportRenderer->SetLineWidth(m_LineWidth);
		UpdateSceneRendererSettings();

		if (m_UserPreferences->ShowWelcomeScreen)
			UI_ShowWelcomePopup();

#if 0
		AssetPack assetPack(Project::GetAssetDirectory() / "AssetPack.hap");

		const auto& reg = AssetManager::GetAssetRegistry();
		for (const auto& [file, metadata] : reg.GetMap())
		{
			if (AssetManager::IsAssetHandleValid(metadata.Handle))
				assetPack.AddAsset(AssetManager::GetAsset<Asset>(metadata.Handle));
		}

		assetPack.Serialize();
#endif
	}

	void EditorLayer::OnDetach()
	{
		EditorResources::Shutdown();
		CloseProject(false);
		FileSystem::StopWatching();
		AssetEditorPanel::UnregisterAllEditors();
	}

	void EditorLayer::OnScenePlay()
	{
		SelectionManager::DeselectAll();

		m_SceneState = SceneState::Play;

		m_RuntimeScene = Ref<Scene>::Create();
		m_EditorScene->CopyTo(m_RuntimeScene);
		m_RuntimeScene->SetSceneTransitionCallback([this](const std::string& scene) { QueueSceneTransition(scene); });
		//ScriptEngine::SetSceneContext(m_RuntimeScene, m_ViewportRenderer);
		AssetEditorPanel::SetSceneContext(m_RuntimeScene);
		m_ViewportRenderer->GetOptions().ShowGrid = false;
		m_RuntimeScene->OnRuntimeStart();
		m_PanelManager->SetSceneContext(m_RuntimeScene);
		m_CurrentScene = m_RuntimeScene;
	}

	void EditorLayer::OnSceneStop()
	{
		SelectionManager::DeselectAll();

		m_RuntimeScene->OnRuntimeStop();
		m_SceneState = SceneState::Edit;
		m_ViewportRenderer->SetOpacity(1.0f);
		m_ViewportRenderer->GetOptions().ShowGrid = true;
		Input::SetCursorMode(CursorMode::Normal);

		// Unload runtime scene
		m_RuntimeScene = nullptr;

		//ScriptEngine::SetSceneContext(m_EditorScene, m_ViewportRenderer);
		AssetEditorPanel::SetSceneContext(m_EditorScene);
		//MiniAudioEngine::SetSceneContext(m_EditorScene);
		m_PanelManager->SetSceneContext(m_EditorScene);
		m_CurrentScene = m_EditorScene;
	}

	void EditorLayer::OnSceneStartSimulation()
	{
		SelectionManager::DeselectAll();

		m_SceneState = SceneState::Simulate;

		m_SimulationScene = Ref<Scene>::Create();
		m_EditorScene->CopyTo(m_SimulationScene);

		AssetEditorPanel::SetSceneContext(m_SimulationScene);
		m_SimulationScene->OnSimulationStart();
		m_PanelManager->SetSceneContext(m_SimulationScene);
		m_CurrentScene = m_SimulationScene;
	}

	void EditorLayer::OnSceneStopSimulation()
	{
		SelectionManager::DeselectAll();

		m_SimulationScene->OnSimulationStop();
		m_SceneState = SceneState::Edit;
		m_SimulationScene = nullptr;
		AssetEditorPanel::SetSceneContext(m_EditorScene);

		m_PanelManager->SetSceneContext(m_EditorScene);
		m_CurrentScene = m_EditorScene;
	}

	void EditorLayer::QueueSceneTransition(const std::string& scene)
	{
		m_PostSceneUpdateQueue.emplace_back([this, scene]() { OnSceneTransition(scene); });
	}

	void EditorLayer::BuildShaderPack()
	{
		ShaderPack::CreateFromLibrary(Renderer::GetShaderLibrary(), "Resources/ShaderPack.xsp");
		UI::ShowSimpleMessageBox<X2_MESSAGE_BOX_OK_BUTTON>(
			"Created ShaderPack!",
			"Successfully created shader pack. File Path: {0}", std::filesystem::absolute("Resources/ShaderPack.xsp").string()
			);
	}

	void EditorLayer::BuildAssetPack()
	{
		if (m_AssetPackBuildInProgress)
			return;

		X2_CONSOLE_LOG_INFO("Building Asset Pack...");

		m_AssetPackBuildProgress = 0.0f;
		std::packaged_task<void()> task([this] {
			AssetPack::CreateFromActiveProject(m_AssetPackBuildProgress);
			});

		m_AssetPackFuture = task.get_future();
		m_AssetPackThread = std::thread(std::move(task));
		m_AssetPackBuildInProgress = true;

		m_OpenAssetPackDialog = true;
#if 0
		UI::ShowSimpleMessageBox<X2_MESSAGE_BOX_OK_BUTTON>(
			"Created AssetPack!",
			"Successfully created asset pack. File Path: {0}", "path"
			);
#endif
	}

	//void EditorLayer::RegenerateProjectScriptSolution(const std::filesystem::path& projectPath)
	//{
	//	std::string batchFilePath = projectPath.string();
	//	std::replace(batchFilePath.begin(), batchFilePath.end(), '/', '\\'); // Only windows
	//	batchFilePath += "\\Win-CreateScriptProjects.bat";
	//	system(batchFilePath.c_str());
	//}

	void EditorLayer::SerializeEnvironmentMap()
	{
		auto skyLights = m_EditorScene->GetAllEntitiesWith<SkyLightComponent>();
		for (auto e : skyLights)
		{
			auto& skyLight = skyLights.get<SkyLightComponent>(e);
			if (!skyLight.DynamicSky && skyLight.SceneEnvironment)
			{
				if (AssetManager::IsAssetHandleValid(skyLight.SceneEnvironment))
				{
					Ref<Environment> environment = AssetManager::GetAsset<Environment>(skyLight.SceneEnvironment);
					if (environment->RadianceMap)
					{
						Ref<VulkanTextureCube> radianceMap = environment->RadianceMap;
						FileStreamWriter writer("RadianceMap.textureCube");
						TextureRuntimeSerializer::SerializeToFile(radianceMap, writer);
					}
				}
			}
		}
	}

	void EditorLayer::OnSceneTransition(const std::string& scene)
	{
		Ref<Scene> newScene = Ref<Scene>::Create();
		SceneSerializer serializer(newScene);
		if (serializer.Deserialize((Project::GetAssetDirectory() / scene).string()))
		{
			newScene->SetViewportSize(m_ViewportRenderer->GetViewportWidth(), m_ViewportRenderer->GetViewportHeight());

			m_RuntimeScene->OnRuntimeStop();

			SelectionManager::DeselectAll();

			m_RuntimeScene = newScene;
			m_RuntimeScene->SetSceneTransitionCallback([this](const std::string& scene) { QueueSceneTransition(scene); });
			//ScriptEngine::SetSceneContext(m_RuntimeScene, m_ViewportRenderer);
			AssetEditorPanel::SetSceneContext(m_RuntimeScene);
			m_RuntimeScene->OnRuntimeStart();
			m_PanelManager->SetSceneContext(m_RuntimeScene);
			m_CurrentScene = m_RuntimeScene;
		}
		else
		{
			X2_CORE_ERROR("Could not deserialize scene {0}", scene);
		}
	}

	void EditorLayer::UpdateWindowTitle(const std::string& sceneName)
	{
		//X2_CORE_VERIFY(RendererAPI::Current() == RendererAPIType::Vulkan, "Only Vulkan is supported!");
		const std::string rendererAPI = "Vulkan";
		const std::string title = fmt::format("{0} ({1}) - X2 - {2} ({3}) Renderer: {4}", sceneName, Project::GetActive()->GetConfig().Name, Application::GetPlatformName(), Application::GetConfigurationName(), rendererAPI);
		Application::Get().GetWindow().SetTitle(title);
	}

	void EditorLayer::UI_DrawMenubar()
	{
		const ImRect menuBarRect = { ImGui::GetCursorPos(), {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeightWithSpacing()} };

		ImGui::BeginGroup();

		if (UI::BeginMenuBar(menuBarRect))
		{
			bool menuOpen = ImGui::IsPopupOpen("##menubar", ImGuiPopupFlags_AnyPopupId);

			if (menuOpen)
			{
				const ImU32 colActive = UI::ColourWithSaturation(Colours::Theme::accent, 0.5f);
				ImGui::PushStyleColor(ImGuiCol_Header, colActive);
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colActive);
			}

			auto popItemHighlight = [&menuOpen]
			{
				if (menuOpen)
				{
					ImGui::PopStyleColor(3);
					menuOpen = false;
				}
			};

			auto pushDarkTextIfActive = [](const char* menuName)
			{
				if (ImGui::IsPopupOpen(menuName))
				{
					ImGui::PushStyleColor(ImGuiCol_Text, Colours::Theme::backgroundDark);
					return true;
				}
				return false;
			};

			const ImU32 colHovered = IM_COL32(0, 0, 0, 80);

			{
				bool colourPushed = pushDarkTextIfActive("File");

				if (ImGui::BeginMenu("File"))
				{
					popItemHighlight();
					colourPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					if (ImGui::MenuItem("Create Project..."))
						UI_ShowNewProjectPopup();
					if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
						OpenProject();
					if (ImGui::BeginMenu("Open Recent"))
					{
						size_t i = 0;
						for (auto it = m_UserPreferences->RecentProjects.begin(); it != m_UserPreferences->RecentProjects.end(); it++)
						{
							if (i > 10)
								break;

							if (ImGui::MenuItem(it->second.Name.c_str()))
							{
								// stash filepath away and defer actual opening of project until it is "safe" to do so
								strcpy(s_OpenProjectFilePathBuffer, it->second.FilePath.data());

								RecentProject projectEntry;
								projectEntry.Name = it->second.Name;
								projectEntry.FilePath = it->second.FilePath;
								projectEntry.LastOpened = time(NULL);

								it = m_UserPreferences->RecentProjects.erase(it);

								m_UserPreferences->RecentProjects[projectEntry.LastOpened] = projectEntry;

								UserPreferencesSerializer preferencesSerializer(m_UserPreferences);
								preferencesSerializer.Serialize(m_UserPreferences->FilePath);

								break;
							}

							i++;
						}
						ImGui::EndMenu();
					}
					if (ImGui::MenuItem("Save Project"))
						SaveProject();
					ImGui::Separator();
					if (ImGui::MenuItem("New Scene"))
						UI_ShowNewScenePopup();
					if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
						SaveScene();
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
						SaveSceneAs();

					ImGui::Separator();
					if (ImGui::MenuItem("Build shader pack"))
						BuildShaderPack();
					/*if (ImGui::MenuItem("Build Sound Bank"))
					{
						if (!MiniAudioEngine::BuildSoundBank())
							UI::ShowSimpleMessageBox<X2_MESSAGE_BOX_OK_BUTTON>(":[", "Failed to build Sound Bank.");
					}
					if (ImGui::MenuItem("Unload current Sound Bank"))
					{
						MiniAudioEngine::UnloadCurrentSoundBank();
					}*/
					if (ImGui::MenuItem("Build asset pack"))
						BuildAssetPack();
					if (ImGui::MenuItem("Serialize Environment Map"))
						SerializeEnvironmentMap();
					ImGui::Separator();
					if (ImGui::MenuItem("Exit", "Alt + F4"))
						Application::Get().Close();

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colourPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colourPushed = pushDarkTextIfActive("Edit");

				if (ImGui::BeginMenu("Edit"))
				{
					popItemHighlight();
					colourPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					for (auto& [id, panelData] : m_PanelManager->GetPanels(PanelCategory::Edit))
						ImGui::MenuItem(panelData.Name, nullptr, &panelData.IsOpen);

					ImGui::Separator();

					if (ImGui::MenuItem("Open Visual Studio Solution", nullptr, nullptr, FileSystem::Exists(s_ProjectSolutionPath)))
						FileSystem::OpenExternally(s_ProjectSolutionPath);

				/*	if (ImGui::MenuItem("Regenerate Visual Studio Solution"))
						RegenerateProjectScriptSolution(Project::GetProjectDirectory());*/

					/*if (ImGui::MenuItem("Build C# Assembly", nullptr, nullptr, FileSystem::Exists(s_ProjectSolutionPath)))
						ScriptBuilder::BuildScriptAssembly(Project::GetActive());*/

					ImGui::MenuItem("Second Viewport", nullptr, &m_ShowSecondViewport);
				/*	if (ImGui::MenuItem("Reload C# Assembly"))
					{
						ScriptEngine::ReloadAppAssembly();

					}*/

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colourPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colourPushed = pushDarkTextIfActive("View");

				if (ImGui::BeginMenu("View"))
				{
					popItemHighlight();
					colourPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					for (auto& [id, panelData] : m_PanelManager->GetPanels(PanelCategory::View))
					{
						if (ImGui::MenuItem(panelData.Name, nullptr, &panelData.IsOpen))
							m_PanelManager->Serialize();
					}

					ImGui::MenuItem("Statistics", nullptr, &m_ShowStatisticsPanel);

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colourPushed)
					ImGui::PopStyleColor();
			}

#ifdef X2_DEBUG
			{
				bool colourPushed = pushDarkTextIfActive("Debug");

				if (ImGui::BeginMenu("Debug"))
				{
					popItemHighlight();
					colourPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					/*if (PhysXDebugger::IsDebugging())
					{
						if (ImGui::MenuItem("Stop PhysX Debugging"))
							PhysXDebugger::StopDebugging();
					}
					else
					{
						if (ImGui::MenuItem("Start PhysX Debugging"))
							PhysXDebugger::StartDebugging((Project::GetActive()->GetProjectDirectory() / "PhysXDebugInfo").string(), PhysicsSystem::GetSettings().DebugType == PhysicsDebugType::LiveDebug);
					}*/

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colourPushed)
					ImGui::PopStyleColor();
			}
#endif

			{
				bool colourPushed = pushDarkTextIfActive("Tools");

				if (ImGui::BeginMenu("Tools"))
				{
					popItemHighlight();
					colourPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					ImGui::MenuItem("ImGui Metrics Tool", nullptr, &m_ShowMetricsTool);
					ImGui::MenuItem("ImGui Stack Tool", nullptr, &m_ShowStackTool);
					ImGui::MenuItem("ImGui Style Editor", nullptr, &m_ShowStyleEditor);

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colourPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colourPushed = pushDarkTextIfActive("Help");

				if (ImGui::BeginMenu("Help"))
				{
					popItemHighlight();
					colourPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					if (ImGui::MenuItem("About"))
						UI_ShowAboutPopup();

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colourPushed)
					ImGui::PopStyleColor();
			}

			if (menuOpen)
				ImGui::PopStyleColor(2);
		}
		UI::EndMenuBar();

		ImGui::EndGroup();
	}

	float EditorLayer::UI_DrawTitlebar()
	{
		const float titlebarHeight = 57.0f;
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

		ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y));
		const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
		const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f,
									 ImGui::GetCursorScreenPos().y + titlebarHeight };
		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(titlebarMin, titlebarMax, Colours::Theme::titlebar);

		// Logo
		{
			const int logoWidth = EditorResources::X2LogoTexture->GetWidth();
			const int logoHeight = EditorResources::X2LogoTexture->GetHeight();
			const ImVec2 logoOffset(16.0f + windowPadding.x, 8.0f + windowPadding.y);
			const ImVec2 logoRectStart = { ImGui::GetItemRectMin().x + logoOffset.x, ImGui::GetItemRectMin().y + logoOffset.y };
			const ImVec2 logoRectMax = { logoRectStart.x + logoWidth, logoRectStart.y + logoHeight };
			drawList->AddImage(UI::GetTextureID(EditorResources::X2LogoTexture), logoRectStart, logoRectMax);
		}

		ImGui::BeginHorizontal("Titlebar", { ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetFrameHeightWithSpacing() });

		static float moveOffsetX;
		static float moveOffsetY;
		const float w = ImGui::GetContentRegionAvail().x;
		const float buttonsAreaWidth = 94;

		// Title bar drag area

		// On Windows we hook into the GLFW win32 window internals
#ifdef X2_PLATFORM_WINDOWS

		ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight));
		m_TitleBarHovered = ImGui::IsItemHovered() && (Input::GetCursorMode() != CursorMode::Locked);

#else
		auto* rootWindow = ImGui::GetCurrentWindow()->RootWindow;
		const float windowWidth = (int)rootWindow->RootWindow->Size.x;

		if (ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight), ImGuiButtonFlags_PressedOnClick))
		{
			ImVec2 point = ImGui::GetMousePos();
			ImRect rect = rootWindow->Rect();
			// Calculate the difference between the cursor pos and window pos
			moveOffsetX = point.x - rect.Min.x;
			moveOffsetY = point.y - rect.Min.y;

		}

		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
		{
			auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			bool maximized = (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
			if (maximized)
				glfwRestoreWindow(window);
			else
				Application::Get().GetWindow().Maximize();
		}
		else if (ImGui::IsItemActive())
		{
			// TODO: move this stuff to a better place, like Window class
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
				int maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
				if (maximized)
				{
					glfwRestoreWindow(window);

					int newWidth, newHeight;
					glfwGetWindowSize(window, &newWidth, &newHeight);

					// Offset position proportionally to mouse position on titlebar
					// This ensures we dragging window relatively to cursor position on titlebar
					// correctly when window size changes
					if (windowWidth - (float)newWidth > 0.0f)
						moveOffsetX *= (float)newWidth / windowWidth;
				}

				ImVec2 point = ImGui::GetMousePos();
				glfwSetWindowPos(window, point.x - moveOffsetX, point.y - moveOffsetY);

			}
		}
#endif

		// Draw Menubar

		ImGui::SuspendLayout();
		{
			ImGui::SetItemAllowOverlap();
			const float logoOffset = 16.0f * 2.0f + 41.0f + windowPadding.x;
			ImGui::SetCursorPos(ImVec2(logoOffset, 4.0f));
			UI_DrawMenubar();

			if (ImGui::IsItemHovered())
				m_TitleBarHovered = false;
		}

		const float menuBarRight = ImGui::GetItemRectMax().x - ImGui::GetCurrentWindow()->Pos.x;

		// Project name
		{

			UI::ScopedColour textColour(ImGuiCol_Text, Colours::Theme::textDarker);
			UI::ScopedColour border(ImGuiCol_Border, IM_COL32(40, 40, 40, 255));

			const std::string title = Project::GetActive()->GetConfig().Name;
			const ImVec2 textSize = ImGui::CalcTextSize(title.c_str());
			const float rightOffset = ImGui::GetWindowWidth() / 5.0f;
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightOffset - textSize.x);
			UI::ShiftCursorY(1.0f + windowPadding.y);

			{
				UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);
				ImGui::Text(title.c_str());
			}
			UI::SetTooltip("Current project (" + Project::GetActive()->GetConfig().ProjectFileName + ")");

			UI::DrawBorder(UI::RectExpanded(UI::GetItemRect(), 24.0f, 68.0f), 1.0f, 3.0f, 0.0f, -60.0f);
		}

		// Current Scene name
		{
			UI::ScopedColour textColour(ImGuiCol_Text, Colours::Theme::text);
			//const std::string sceneName = m_CurrentScene->GetName();
			const std::string sceneName = Utils::RemoveExtension(std::filesystem::path(m_SceneFilePath).filename().string());

			ImGui::SetCursorPosX(menuBarRight);
			UI::ShiftCursorX(50.0f);

			{
				UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);
				ImGui::Text(sceneName.c_str());
			}
			UI::SetTooltip("Current scene (" + m_SceneFilePath + ")");

			const float underlineThickness = 2.0f;
			const float underlineExpandWidth = 4.0f;
			ImRect itemRect = UI::RectExpanded(UI::GetItemRect(), underlineExpandWidth, 0.0f);

			// Horizontal line
			//itemRect.Min.y = itemRect.Max.y - underlineThickness;
			//itemRect = UI::RectOffset(itemRect, 0.0f, underlineThickness * 2.0f);

			// Vertical line
			itemRect.Max.x = itemRect.Min.x + underlineThickness;
			itemRect = UI::RectOffset(itemRect, -underlineThickness * 2.0f, 0.0f);

			drawList->AddRectFilled(itemRect.Min, itemRect.Max, Colours::Theme::muted, 2.0f);
		}
		ImGui::ResumeLayout();

		// Window buttons
		const ImU32 buttonColN = UI::ColourWithMultipliedValue(Colours::Theme::text, 0.9f);
		const ImU32 buttonColH = UI::ColourWithMultipliedValue(Colours::Theme::text, 1.2f);
		const ImU32 buttonColP = Colours::Theme::textDarker;
		const float buttonWidth = 14.0f;
		const float buttonHeight = 14.0f;

		// Minimize Button

		ImGui::Spring();
		UI::ShiftCursorY(8.0f);
		{
			const int iconWidth = EditorResources::MinimizeIcon->GetWidth();
			const int iconHeight = EditorResources::MinimizeIcon->GetHeight();
			const float padY = (buttonHeight - (float)iconHeight) / 2.0f;
			if (ImGui::InvisibleButton("Minimize", ImVec2(buttonWidth, buttonHeight)))
			{
				// TODO: move this stuff to a better place, like Window class
				if (auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()))
				{
					Application::Get().QueueEvent([window]() { glfwIconifyWindow(window); });
				}
			}

			UI::DrawButtonImage(EditorResources::MinimizeIcon, buttonColN, buttonColH, buttonColP, UI::RectExpanded(UI::GetItemRect(), 0.0f, -padY));
		}


		// Maximize Button

		ImGui::Spring(-1.0f, 17.0f);
		UI::ShiftCursorY(8.0f);
		{
			const int iconWidth = EditorResources::MaximizeIcon->GetWidth();
			const int iconHeight = EditorResources::MaximizeIcon->GetHeight();

			auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			bool isMaximized = (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

			if (ImGui::InvisibleButton("Maximize", ImVec2(buttonWidth, buttonHeight)))
			{
				// TODO: move this stuff to a better place, like Window class
				if (isMaximized)
				{
					Application::Get().QueueEvent([window]() { glfwRestoreWindow(window); });
				}
				else
				{
					Application::Get().QueueEvent([]() { Application::Get().GetWindow().Maximize(); });
				}
			}

			UI::DrawButtonImage(isMaximized ? EditorResources::RestoreIcon : EditorResources::MaximizeIcon, buttonColN, buttonColH, buttonColP);
		}


		// Close Button

		ImGui::Spring(-1.0f, 15.0f);
		UI::ShiftCursorY(8.0f);
		{
			const int iconWidth = EditorResources::CloseIcon->GetWidth();
			const int iconHeight = EditorResources::CloseIcon->GetHeight();
			if (ImGui::InvisibleButton("Close", ImVec2(buttonWidth, buttonHeight)))
			{
				Application::Get().DispatchEvent<WindowCloseEvent>();
			}

			UI::DrawButtonImage(EditorResources::CloseIcon, Colours::Theme::text, UI::ColourWithMultipliedValue(Colours::Theme::text, 1.4f), buttonColP);
		}

		ImGui::Spring(-1.0f, 18.0f);
		ImGui::EndHorizontal();

		return titlebarHeight;
	}

	void EditorLayer::UI_HandleManualWindowResize()
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		const bool maximized = (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

		ImVec2 newSize, newPosition;
		if (!maximized && UI::UpdateWindowManualResize(ImGui::GetCurrentWindow(), newSize, newPosition))
		{
			// On Windows we hook into the GLFW win32 window internals
#ifndef X2_PLATFORM_WINDOWS

			glfwSetWindowPos(window, newPosition.x, newPosition.y);
			glfwSetWindowSize(window, newSize.x, newSize.y);
#endif
		}
	}

	bool EditorLayer::UI_TitleBarHitTest(int /*x*/, int /*y*/) const
	{
		return m_TitleBarHovered;
	}

	void EditorLayer::UI_GizmosToolbar()
	{
		if (!m_ShowGizmosInPlayMode && m_CurrentScene == m_RuntimeScene)
			return;

		UI::PushID();

		UI::ScopedStyle disableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		UI::ScopedStyle disableWindowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
		UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
		UI::ScopedStyle disablePadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const float buttonSize = 18.0f;
		const float edgeOffset = 4.0f;
		const float windowHeight = 32.0f; // annoying limitation of ImGui, window can't be smaller than 32 pixels
		const float numberOfButtons = 4.0f;
		const float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

		ImGui::SetNextWindowPos(ImVec2(m_ViewportBounds[0].x + 14, m_ViewportBounds[0].y + edgeOffset));
		ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##viewport_tools", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

		// A hack to make icon panel appear smaller than minimum allowed by ImGui size
		// Filling the background for the desired 26px height
		const float desiredHeight = 26.0f;
		ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) / 2.0f);
		ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

		ImGui::BeginVertical("##gizmosV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
		ImGui::Spring();
		ImGui::BeginHorizontal("##gizmosH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
		ImGui::Spring();
		{
			UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));

			const ImColor c_SelectedGizmoButtonColor = Colours::Theme::accent;
			const ImColor c_UnselectedGizmoButtonColor = Colours::Theme::textBrighter;

			auto gizmoButton = [&c_SelectedGizmoButtonColor, buttonSize](const Ref<VulkanTexture2D>& icon, const ImColor& tint, float paddingY = 0.0f)
			{
				const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
				const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
				const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
				UI::DrawButtonImage(icon,
					tint,
					tint,
					tint,
					UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

				return clicked;
			};

			ImColor buttonTint = m_GizmoType == -1 ? c_SelectedGizmoButtonColor : c_UnselectedGizmoButtonColor;
			if (gizmoButton(EditorResources::PointerIcon, buttonTint))
				m_GizmoType = -1;
			UI::SetTooltip("Select");

			buttonTint = m_GizmoType == ImGuizmo::OPERATION::TRANSLATE ? c_SelectedGizmoButtonColor : c_UnselectedGizmoButtonColor;
			if (gizmoButton(EditorResources::MoveIcon, buttonTint))
				m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
			UI::SetTooltip("Translate");

			buttonTint = m_GizmoType == ImGuizmo::OPERATION::ROTATE ? c_SelectedGizmoButtonColor : c_UnselectedGizmoButtonColor;
			if (gizmoButton(EditorResources::RotateIcon, buttonTint))
				m_GizmoType = ImGuizmo::OPERATION::ROTATE;
			UI::SetTooltip("Rotate");

			buttonTint = m_GizmoType == ImGuizmo::OPERATION::SCALE ? c_SelectedGizmoButtonColor : c_UnselectedGizmoButtonColor;
			if (gizmoButton(EditorResources::ScaleIcon, buttonTint))
				m_GizmoType = ImGuizmo::OPERATION::SCALE;
			UI::SetTooltip("Scale");

		}
		ImGui::Spring();
		ImGui::EndHorizontal();
		ImGui::Spring();
		ImGui::EndVertical();

		ImGui::End();

		UI::PopID();
	}

	void EditorLayer::UI_CentralToolbar()
	{
		UI::PushID();

		UI::ScopedStyle disableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		UI::ScopedStyle disableWindowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
		UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
		UI::ScopedStyle disablePadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const float buttonSize = 18.0f + 5.0f;
		const float edgeOffset = 4.0f;
		const float windowHeight = 32.0f; // annoying limitation of ImGui, window can't be smaller than 32 pixels
		const float numberOfButtons = 3.0f;
		const float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

		float toolbarX = (m_ViewportBounds[0].x + m_ViewportBounds[1].x) / 2.0f;
		ImGui::SetNextWindowPos(ImVec2(toolbarX - (backgroundWidth / 2.0f), m_ViewportBounds[0].y + edgeOffset));
		ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##viewport_central_toolbar", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

		// A hack to make icon panel appear smaller than minimum allowed by ImGui size
		// Filling the background for the desired 26px height
		const float desiredHeight = 26.0f + 5.0f;
		ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) / 2.0f);
		ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

		ImGui::BeginVertical("##viewport_central_toolbarV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
		ImGui::Spring();
		ImGui::BeginHorizontal("##viewport_central_toolbarH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
		ImGui::Spring();
		{
			UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));

			const ImColor c_ButtonTint = Colours::Theme::text;
			const ImColor c_SimulateButtonTint = m_SceneState == SceneState::Simulate ? ImColor(1.0f, 0.75f, 0.75f, 1.0f) : c_ButtonTint;

			auto toolbarButton = [buttonSize](const Ref<VulkanTexture2D>& icon, const ImColor& tint, float paddingY = 0.0f)
			{
				const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
				const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
				const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
				UI::DrawButtonImage(icon,
					tint,
					tint,
					tint,
					UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

				return clicked;
			};

			Ref<VulkanTexture2D> buttonTex = m_SceneState == SceneState::Play ? EditorResources::StopIcon : EditorResources::PlayIcon;
			if (toolbarButton(buttonTex, c_ButtonTint))
			{
				if (m_SceneState == SceneState::Edit)
					OnScenePlay();
				else if (m_SceneState != SceneState::Simulate)
					OnSceneStop();
			}
			UI::SetTooltip(m_SceneState == SceneState::Edit ? "Play" : "Stop");

			if (toolbarButton(EditorResources::SimulateIcon, c_SimulateButtonTint))
			{
				if (m_SceneState == SceneState::Edit)
					OnSceneStartSimulation();
				else if (m_SceneState == SceneState::Simulate)
					OnSceneStopSimulation();
			}
			UI::SetTooltip(m_SceneState == SceneState::Simulate ? "Stop" : "Simulate Physics");

			if (toolbarButton(EditorResources::PauseIcon, c_ButtonTint))
			{
				if (m_SceneState == SceneState::Play)
					m_SceneState = SceneState::Pause;
				else if (m_SceneState == SceneState::Pause)
					m_SceneState = SceneState::Play;
			}
			UI::SetTooltip(m_SceneState == SceneState::Pause ? "Resume" : "Pause");
		}
		ImGui::Spring();
		ImGui::EndHorizontal();
		ImGui::Spring();
		ImGui::EndVertical();

		ImGui::End();

		UI::PopID();
	}

	void EditorLayer::UI_ViewportSettings()
	{
		UI::PushID();

		UI::ScopedStyle disableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		UI::ScopedStyle disableWindowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
		UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
		UI::ScopedStyle disablePadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const float buttonSize = 18.0f;
		const float edgeOffset = 2.0f;
		const float windowHeight = 32.0f; // annoying limitation of ImGui, window can't be smaller than 32 pixels
		const float numberOfButtons = 1.0f;
		const float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

		ImGui::SetNextWindowPos(ImVec2(m_ViewportBounds[1].x - backgroundWidth - 14, m_ViewportBounds[0].y + edgeOffset));
		ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##viewport_settings", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

		// A hack to make icon panel appear smaller than minimum allowed by ImGui size
		// Filling the background for the desired 26px height
		const float desiredHeight = 26.0f;
		ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) / 2.0f);
		ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

		bool openSettingsPopup = false;

		ImGui::BeginVertical("##viewportSettingsV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
		ImGui::Spring();
		ImGui::BeginHorizontal("##viewportSettingsH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
		ImGui::Spring();
		{
			UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));

			const ImColor c_SelectedGizmoButtonColor = Colours::Theme::accent;
			const ImColor c_UnselectedGizmoButtonColor = Colours::Theme::textBrighter;

			auto imageButton = [&c_SelectedGizmoButtonColor, buttonSize](const Ref<VulkanTexture2D>& icon, const ImColor& tint, float paddingY = 0.0f)
			{
				const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
				const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
				const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
				UI::DrawButtonImage(icon,
					tint,
					tint,
					tint,
					UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

				return clicked;
			};

			if (imageButton(EditorResources::GearIcon, c_UnselectedGizmoButtonColor))
				openSettingsPopup = true;
			UI::SetTooltip("Viewport Settings");
		}
		ImGui::Spring();
		ImGui::EndHorizontal();
		ImGui::Spring();
		ImGui::EndVertical();

		// Draw the settings popup
		{
			int32_t sectionIdx = 0;

			static float popupWidth = 300.0f;

			auto beginSection = [&sectionIdx](const char* name)
			{
				if (sectionIdx > 0)
					UI::ShiftCursorY(5.5f);

				UI::Fonts::PushFont("Bold");
				ImGui::TextUnformatted(name);
				UI::Fonts::PopFont();
				UI::Draw::Underline(Colours::Theme::backgroundDark);
				UI::ShiftCursorY(3.5f);

				bool result = ImGui::BeginTable("##section_table", 2, ImGuiTableFlags_SizingStretchSame);
				if (result)
				{
					ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.5f);
					ImGui::TableSetupColumn("Widgets", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.5f);
				}

				sectionIdx++;
				return result;
			};

			auto endSection = []()
			{
				ImGui::EndTable();
			};

			auto slider = [](const char* label, float& value, float min = 0.0f, float max = 0.0f)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(label);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);
				return ImGui::SliderFloat(UI::GenerateID(), &value, min, max);
			};

			auto drag = [](const char* label, float& value, float delta = 1.0f, float min = 0.0f, float max = 0.0f)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(label);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);
				return ImGui::DragFloat(UI::GenerateID(), &value, delta, min, max);
			};

			auto checkbox = [](const char* label, bool& value)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(label);
				ImGui::TableSetColumnIndex(1);
				auto table = ImGui::GetCurrentTable();
				float columnWidth = ImGui::TableGetMaxColumnWidth(table, 1);
				UI::ShiftCursorX(columnWidth - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x);
				return ImGui::Checkbox(UI::GenerateID(), &value);
			};

			auto dropdown = [](const char* label, const char** options, int32_t optionCount, int32_t* selected)
			{
				const char* current = options[*selected];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(label);
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-1);

				bool result = false;
				if (ImGui::BeginCombo(UI::GenerateID(), current))
				{
					for (int i = 0; i < optionCount; i++)
					{
						const bool is_selected = (current == options[i]);
						if (ImGui::Selectable(options[i], is_selected))
						{
							current = options[i];
							*selected = i;
							result = true;
						}

						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::PopItemWidth();

				return result;
			};

			UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 5.5f));
			UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
			UI::ScopedStyle windowRounding(ImGuiStyleVar_PopupRounding, 4.0f);
			UI::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 5.5f));

			if (openSettingsPopup)
				ImGui::OpenPopup("ViewportSettingsPanel");

			ImGui::SetNextWindowPos({ (m_ViewportBounds[1].x - popupWidth) - 34, m_ViewportBounds[0].y + edgeOffset + windowHeight });
			if (ImGui::BeginPopup("ViewportSettingsPanel", ImGuiWindowFlags_NoMove))
			{
				// NOTE(Peter): This method is currently only called for the main viewport because the second viewport is currently a bit broken
				//				We may want to render these settings for both viewports at some point, and tbh that would be really easy to do
				auto& viewportRenderOptions = m_ViewportRenderer->GetOptions();

				if (beginSection("General"))
				{
					static const char* selectionModes[] = { "Entity", "Submesh" };
					dropdown("Selection Mode", selectionModes, 2, (int32_t*)&m_SelectionMode);

					static const char* s_TransformTargetNames[] = { "Median Point", "Individual Origins" };
					dropdown("Multi-Transform Target", s_TransformTargetNames, 2, (int32_t*)&m_MultiTransformTarget);

					endSection();
				}

				if (beginSection("Display"))
				{
					checkbox("Show Icons", m_ShowIcons);
					checkbox("Show Gizmos", m_ShowGizmos);
					checkbox("Show Gizmos In Play Mode", m_ShowGizmosInPlayMode);
					checkbox("Show Bounding Boxes", m_ShowBoundingBoxes);
					if (m_ShowBoundingBoxes)
					{
						checkbox("Selected Entity", m_ShowBoundingBoxSelectedMeshOnly);

						if (m_ShowBoundingBoxSelectedMeshOnly)
							checkbox("Submeshes", m_ShowBoundingBoxSubmeshes);
					}

					checkbox("Show Grid", viewportRenderOptions.ShowGrid);
					checkbox("Show Selected Wireframe", viewportRenderOptions.ShowSelectedInWireframe);

					static const char* physicsColliderViewOptions[] = { "Selected Entity", "All" };
					checkbox("Show Physics Colliders", viewportRenderOptions.ShowPhysicsColliders);
					dropdown("Physics Collider Mode", physicsColliderViewOptions, 2, (int32_t*)&viewportRenderOptions.PhysicsColliderMode);
					checkbox("Show Colliders On Top", viewportRenderOptions.ShowPhysicsCollidersOnTop);

					if (drag("Line Width", m_LineWidth, 0.5f, 1.0f, 10.0f))
					{
						m_Renderer2D->SetLineWidth(m_LineWidth);
						m_ViewportRenderer->SetLineWidth(m_LineWidth);
					}

					endSection();
				}

				if (beginSection("Scene Camera"))
				{
					slider("Exposure", m_EditorCamera.GetExposure(), 0.0f, 5.0f);
					drag("Speed", m_EditorCamera.m_NormalSpeed, 0.001f, 0.0002f, 0.5f);

					endSection();
				}

				ImGui::EndPopup();
			}
		}

		ImGui::End();

		UI::PopID();
	}

	void EditorLayer::UI_DrawGizmos()
	{
		X2_PROFILE_FUNC();

		if (m_SelectionMode != SelectionMode::Entity || m_GizmoType == -1)
			return;

		const auto& selections = SelectionManager::GetSelections(SelectionContext::Scene);

		if (selections.empty())
			return;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

		bool snap = Input::IsKeyDown(X2_KEY_LEFT_CONTROL);

		float snapValue = GetSnapValue();
		float snapValues[3] = { snapValue, snapValue, snapValue };

		glm::mat4 projectionMatrix, viewMatrix;
		if (m_SceneState == SceneState::Play && !m_EditorCameraInRuntime)
		{
			Entity cameraEntity = m_CurrentScene->GetMainCameraEntity();
			SceneCamera& camera = cameraEntity.GetComponent<CameraComponent>();
			projectionMatrix = camera.GetProjectionMatrix();
			viewMatrix = glm::inverse(m_CurrentScene->GetWorldSpaceTransformMatrix(cameraEntity));
		}
		else
		{
			projectionMatrix = m_EditorCamera.GetProjectionMatrix();
			viewMatrix = m_EditorCamera.GetViewMatrix();
		}

		if (selections.size() == 1)
		{
			Entity entity = m_CurrentScene->GetEntityWithUUID(selections[0]);
			TransformComponent& entityTransform = entity.Transform();
			glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);

			if (ImGuizmo::Manipulate(
				glm::value_ptr(viewMatrix),
				glm::value_ptr(projectionMatrix),
				(ImGuizmo::OPERATION)m_GizmoType,
				ImGuizmo::LOCAL,
				glm::value_ptr(transform),
				nullptr,
				snap ? snapValues : nullptr)
				)
			{
				Entity parent = m_CurrentScene->TryGetEntityWithUUID(entity.GetParentUUID());
				if (parent)
				{
					glm::mat4 parentTransform = m_CurrentScene->GetWorldSpaceTransformMatrix(parent);
					transform = glm::inverse(parentTransform) * transform;
				}

				// Manipulated transform is now in local space of parent (= world space if no parent)
				// We can decompose into translation, rotation, and scale and compare with original
				// to figure out how to best update entity transform
				//
				// Why do we do this instead of just setting the entire entity transform?
				// Because it's more robust to set only those components of transform
				// that we are meant to be changing (dictated by m_GizmoType).  That way we avoid
				// small drift (particularly in rotation and scale) due numerical precision issues
				// from all those matrix operations.
				glm::vec3 translation;
				glm::quat rotation;
				glm::vec3 scale;
				Math::DecomposeTransform(transform, translation, rotation, scale);

				switch (m_GizmoType)
				{
				case ImGuizmo::TRANSLATE:
				{
					entityTransform.Translation = translation;
					break;
				}
				case ImGuizmo::ROTATE:
				{
					// Do this in Euler in an attempt to preserve any full revolutions (> 360)
					glm::vec3 originalRotationEuler = entityTransform.GetRotationEuler();

					// Map original rotation to range [-180, 180] which is what ImGuizmo gives us
					originalRotationEuler.x = fmodf(originalRotationEuler.x + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
					originalRotationEuler.y = fmodf(originalRotationEuler.y + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
					originalRotationEuler.z = fmodf(originalRotationEuler.z + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();

					glm::vec3 deltaRotationEuler = glm::eulerAngles(rotation) - originalRotationEuler;

					// Try to avoid drift due numeric precision
					if (fabs(deltaRotationEuler.x) < 0.001) deltaRotationEuler.x = 0.0f;
					if (fabs(deltaRotationEuler.y) < 0.001) deltaRotationEuler.y = 0.0f;
					if (fabs(deltaRotationEuler.z) < 0.001) deltaRotationEuler.z = 0.0f;

					entityTransform.SetRotationEuler(entityTransform.GetRotationEuler() += deltaRotationEuler);
					break;
				}
				case ImGuizmo::SCALE:
				{
					entityTransform.Scale = scale;
					break;
				}
				}
			}
		}
		else
		{
			if (m_MultiTransformTarget == TransformationTarget::MedianPoint && m_GizmoType == ImGuizmo::SCALE)
			{
				// NOTE(Peter): Disabling multi-entity scaling for median point mode for now since it causes strange scaling behavior
				return;
			}

			glm::vec3 medianLocation = glm::vec3(0.0f);
			glm::vec3 medianScale = glm::vec3(1.0f);
			glm::vec3 medianRotation = glm::vec3(0.0f);
			for (auto entityID : selections)
			{
				Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
				medianLocation += entity.Transform().Translation;
				medianScale += entity.Transform().Scale;
				medianRotation += entity.Transform().GetRotationEuler();
			}
			medianLocation /= selections.size();
			medianScale /= selections.size();
			medianRotation /= selections.size();

			glm::mat4 medianPointMatrix = glm::translate(glm::mat4(1.0f), medianLocation)
				* glm::toMat4(glm::quat(medianRotation))
				* glm::scale(glm::mat4(1.0f), medianScale);

			glm::mat4 deltaMatrix = glm::mat4(1.0f);

			if (ImGuizmo::Manipulate(
				glm::value_ptr(viewMatrix),
				glm::value_ptr(projectionMatrix),
				(ImGuizmo::OPERATION)m_GizmoType,
				ImGuizmo::LOCAL,
				glm::value_ptr(medianPointMatrix),
				glm::value_ptr(deltaMatrix),
				snap ? snapValues : nullptr)
				)
			{
				switch (m_MultiTransformTarget)
				{
				case TransformationTarget::MedianPoint:
				{
					for (auto entityID : selections)
					{
						Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
						TransformComponent& transform = entity.Transform();
						transform.SetTransform(deltaMatrix * transform.GetTransform());
					}

					break;
				}
				case TransformationTarget::IndividualOrigins:
				{
					glm::vec3 deltaTranslation, deltaScale;
					glm::quat deltaRotation;
					Math::DecomposeTransform(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);

					for (auto entityID : selections)
					{
						Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
						TransformComponent& transform = entity.Transform();

						switch (m_GizmoType)
						{
						case ImGuizmo::TRANSLATE:
						{
							transform.Translation += deltaTranslation;
							break;
						}
						case ImGuizmo::ROTATE:
						{
							transform.SetRotationEuler(transform.GetRotationEuler() + glm::eulerAngles(deltaRotation));
							break;
						}
						case ImGuizmo::SCALE:
						{
							if (deltaScale != glm::vec3(1.0f, 1.0f, 1.0f))
								transform.Scale *= deltaScale;
							break;
						}
						}
					}
					break;
				}
				}
			}
		}
	}

	void EditorLayer::UI_HandleAssetDrop()
	{
		if (!ImGui::BeginDragDropTarget())
			return;

		auto data = ImGui::AcceptDragDropPayload("asset_payload");
		if (data)
		{
			uint64_t count = data->DataSize / sizeof(AssetHandle);

			for (uint64_t i = 0; i < count; i++)
			{
				AssetHandle assetHandle = *(((AssetHandle*)data->Data) + i);
				const AssetMetadata& assetData = Project::GetEditorAssetManager()->GetMetadata(assetHandle);

				// We can't really support dragging and dropping scenes when we're dropping multiple assets
				if (count == 1 && assetData.Type == AssetType::Scene)
				{
					OpenScene(assetData);
					break;
				}

				Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
				if (asset)
				{
					if (asset->GetAssetType() == AssetType::MeshSource)
					{
						OnCreateMeshFromMeshSource({}, asset.As<MeshSource>());
					}
					else if (asset->GetAssetType() == AssetType::Mesh)
					{
						Ref<Mesh> mesh = asset.As<Mesh>();
						const auto& submeshIndices = mesh->GetSubmeshes();
						const auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
						Entity rootEntity = m_EditorScene->InstantiateMesh(mesh, true);
						SelectionManager::Select(SelectionContext::Scene, rootEntity.GetUUID());
					}
					else if (asset->GetAssetType() == AssetType::StaticMesh)
					{
						Ref<StaticMesh> staticMesh = asset.As<StaticMesh>();
						auto& assetData = Project::GetEditorAssetManager()->GetMetadata(staticMesh->Handle);
						Entity entity = m_EditorScene->CreateEntity(assetData.FilePath.stem().string());
						entity.AddComponent<StaticMeshComponent>(staticMesh->Handle);
						SelectionManager::Select(SelectionContext::Scene, entity.GetUUID());
					}
					else if (asset->GetAssetType() == AssetType::Prefab)
					{
						Ref<Prefab> prefab = asset.As<Prefab>();
						m_EditorScene->Instantiate(prefab);
					}
				}
				else
				{
					m_InvalidAssetMetadataPopupData.Metadata = assetData;
					UI_ShowInvalidAssetMetadataPopup();
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	void EditorLayer::UI_ShowNewProjectPopup()
	{
		UI::ShowMessageBox("New Project", [this]()
			{
				UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(10, 7));

				UI::Fonts::PushFont("Bold");
				std::string fullProjectPath = strlen(s_NewProjectFilePathBuffer) > 0 ? std::string(s_NewProjectFilePathBuffer) + "/" + std::string(s_ProjectNameBuffer) : "";
				ImGui::Text("Full Project Path: %s", fullProjectPath.c_str());
				UI::Fonts::PopFont();

				ImGui::SetNextItemWidth(-1);
				ImGui::InputTextWithHint("##new_project_name", "Project Name", s_ProjectNameBuffer, MAX_PROJECT_NAME_LENGTH);

				ImVec2 label_size = ImGui::CalcTextSize("...", NULL, true);
				auto& style = ImGui::GetStyle();
				ImVec2 button_size = ImGui::CalcItemSize(ImVec2(0, 0), label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

				ImGui::SetNextItemWidth(590 - button_size.x - style.FramePadding.x * 2.0f - style.ItemInnerSpacing.x - 1);
				ImGui::InputTextWithHint("##new_project_location", "Project Location", s_NewProjectFilePathBuffer, MAX_PROJECT_FILEPATH_LENGTH);

				ImGui::SameLine();

				if (ImGui::Button("..."))
				{
					std::string result = FileSystem::OpenFolderDialog().string();
					memcpy(s_NewProjectFilePathBuffer, result.data(), result.length());
				}

				ImGui::Separator();

				UI::Fonts::PushFont("Bold");
				if (ImGui::Button("Create"))
				{
					CreateProject(fullProjectPath);
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("Cancel"))
				{
					memset(s_NewProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);
					ImGui::CloseCurrentPopup();
				}
				UI::Fonts::PopFont();
			}, 600);
	}

	void EditorLayer::UI_ShowLoadAutoSavePopup()
	{
		UI::ShowMessageBox("LoadAutoSave", [this]()
			{
				ImGui::Text("The scene has a newer auto-save file.");
				ImGui::Text("Do you want to load the auto-saved scene instead?");

				if (ImGui::Button("Yes"))
				{
					OpenScene(m_LoadAutoSavePopupData.FilePathAuto, false);
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();
				if (ImGui::Button("No"))
				{
					OpenScene(m_LoadAutoSavePopupData.FilePath, false);
					ImGui::CloseCurrentPopup();
				}

			}, 400);
	}

	float EditorLayer::GetSnapValue()
	{
		switch (m_GizmoType)
		{
		case ImGuizmo::OPERATION::TRANSLATE: return 0.5f;
		case ImGuizmo::OPERATION::ROTATE: return 45.0f;
		case ImGuizmo::OPERATION::SCALE: return 0.5f;
		}
		return 0.0f;
	}

	void EditorLayer::DeleteEntity(Entity entity)
	{
		if (!entity)
			return;

		m_EditorScene->DestroyEntity(entity);
	}

	void EditorLayer::UpdateSceneRendererSettings()
	{
		std::array<Ref<SceneRenderer>, 2> renderers = { m_ViewportRenderer, m_SecondViewportRenderer };

		for (Ref<SceneRenderer> renderer : renderers)
		{
			/*SceneRendererOptions& options = renderer->GetOptions();
			options.ShowSelectedInWireframe = m_ShowSelectedWireframe;

			SceneRendererOptions::PhysicsColliderView colliderView = SceneRendererOptions::PhysicsColliderView::None;
			if (m_ShowPhysicsCollidersWireframe)
				colliderView = m_ShowPhysicsCollidersWireframeOnTop ? SceneRendererOptions::PhysicsColliderView::OnTop : SceneRendererOptions::PhysicsColliderView::Normal;
			options.ShowPhysicsColliders = colliderView;*/
		}
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		X2_PROFILE_FUNC();

		switch (m_SceneState)
		{
		case SceneState::Edit:
		{
			m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
			m_EditorCamera.OnUpdate(ts);
			m_EditorScene->OnUpdateEditor(ts);
			m_EditorScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);

			if (m_ShowSecondViewport)
			{
				m_SecondEditorCamera.SetActive(m_ViewportPanel2Focused);
				m_SecondEditorCamera.OnUpdate(ts);
				m_EditorScene->OnRenderEditor(m_SecondViewportRenderer, ts, m_SecondEditorCamera);
			}

			OnRender2D();

			if (const auto& project = Project::GetActive(); project && project->GetConfig().EnableAutoSave)
			{
				m_TimeSinceLastSave += ts;
				if (m_TimeSinceLastSave > project->GetConfig().AutoSaveIntervalSeconds)
				{
					SaveSceneAuto();
				}
			}

			break;
		}
		case SceneState::Play:
		{
			m_RuntimeScene->OnUpdateRuntime(ts);

			if (m_EditorCameraInRuntime)
			{
				m_EditorCamera.SetActive(m_ViewportPanelMouseOver || m_AllowViewportCameraEvents);
				m_EditorCamera.OnUpdate(ts);
				m_RuntimeScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);
				OnRender2D();
			}
			else
			{
				m_RuntimeScene->OnRenderRuntime(m_ViewportRenderer, ts);
			}

			for (auto& fn : m_PostSceneUpdateQueue)
				fn();
			m_PostSceneUpdateQueue.clear();
			break;
		}
		case SceneState::Simulate:
		{
			m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
			m_EditorCamera.OnUpdate(ts);
			m_SimulationScene->OnUpdateRuntime(ts);
			//m_SimulationScene->OnRenderSimulation(m_ViewportRenderer, ts, m_EditorCamera);
			break;
		}
		case SceneState::Pause:
		{
			m_EditorCamera.SetActive(m_ViewportPanelMouseOver);
			m_EditorCamera.OnUpdate(ts);

			m_RuntimeScene->OnRenderRuntime(m_ViewportRenderer, ts);
			break;
		}
		}

		if (((Input::IsKeyDown(X2::KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || (Input::IsMouseButtonDown(MouseButton::Middle)))) || Input::IsMouseButtonDown(MouseButton::Right)) && !m_StartedCameraClickInViewport && m_ViewportPanelFocused && m_ViewportPanelMouseOver)
			m_StartedCameraClickInViewport = true;

		if (!Input::IsMouseButtonDown(MouseButton::Right) && !(Input::IsKeyDown(X2::KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || (Input::IsMouseButtonDown(MouseButton::Middle)))))
			m_StartedCameraClickInViewport = false;

		AssetEditorPanel::OnUpdate(ts);

		SceneRenderer::WaitForThreads();

		/*if (ScriptEngine::ShouldReloadAppAssembly())
			ScriptEngine::ReloadAppAssembly();*/
	}

	void EditorLayer::OnEntityDeleted(Entity e)
	{
		SelectionManager::Deselect(SelectionContext::Scene, e.GetUUID());
	}

	void EditorLayer::OnRender2D()
	{
		if (!m_ViewportRenderer->GetFinalPassImage())
			return;

		m_Renderer2D->BeginScene(m_EditorCamera.GetViewProjection(), m_EditorCamera.GetViewMatrix());
		m_Renderer2D->SetTargetRenderPass(m_ViewportRenderer->GetExternalCompositeRenderPass());

		if (m_DrawOnTopBoundingBoxes && m_ShowBoundingBoxes)
		{
			if (m_ShowBoundingBoxSelectedMeshOnly)
			{
				const auto& selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
				for (const auto& entityID : selectedEntities)
				{
					Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);

					if (entity.HasComponent<MeshComponent>())
					{
						auto mesh = AssetManager::GetAsset<Mesh>(entity.GetComponent<MeshComponent>().Mesh);

						if (mesh)
						{
							if (m_ShowBoundingBoxSubmeshes)
							{
								const auto& submeshIndices = mesh->GetSubmeshes();
								const auto& meshAsset = mesh->GetMeshSource();
								const auto& submeshes = meshAsset->GetSubmeshes();

								for (uint32_t submeshIndex : submeshIndices)
								{
									glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
									const AABB& aabb = submeshes[submeshIndex].BoundingBox;
									m_Renderer2D->DrawAABB(aabb, transform * submeshes[submeshIndex].Transform, glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
								}
							}
							else
							{
								glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
								const AABB& aabb = mesh->GetMeshSource()->GetBoundingBox();
								m_Renderer2D->DrawAABB(aabb, transform, glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
							}
						}
					}
					else if (entity.HasComponent<StaticMeshComponent>())
					{
						auto mesh = AssetManager::GetAsset<StaticMesh>(entity.GetComponent<StaticMeshComponent>().StaticMesh);

						if (mesh)
						{
							if (m_ShowBoundingBoxSubmeshes)
							{
								const auto& submeshIndices = mesh->GetSubmeshes();
								const auto& meshAsset = mesh->GetMeshSource();
								const auto& submeshes = meshAsset->GetSubmeshes();

								for (uint32_t submeshIndex : submeshIndices)
								{
									glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
									const AABB& aabb = submeshes[submeshIndex].BoundingBox;
									m_Renderer2D->DrawAABB(aabb, transform * submeshes[submeshIndex].Transform, glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
								}
							}
							else
							{
								glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
								const AABB& aabb = mesh->GetMeshSource()->GetBoundingBox();
								m_Renderer2D->DrawAABB(aabb, transform, glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
							}
						}
					}
				}
			}
			else
			{
				auto dynamicMeshEntities = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
				for (auto e : dynamicMeshEntities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
					auto mesh = AssetManager::GetAsset<Mesh>(entity.GetComponent<MeshComponent>().Mesh);
					if (mesh)
					{
						const AABB& aabb = mesh->GetMeshSource()->GetBoundingBox();
						m_Renderer2D->DrawAABB(aabb, transform, glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
					}
				}

				auto staticMeshEntities = m_CurrentScene->GetAllEntitiesWith<StaticMeshComponent>();
				for (auto e : staticMeshEntities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
					auto mesh = AssetManager::GetAsset<StaticMesh>(entity.GetComponent<StaticMeshComponent>().StaticMesh);
					if (mesh)
					{
						const AABB& aabb = mesh->GetMeshSource()->GetBoundingBox();
						m_Renderer2D->DrawAABB(aabb, transform, glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
					}
				}
			}
		}

		if (m_ShowIcons)
		{
			{
				auto entities = m_CurrentScene->GetAllEntitiesWith<PointLightComponent>();
				for (auto e : entities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					m_Renderer2D->DrawQuadBillboard(m_CurrentScene->GetWorldSpaceTransform(entity).Translation, { 1.0f, 1.0f }, EditorResources::PointLightIcon);
				}
			}

			{
				auto entities = m_CurrentScene->GetAllEntitiesWith<SpotLightComponent>();
				for (auto e : entities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					m_Renderer2D->DrawQuadBillboard(m_CurrentScene->GetWorldSpaceTransform(entity).Translation, { 1.0f, 1.0f }, EditorResources::SpotLightIcon);
				}
			}

			{
				auto entities = m_CurrentScene->GetAllEntitiesWith<CameraComponent>();
				for (auto e : entities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					m_Renderer2D->DrawQuadBillboard(m_CurrentScene->GetWorldSpaceTransform(entity).Translation, { 1.0f, 1.0f }, EditorResources::CameraIcon);
				}
			}

			/*{
				auto entities = m_CurrentScene->GetAllEntitiesWith<AudioComponent>();
				for (auto e : entities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					m_Renderer2D->DrawQuadBillboard(m_CurrentScene->GetWorldSpaceTransform(entity).Translation, { 1.0f, 1.0f }, EditorResources::AudioIcon);
				}
			}*/
		}

		const auto& selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
		for (const auto& entityID : selectedEntities)
		{
			Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);

			if (!entity.HasComponent<PointLightComponent>())
				continue;

			const auto& plc = entity.GetComponent<PointLightComponent>();
			glm::vec3 translation = m_CurrentScene->GetWorldSpaceTransform(entity).Translation;
			m_Renderer2D->DrawCircle(translation, { 0.0f, 0.0f, 0.0f }, plc.Radius, glm::vec4(plc.Radiance, 1.0f));
			m_Renderer2D->DrawCircle(translation, { glm::radians(90.0f), 0.0f, 0.0f }, plc.Radius, glm::vec4(plc.Radiance, 1.0f));
			m_Renderer2D->DrawCircle(translation, { 0.0f, glm::radians(90.0f), 0.0f }, plc.Radius, glm::vec4(plc.Radiance, 1.0f));
		}

		m_Renderer2D->EndScene();
	}

	static void ReplaceToken(std::string& str, const char* token, const std::string& value)
	{
		size_t pos = 0;
		while ((pos = str.find(token, pos)) != std::string::npos)
		{
			str.replace(pos, strlen(token), value);
			pos += strlen(token);
		}
	}

	void EditorLayer::CreateProject(std::filesystem::path projectPath)
	{
		if (!std::filesystem::exists(projectPath))
			std::filesystem::create_directories(projectPath);

		std::filesystem::copy("Resources/NewProjectTemplate", projectPath, std::filesystem::copy_options::recursive);
		std::filesystem::path RootDirectory = std::filesystem::absolute("./Resources").parent_path().string();
		std::string RootDirectoryString = RootDirectory.string();

		if (RootDirectory.stem().string() == "X2")
			RootDirectoryString = RootDirectory.parent_path().string();

		std::replace(RootDirectoryString.begin(), RootDirectoryString.end(), '\\', '/');

		{
			// premake5.lua
			std::ifstream stream(projectPath / "premake5.lua");
			X2_CORE_VERIFY(stream.is_open());
			std::stringstream ss;
			ss << stream.rdbuf();
			stream.close();

			std::string str = ss.str();
			ReplaceToken(str, "$PROJECT_NAME$", s_ProjectNameBuffer);

			std::ofstream ostream(projectPath / "premake5.lua");
			ostream << str;
			ostream.close();
		}

		{
			// Project File
			std::ifstream stream(projectPath / "Project.xproj");
			X2_CORE_VERIFY(stream.is_open());
			std::stringstream ss;
			ss << stream.rdbuf();
			stream.close();

			std::string str = ss.str();
			ReplaceToken(str, "$PROJECT_NAME$", s_ProjectNameBuffer);

			std::ofstream ostream(projectPath / "Project.xproj");
			ostream << str;
			ostream.close();

			std::string newProjectFileName = std::string(s_ProjectNameBuffer) + ".xproj";
			std::filesystem::rename(projectPath / "Project.xproj", projectPath / newProjectFileName);
		}

		std::filesystem::create_directories(projectPath / "Assets" / "Audio");
		std::filesystem::create_directories(projectPath / "Assets" / "Materials");
		std::filesystem::create_directories(projectPath / "Assets" / "Meshes" / "Default");
		std::filesystem::create_directories(projectPath / "Assets" / "Meshes" / "Default" / "Source");
		std::filesystem::create_directories(projectPath / "Assets" / "Scenes");
		std::filesystem::create_directories(projectPath / "Assets" / "Scripts" / "Source");
		std::filesystem::create_directories(projectPath / "Assets" / "Textures");

		//NOTE (Tim) : Copying meshes from resources, change this in the future to just a vertex buffer thats built into 
		//			   the engine since we wouldn't really want to modify these meshes and hence there's no point to have gltf files...
		{
			std::filesystem::path originalFilePath = RootDirectoryString + "/X2/Resources/Meshes/Default";
			std::filesystem::path targetPath = projectPath / "Assets" / "Meshes" / "Default" / "Source";
			X2_CORE_VERIFY(std::filesystem::exists(originalFilePath));

			for (const auto& dirEntry : std::filesystem::directory_iterator(originalFilePath))
				std::filesystem::copy(dirEntry.path(), targetPath);
		}

		{
			RecentProject projectEntry;
			projectEntry.Name = s_ProjectNameBuffer;
			projectEntry.FilePath = (projectPath / (std::string(s_ProjectNameBuffer) + ".xproj")).string();
			projectEntry.LastOpened = time(NULL);
			m_UserPreferences->RecentProjects[projectEntry.LastOpened] = projectEntry;

			UserPreferencesSerializer preferencesSerializer(m_UserPreferences);
			preferencesSerializer.Serialize(m_UserPreferences->FilePath);
		}

		//RegenerateProjectScriptSolution(projectPath);
		OpenProject(projectPath.string() + "/" + std::string(s_ProjectNameBuffer) + ".xproj");
	}

	void EditorLayer::EmptyProject()
	{
		if (Project::GetActive())
			CloseProject();

		Ref<Project> project = Ref<Project>::Create();
		Project::SetActive(project);

		m_PanelManager->OnProjectChanged(project);
		NewScene();

		SelectionManager::DeselectAll();

		FileSystem::StartWatching();

		// Reset cameras
		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f);
		m_SecondEditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f);

		memset(s_ProjectNameBuffer, 0, MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);
	}

	void EditorLayer::UpdateCurrentProject()
	{
		//std::filesystem::copy("Resources/NewProjectTemplate/premake5.lua", Project::GetProjectDirectory() / "premake5.lua", std::filesystem::copy_options::overwrite_existing);
		//std::filesystem::copy("Resources/NewProjectTemplate/Win-CreateScriptProjects.bat", Project::GetProjectDirectory() / "Win-CreateScriptProjects.bat", std::filesystem::copy_options::overwrite_existing);

		//{
		//	// premake5.lua
		//	std::ifstream stream(Project::GetProjectDirectory() / "premake5.lua");
		//	X2_CORE_VERIFY(stream.is_open());
		//	std::stringstream ss;
		//	ss << stream.rdbuf();
		//	stream.close();

		//	std::string str = ss.str();
		//	ReplaceToken(str, "$PROJECT_NAME$", Project::GetProjectName());

		//	std::ofstream ostream(Project::GetProjectDirectory() / "premake5.lua");
		//	ostream << str;
		//	ostream.close();
		//}

		//RegenerateProjectScriptSolution(Project::GetProjectDirectory());
	}

	void EditorLayer::OpenProject()
	{
		std::filesystem::path filepath = FileSystem::OpenFileDialog("X2 Project (*.xproj)\0*.xproj\0");

		if (filepath.empty())
			return;

		// stash the filepath away.  Actual opening of project is deferred until it is "safe" to do so.
		strcpy(s_OpenProjectFilePathBuffer, filepath.string().data());

		RecentProject projectEntry;
		projectEntry.Name = Utils::RemoveExtension(filepath.filename().string());
		projectEntry.FilePath = filepath.string();
		projectEntry.LastOpened = time(NULL);

		for (auto it = m_UserPreferences->RecentProjects.begin(); it != m_UserPreferences->RecentProjects.end(); it++)
		{
			if (it->second.Name == projectEntry.Name)
			{
				m_UserPreferences->RecentProjects.erase(it);
				break;
			}
		}

		m_UserPreferences->RecentProjects[projectEntry.LastOpened] = projectEntry;

		UserPreferencesSerializer serializer(m_UserPreferences);
		serializer.Serialize(m_UserPreferences->FilePath);
	}

	void EditorLayer::OpenProject(const std::filesystem::path& filepath)
	{
		if (!FileSystem::Exists(filepath))
		{
			X2_CORE_ERROR("Tried to open a project that doesn't exist. Project path: {0}", filepath);
			memset(s_OpenProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);
			return;
		}

		if (Project::GetActive())
			CloseProject();

		Ref<Project> project = Ref<Project>::Create();
		ProjectSerializer serializer(project);
		serializer.Deserialize(filepath);
		Project::SetActive(project);

		m_PanelManager->OnProjectChanged(project);

		bool hasScene = !project->GetConfig().StartScene.empty();
		if (hasScene)
			hasScene = OpenScene((Project::GetAssetDirectory() / project->GetConfig().StartScene).string(), true);

		if (!hasScene)
			NewScene();

		SelectionManager::DeselectAll();

		FileSystem::StartWatching();

		// Reset cameras
		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f);
		m_SecondEditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f);

		memset(s_ProjectNameBuffer, 0, MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, MAX_PROJECT_FILEPATH_LENGTH);

		s_ProjectSolutionPath = Project::GetProjectDirectory() / std::filesystem::path(Project::GetProjectName() + ".sln");

		if (false && m_ProjectUpdateNeeded)
		{
			UpdateCurrentProject();
			m_ShowProjectUpdatedPopup = true;
		}
	}

	void EditorLayer::SaveProject()
	{
		if (!Project::GetActive())
			X2_CORE_ASSERT(false); // TODO

		auto& project = Project::GetActive();
		ProjectSerializer serializer(project);
		serializer.Serialize(project->GetConfig().ProjectDirectory + "/" + project->GetConfig().ProjectFileName);

		m_PanelManager->Serialize();
		//AudioEventsEditor::Serialize();
	}

	void EditorLayer::CloseProject(bool unloadProject)
	{
		FileSystem::StopWatching();

		SaveProject();

		// There are several things holding references to the scene.
		// These all need to be set back to nullptr so that you end up with a zero reference count and the scene is destroyed.
		// If you do not do this, then after opening the new project (and possibly loading script assemblies that are un-related to the old scene)
		// you still have the old scene kicking around => Not ideal.
		// Of course you could just wait until all of these things (eventually) get pointed to the new scene, and then when the old scene is
		// finally decref'd to zero it will be destroyed.  However, that will be after the project has been closed, and the new project has
		// been opened, which is out of order.  Seems safter to make sure the old scene is cleaned up before closing project.
		m_PanelManager->SetSceneContext(nullptr);
		//ScriptEngine::UnloadAppAssembly();
		//ScriptEngine::SetSceneContext(nullptr, nullptr);
		//MiniAudioEngine::SetSceneContext(nullptr);
		AssetEditorPanel::SetSceneContext(nullptr);
		m_ViewportRenderer->SetScene(nullptr);
		m_SecondViewportRenderer->SetScene(nullptr);
		m_RuntimeScene = nullptr;
		m_CurrentScene = nullptr;

		// Check that m_EditorScene is the last one (so setting it null here will destroy the scene)
		X2_CORE_ASSERT(m_EditorScene->GetRefCount() == 1, "Scene will not be destroyed after project is closed - something is still holding scene refs!");
		m_EditorScene = nullptr;

		//PhysicsLayerManager::ClearLayers();

		if (unloadProject)
			Project::SetActive(nullptr);
	}

	void EditorLayer::NewScene(const std::string& name)
	{
		SelectionManager::DeselectAll();

		//ScriptEngine::SetSceneContext(nullptr, nullptr);
		m_EditorScene = Ref<Scene>::Create(name, true);
		m_PanelManager->SetSceneContext(m_EditorScene);
		//ScriptEngine::SetSceneContext(m_EditorScene, m_ViewportRenderer);
		//MiniAudioEngine::SetSceneContext(m_EditorScene);
		AssetEditorPanel::SetSceneContext(m_EditorScene);
		UpdateWindowTitle(name);
		m_SceneFilePath = std::string();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 100.f);
		m_CurrentScene = m_EditorScene;

		if (m_ViewportRenderer)
			m_ViewportRenderer->SetScene(m_CurrentScene);
		if (m_SecondViewportRenderer)
			m_SecondViewportRenderer->SetScene(m_CurrentScene);
	}

	bool EditorLayer::OpenScene()
	{
		std::filesystem::path filepath = FileSystem::OpenFileDialog(SceneSerializer::FileFilter.data());
		if (!filepath.empty())
			return OpenScene(filepath, true);

		return false;
	}

	bool EditorLayer::OpenScene(const std::filesystem::path& filepath, const bool checkAutoSave)
	{
		if (!FileSystem::Exists(filepath))
		{
			X2_CORE_ERROR("Tried loading a non-existing scene: {0}", filepath);
			return false;
		}

		if (checkAutoSave)
		{
			auto filepathAuto = filepath;
			filepathAuto += ".auto";
			if (FileSystem::Exists(filepathAuto) && FileSystem::IsNewer(filepathAuto, filepath))
			{
				m_LoadAutoSavePopupData.FilePath = filepath.string();
				m_LoadAutoSavePopupData.FilePathAuto = filepathAuto.string();
				UI_ShowLoadAutoSavePopup();
				return false;
			}
		}

		if (m_SceneState == SceneState::Play)
			OnSceneStop();
		else if (m_SceneState == SceneState::Simulate)
			OnSceneStopSimulation();

		// NOTE(Peter): We set the scene context to nullptr here to make sure all old script entities have been destroyed
		//ScriptEngine::SetSceneContext(nullptr, nullptr);

		Ref<Scene> newScene = Ref<Scene>::Create("New Scene", true);
		SceneSerializer serializer(newScene);
		serializer.Deserialize(filepath);
		m_EditorScene = newScene;
		m_SceneFilePath = filepath.string();
		std::replace(m_SceneFilePath.begin(), m_SceneFilePath.end(), '\\', '/');
		if ((m_SceneFilePath.size() >= 5) && (m_SceneFilePath.substr(m_SceneFilePath.size() - 5) == ".auto"))
			m_SceneFilePath = m_SceneFilePath.substr(0, m_SceneFilePath.size() - 5);

		UpdateWindowTitle(filepath.filename().string());
		m_PanelManager->SetSceneContext(m_EditorScene);
		//ScriptEngine::SetSceneContext(m_EditorScene, m_ViewportRenderer);
		//MiniAudioEngine::SetSceneContext(m_EditorScene);
		AssetEditorPanel::SetSceneContext(m_EditorScene);

		SelectionManager::DeselectAll();

		m_CurrentScene = m_EditorScene;

		if (m_ViewportRenderer)
			m_ViewportRenderer->SetScene(m_CurrentScene);
		if (m_SecondViewportRenderer)
			m_SecondViewportRenderer->SetScene(m_CurrentScene);

		return true;
	}

	bool EditorLayer::OpenScene(const AssetMetadata& assetMetadata)
	{
		std::filesystem::path workingDirPath = Project::GetAssetDirectory() / assetMetadata.FilePath;
		return OpenScene(workingDirPath.string(), true);
	}

	void EditorLayer::SaveScene()
	{
		if (!m_SceneFilePath.empty())
		{
			SceneSerializer serializer(m_EditorScene);
			serializer.Serialize(m_SceneFilePath);

			m_TimeSinceLastSave = 0.0f;
		}
		else
		{
			SaveSceneAs();
		}
	}

	// Auto save the scene.
	// We save to different files so as not to overwrite the manually saved scene.
	// (the manually saved scene is the authoritative source, auto saved scene is just a backup)
	// If scene has never been saved, then do nothing (there is no nice option for where to save the scene to in this situation)
	void EditorLayer::SaveSceneAuto()
	{
		if (!m_SceneFilePath.empty())
		{
			SceneSerializer serializer(m_EditorScene);
			serializer.Serialize(m_SceneFilePath + ".auto"); // this isn't perfect as there is a non-zero chance (admittedly small) of overwriting some existing .auto file that the user actually wanted)

			// Need to save the audio command registry also, as otherwise there's a chance .auto saved scene refers to non-existent audio asset ids
			// Note that the main asset registry is saved whenever it is changed, so there should be no need to periodically auto save it.
			// TODO: JP. ACR shouldn't be coupled with the scene, it's scope is Project.
			
			/*if (auto& activeACR = AudioCommandRegistry::GetActive())
			{
				activeACR->WriteRegistryToFile(Project::GetAudioCommandsRegistryPath().string() + ".auto");
				AudioEventsEditor::Serialize();
			}*/

			m_TimeSinceLastSave = 0.0f;
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::lock_guard guard(ContentBrowserPanel::s_LockMutex);

		std::filesystem::path filepath = FileSystem::SaveFileDialog(SceneSerializer::FileFilter.data());

		if (filepath.empty())
			return;

		if (!filepath.has_extension())
			filepath += SceneSerializer::DefaultExtension;

		SceneSerializer serializer(m_EditorScene);
		serializer.Serialize(filepath.string());

		std::filesystem::path path = filepath;
		UpdateWindowTitle(path.filename().string());
		m_SceneFilePath = filepath.string();
		std::replace(m_SceneFilePath.begin(), m_SceneFilePath.end(), '\\', '/');
	}

	void EditorLayer::UI_ShowWelcomePopup()
	{
		UI::ShowMessageBox("Welcome", [this]()
			{
				ImGui::Text("Welcome to X2!");
				ImGui::Separator();
				ImGui::TextWrapped("X2 is an early-stage interactive application and rendering engine for Windows.");
				ImGui::TextWrapped("Please report bugs by opening an issue on the X2-dev GitLab.");

				if (ImGui::Button("OK"))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine(250.0f);
				ImGui::TextUnformatted("Don't Show Again");
				ImGui::SameLine(365.0f);
				bool dontShowAgain = !m_UserPreferences->ShowWelcomeScreen;
				if (ImGui::Checkbox("##dont_show_again", &dontShowAgain))
				{
					m_UserPreferences->ShowWelcomeScreen = !dontShowAgain;
					UserPreferencesSerializer serializer(m_UserPreferences);
					serializer.Serialize(m_UserPreferences->FilePath);
				}
			}, 400);
	}

	void EditorLayer::UI_ShowAboutPopup()
	{
		UI::ShowMessageBox("About X2", []()
			{
				UI::Fonts::PushFont("Large");
				ImGui::Text("X2 Engine");
				UI::Fonts::PopFont();

				ImGui::Separator();
				ImGui::TextWrapped("X2 is an early-stage interactive application and rendering engine for Windows.");
				ImGui::Separator();

				UI::Fonts::PushFont("Bold");
				ImGui::Text("X2 Core Team");
				UI::Fonts::PopFont();
				ImGui::Text("Yan Chernikov");
				ImGui::Text("Peter Nilsson");
				ImGui::Text("Tim Kireenko");
				ImGui::Text("Jim Waite");
				ImGui::Text("Jaroslav Pevno");
				ImGui::Text("Karim Sayed");
				ImGui::Separator();


				ImGui::Separator();

				UI::Fonts::PushFont("Bold");
				ImGui::TextUnformatted("Third-Party Info");
				UI::Fonts::PopFont();

				ImGui::Text("ImGui Version: %s (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
				ImGui::TextColored(ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f }, "This software contains source code provided by NVIDIA Corporation.");

				ImGui::Separator();
				/*const auto coreAssemblyInfo = ScriptEngine::GetCoreAssemblyInfo();
				ImGui::Text("Script Library Version: %d.%d.%d.%d", coreAssemblyInfo->Metadata.MajorVersion, coreAssemblyInfo->Metadata.MinorVersion, coreAssemblyInfo->Metadata.BuildVersion, coreAssemblyInfo->Metadata.RevisionVersion);*/

				if (ImGui::Button("OK"))
					ImGui::CloseCurrentPopup();
			});
	}

	void EditorLayer::UI_BuildAssetPackDialog()
	{
		if (m_AssetPackFuture.valid())
		{
			using namespace std::chrono_literals;
			auto status = m_AssetPackFuture.wait_for(0s);
			if (status == std::future_status::ready)
			{
				m_AssetPackBuildInProgress = false;
				if (m_AssetPackThread.joinable())
					m_AssetPackThread.join();

				m_AssetPackFuture = {};
				X2_CONSOLE_LOG_INFO("Asset Pack build done!");
			}
		}

		if (m_OpenAssetPackDialog)
		{
			ImGui::OpenPopup("Building Asset Pack");
			m_OpenAssetPackDialog = false;
		}

		if (!m_OpenAssetPackDialog && !ImGui::IsPopupOpen("Building Asset Pack") && m_AssetPackBuildProgress < 1.0f && m_AssetPackBuildProgress > 0.0f)
			ImGui::OpenPopup("Building Asset Pack");

		if (ImGui::BeginPopupModal("Building Asset Pack", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			float progress = m_AssetPackBuildProgress;
			ImGui::ProgressBar(progress, ImVec2(0, 0));

			if (progress >= 1.0f)
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void EditorLayer::UI_ShowCreateNewMeshPopup()
	{
		static std::array<int, 2> gridColumn0Widths = { 0, 0 };

		const std::string meshAssetPathLabel = fmt::format("mesh: {0}/", Project::GetActive()->GetConfig().MeshPath);
		const std::string skeletonAssetPathLabel = fmt::format("skeleton: {0}/", Project::GetActive()->GetConfig().AnimationPath);
		const std::string animationAssetPathLabel = fmt::format("animations: {0}/", Project::GetActive()->GetConfig().AnimationPath);
		int maxAssetPathWidth = std::max({ 160.0f, ImGui::CalcTextSize(meshAssetPathLabel.c_str()).x, ImGui::CalcTextSize(skeletonAssetPathLabel.c_str()).x, ImGui::CalcTextSize(animationAssetPathLabel.c_str()).x }) + 30.0f;

		gridColumn0Widths = { maxAssetPathWidth, maxAssetPathWidth };

		UI::ShowMessageBox("Create New Mesh", [this, meshAssetPathLabel, skeletonAssetPathLabel, animationAssetPathLabel]()
			{


				ImGui::TextWrapped("A Mesh asset must be created from this Mesh Source file (eg. FBX) before it can be added to your scene. More options can be accessed by double-clicking a Mesh Source file in the Content Browser panel.");
				ImGui::AlignTextToFramePadding();

				// Import settings
				enum MeshType
				{
					Static,
					Dynamic
				};
				static int meshType;
				static bool doImportMesh = true;
				static bool doImportSkeleton = false;
				static bool doImportAnimations = false;
				static bool doCreateAnimationController = false;
				static bool doGenerateColliders = true;

				ImGui::Text("Import Settings:");
				UI::Draw::Underline();
				int grid = 0;
				{
					UI::ScopedID gridId(grid);
					UI::BeginPropertyGrid();
					if (gridColumn0Widths[grid])
					{
						ImGui::SetColumnWidth(0, gridColumn0Widths[grid]);
						gridColumn0Widths[grid] = 0;
					}

					if (UI::PropertyRadio(
						"Import as",
						meshType,
						{
							{MeshType::Static, "Static Mesh"},
							{MeshType::Dynamic, "Dynamic Mesh"}
						},
						"",
						{
							{MeshType::Static, "Everything in the source asset will be imported into a single mesh.  Static is good for things like scene background entities where you do not need be able to manipulate parts of the source asset independently."},
							{MeshType::Dynamic, "The structure of the source asset will be preserved.  Adding a dynamic mesh to a scene will result in multiple entities with hierarchy mirroring the structure of the source asset.  Dynamic is needed for rigged or animated entities, or where you need to be able to manipulate individual parts of the source asset independently."}
						}
					))
					{
						m_CreateNewMeshPopupData.CreateMeshFilenameBuffer[0] = 0; // Force re-creation of filepath when user changes static/dynamic choice
						if (meshType == MeshType::Static)
						{
							doImportMesh = true;
							doImportSkeleton = false;
							doImportAnimations = false;
							doCreateAnimationController = false;
						}
					}

					{
						UI::ScopedDisable disable(meshType == MeshType::Static);
						UI::Property("Import Mesh", doImportMesh, "Controls whether mesh(es) will be imported from source asset.");
						UI::Property("Import Skeleton", doImportSkeleton, "Controls whether skeletal animation rig (if present) will be imported from source asset.");
						UI::Property("Import Animations", doImportAnimations, "Controls whether animations (if present) will be imported from source asset.");
						{
							UI::ScopedDisable disable2(!doImportSkeleton);
							doCreateAnimationController = doCreateAnimationController && doImportSkeleton;
							UI::Property("Create Animation Controller", doCreateAnimationController, "Controls whether an animation controller will be created, and pre-populated with the imported asset(s).  The animation controller requires at least a skeleton asset.");
						}
					}

					UI::Property("Generate Colliders", doGenerateColliders, "Controls whether physics components (collider and rigid body) will be added to the newly created entity.");

					UI::EndPropertyGrid();
					grid++;
				}

				const AssetMetadata& assetData = Project::GetEditorAssetManager()->GetMetadata(m_CreateNewMeshPopupData.MeshToCreate->Handle);
				std::string filepathMesh = fmt::format("{0}/{1}.{2}", m_CurrentScene->GetName(), assetData.FilePath.stem().string(), meshType == MeshType::Static ? "xsmesh" : "xmesh");
				std::string filepathSkeleton = fmt::format("{0}/{1}.{2}", m_CurrentScene->GetName(), assetData.FilePath.stem().string(), "xskel");
				std::string filepathAnimation = fmt::format("{0}/{1}.{2}", m_CurrentScene->GetName(), assetData.FilePath.stem().string(), "xanim");

				if (!m_CreateNewMeshPopupData.CreateMeshFilenameBuffer[0])
				{
					strcpy(m_CreateNewMeshPopupData.CreateMeshFilenameBuffer.data(), filepathMesh.c_str());
					strcpy(m_CreateNewMeshPopupData.CreateSkeletonFilenameBuffer.data(), filepathSkeleton.c_str());
					strcpy(m_CreateNewMeshPopupData.CreateAnimationFilenameBuffer.data(), filepathAnimation.c_str());
				}

				ImGui::Text("Asset Paths:");
				UI::Draw::Underline();
				{
					UI::ScopedID gridId(grid);
					UI::BeginPropertyGrid();
					if (gridColumn0Widths[grid])
					{
						ImGui::SetColumnWidth(0, gridColumn0Widths[grid]);
						gridColumn0Widths[grid] = 0;
					}
					{
						UI::ScopedDisable disable(!doImportMesh);
						UI::Property(meshAssetPathLabel.c_str(), m_CreateNewMeshPopupData.CreateMeshFilenameBuffer.data(), 256);
					}
					{
						UI::ScopedDisable disable(!doImportSkeleton);
						UI::Property(skeletonAssetPathLabel.c_str(), m_CreateNewMeshPopupData.CreateSkeletonFilenameBuffer.data(), 256);
					}
					{
						UI::ScopedDisable disable(!doImportAnimations);
						UI::Property(animationAssetPathLabel.c_str(), m_CreateNewMeshPopupData.CreateAnimationFilenameBuffer.data(), 256);
					}
					UI::EndPropertyGrid();
					grid++;
				}

				X2_CORE_ASSERT(m_CreateNewMeshPopupData.MeshToCreate);

				if (ImGui::Button("Create"))
				{
					SelectionManager::DeselectAll(SelectionContext::Scene);
					Entity entity;
					if (doImportMesh)
					{
						if (!m_CreateNewMeshPopupData.MeshToCreate->GetSubmeshes().empty())
						{
							std::string serializePath = m_CreateNewMeshPopupData.CreateMeshFilenameBuffer.data();
							std::filesystem::path path = Project::GetActive()->GetMeshPath() / serializePath;
							if (!FileSystem::Exists(path.parent_path()))
								FileSystem::CreateDirectory(path.parent_path());

							if (meshType == MeshType::Static)
							{
								Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>(path.filename().string(), path.parent_path().string(), m_CreateNewMeshPopupData.MeshToCreate);

								entity = m_CreateNewMeshPopupData.TargetEntity;
								if (entity)
								{
									if (!entity.HasComponent<StaticMeshComponent>())
										entity.AddComponent<StaticMeshComponent>();

									StaticMeshComponent& mc = entity.GetComponent<StaticMeshComponent>();
									mc.StaticMesh = mesh->Handle;
								}
								else
								{
									const auto& meshMetadata = Project::GetEditorAssetManager()->GetMetadata(mesh->Handle);
									entity = m_EditorScene->CreateEntity(meshMetadata.FilePath.stem().string());
									entity.AddComponent<StaticMeshComponent>(mesh->Handle);
									SelectionManager::Select(SelectionContext::Scene, entity.GetUUID());
								}

								
							}
							else
							{
								Ref<Mesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<Mesh>(path.filename().string(), path.parent_path().string(), m_CreateNewMeshPopupData.MeshToCreate);

								entity = m_CreateNewMeshPopupData.TargetEntity;
								if (entity)
								{
									if (!entity.HasComponent<MeshComponent>())
										entity.AddComponent<MeshComponent>();

									MeshComponent& mc = entity.GetComponent<MeshComponent>();
									mc.Mesh = mesh->Handle;
								}
								else
								{
									entity = m_EditorScene->InstantiateMesh(mesh, doGenerateColliders);
									SelectionManager::Select(SelectionContext::Scene, entity.GetUUID());
								}
							}
						}
						else
						{
							UI_ShowNoMeshPopup();
						}
					}

				
					m_CreateNewMeshPopupData = {};
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("Cancel"))
				{
					m_CreateNewMeshPopupData = {};
					ImGui::CloseCurrentPopup();
				}
			}, 600);
	}

	void EditorLayer::UI_ShowInvalidAssetMetadataPopup()
	{
		UI::ShowMessageBox("Invalid Asset Metadata", [&]()
			{
				ImGui::TextWrapped("You tried to use an asset with invalid metadata. This can happen when an asset has a reference to another non-existent asset, or when as asset is empty.");
				ImGui::Separator();

				auto& metadata = m_InvalidAssetMetadataPopupData.Metadata;
				UI::BeginPropertyGrid();
				const auto& filepath = metadata.FilePath.string();
				UI::Property("Asset Filepath", filepath);
				UI::Property("Asset ID", fmt::format("{0}", (uint64_t)metadata.Handle));
				UI::EndPropertyGrid();

				if (ImGui::Button("OK"))
					ImGui::CloseCurrentPopup();
			}, 400);
	}

	void EditorLayer::UI_ShowNoMeshPopup()
	{
		UI::ShowMessageBox("No Mesh", []()
			{
				ImGui::TextWrapped("A mesh asset was not created because no meshes were found in the source asset.");
				if (ImGui::Button("OK"))
				{
					ImGui::CloseCurrentPopup();
				}
			}, 400);
	}

	void EditorLayer::UI_ShowNoSkeletonPopup()
	{
		UI::ShowMessageBox("No Skeleton", []()
			{
				ImGui::TextWrapped("A skeleton asset was not created because no skeletal animation rig was found in the source asset.  (Note: if the source asset does contain a skeleton but is unskinned (i.e. no mesh), then Assimp will not load that skeleton).");
				if (ImGui::Button("OK"))
				{
					ImGui::CloseCurrentPopup();
				}
			}, 400);
	}

	void EditorLayer::UI_ShowNoAnimationPopup()
	{
		UI::ShowMessageBox("No Animation", []()
			{
				ImGui::TextWrapped("An animation asset was not created because no animations were found in the source asset.");
				if (ImGui::Button("OK"))
				{
					ImGui::CloseCurrentPopup();
				}
			}, 400);
	}

	void EditorLayer::UI_ShowNewScenePopup()
	{
		static char s_NewSceneName[128];

		UI::ShowMessageBox("New Scene", [this]()
			{
				ImGui::BeginVertical("NewSceneVertical");
				ImGui::InputTextWithHint("##scene_name_input", "Name", s_NewSceneName, 128);

				ImGui::BeginHorizontal("NewSceneHorizontal");
				if (ImGui::Button("Create"))
				{
					NewScene(s_NewSceneName);
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::Button("Cancel"))
					ImGui::CloseCurrentPopup();

				ImGui::EndHorizontal();
				ImGui::EndVertical();
			}, 400);
	}

	void EditorLayer::UI_StatisticsPanel()
	{
		if (!m_ShowStatisticsPanel)
			return;

		if (ImGui::Begin("Statistics"))
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, Colours::Theme::backgroundDark);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, Colours::Theme::backgroundDark);

			ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
			if (ImGui::BeginTabBar("StatisticsTabs", tab_bar_flags))
			{
				Application& app = Application::Get();

				if (ImGui::BeginTabItem("Renderer"))
				{
					auto& caps = Renderer::GetCapabilities();
					ImGui::Text("Vendor: %s", caps.Vendor.c_str());
					ImGui::Text("Renderer: %s", caps.Device.c_str());
					ImGui::Text("Version: %s", caps.Version.c_str());
					ImGui::Separator();
					ImGui::Text("Frame Time: %.2fms\n", app.GetTimestep().GetMilliseconds());

#if 0
					if (RendererAPI::Current() == RendererAPIType::Vulkan)
					{
						GPUMemoryStats memoryStats = VulkanAllocator::GetStats();
						std::string used = Utils::BytesToString(memoryStats.Used);
						std::string free = Utils::BytesToString(memoryStats.Free);
						ImGui::Text("Used VRAM: %s", used.c_str());
						ImGui::Text("Free VRAM: %s", free.c_str());
						ImGui::Text("Descriptor Allocs: %d", VulkanRenderer::GetDescriptorAllocationCount(Renderer::RT_GetCurrentFrameIndex()));
					}
#endif
					bool vsync = app.GetWindow().IsVSync();
					if (ImGui::Checkbox("Vsync", &vsync))
						app.GetWindow().SetVSync(vsync);
					ImGui::EndTabItem();
				}
				
				if (ImGui::BeginTabItem("Performance"))
				{
					ImGui::Text("Frame Time: %.2fms\n", app.GetTimestep().GetMilliseconds());
					const auto& perFrameData = app.GetPerformanceProfiler()->GetPerFrameData();
					for (auto&& [name, time] : perFrameData)
					{
						ImGui::Text("%s: %.3fms\n", name, time);
					}
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Memory"))
				{
#if X2_TRACK_MEMORY
					const auto& allocStats = Memory::GetAllocationStats();
					const auto& allocStatsMap = Allocator::GetAllocationStats();

					{
						std::string totalAllocatedStr = Utils::BytesToString(allocStats.TotalAllocated);
						std::string totalFreedStr = Utils::BytesToString(allocStats.TotalFreed);
						std::string totalUsedStr = Utils::BytesToString(allocStats.TotalAllocated - allocStats.TotalFreed);

						ImGui::Text("Total allocated %s", totalAllocatedStr.c_str());
						ImGui::Text("Total freed %s", totalFreedStr.c_str());
						ImGui::Text("Current usage: %s", totalUsedStr.c_str());
					}

					ImGui::Separator();

					static std::string searchedString;
					UI::Widgets::SearchWidget(searchedString);


					struct MemoryRefEntry
					{
						const char* Category;
						size_t Size;
					};
					std::vector<MemoryRefEntry> sortedEntries;
					sortedEntries.reserve(allocStatsMap.size());
					for (auto& [category, stats] : allocStatsMap)
					{
						if (!UI::IsMatchingSearch(category, searchedString))
							continue;

						sortedEntries.push_back({ category, stats.TotalAllocated - stats.TotalFreed });
					}

					std::sort(sortedEntries.begin(), sortedEntries.end(), [](auto& a, auto& b) { return a.Size > b.Size; });

					for (const auto& entry : sortedEntries)
					{
						std::string usageStr = Utils::BytesToString(entry.Size);

						if (const char* slash = strstr(entry.Category, "\\"))
						{
							std::string tag = slash;
							auto lastSlash = tag.find_last_of("\\");
							if (lastSlash != std::string::npos)
								tag = tag.substr(lastSlash + 1, tag.size() - lastSlash);

							ImGui::TextColored(ImVec4(0.3f, 0.4f, 0.9f, 1.0f), "%s: %s", tag.c_str(), usageStr.c_str());
						}
						else
						{
							const char* category = entry.Category;
							if (category = strstr(entry.Category, "class"))
								category += 6;
							ImGui::Text("%s: %s", category, usageStr.c_str());

						}
					}
#else
					ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1.0f), "Memory is not being tracked because X2_TRACK_MEMORY is not defined!");
#endif

					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}

			ImGui::PopStyleColor(2);
		}

		ImGui::End();
	}

	void EditorLayer::OnCreateMeshFromMeshSource(Entity entity, Ref<MeshSource> meshSource)
	{
		m_CreateNewMeshPopupData.MeshToCreate = meshSource;
		m_CreateNewMeshPopupData.TargetEntity = entity;
		m_CreateNewMeshPopupData.CreateMeshFilenameBuffer.fill(0);
		UI_ShowCreateNewMeshPopup();
	}

	void EditorLayer::SceneHierarchyInvalidMetadataCallback(Entity entity, AssetHandle handle)
	{
		m_InvalidAssetMetadataPopupData.Metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
		UI_ShowInvalidAssetMetadataPopup();
	}

	void EditorLayer::SceneHierarchySetEditorCameraTransform(Entity entity)
	{
		TransformComponent& tc = entity.Transform();
		tc.SetTransform(glm::inverse(m_EditorCamera.GetViewMatrix()));
	}

	void EditorLayer::OnImGuiRender()
	{
		X2_PROFILE_FUNC();

		// ImGui + Dockspace Setup ------------------------------------------------------------------------------
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		auto boldFont = io.Fonts->Fonts[0];
		auto largeFont = io.Fonts->Fonts[1];

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_StartedCameraClickInViewport))
		{
			if (m_SceneState != SceneState::Play)
			{
				ImGui::FocusWindow(GImGui->HoveredWindow);
				Input::SetCursorMode(CursorMode::Normal);
			}
		}

		io.ConfigWindowsResizeFromEdges = io.BackendFlags & ImGuiBackendFlags_HasMouseCursors;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		bool isMaximized = (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
		ImGui::Begin("DockSpace Demo", nullptr, window_flags);
		ImGui::PopStyleColor(); // MenuBarBg
		ImGui::PopStyleVar(2);

		ImGui::PopStyleVar(2);

		{
			UI::ScopedColour windowBorder(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
			// Draw window border if the window is not maximized
			if (!isMaximized)
				UI::RenderWindowOuterBorders(ImGui::GetCurrentWindow());
		}
		UI_HandleManualWindowResize();

		const float titlebarHeight = UI_DrawTitlebar();
		ImGui::SetCursorPosY(titlebarHeight + ImGui::GetCurrentWindow()->WindowPadding.y);

		// Dockspace
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		ImGui::DockSpace(ImGui::GetID("MyDockspace"));
		style.WindowMinSize.x = minWinSizeX;

		// Editor Panel ------------------------------------------------------------------------------
		//ImGui::ShowDemoWindow();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Viewport");
		{
			m_ViewportPanelMouseOver = ImGui::IsWindowHovered();
			m_ViewportPanelFocused = ImGui::IsWindowFocused();

			Application::Get().GetImGuiLayer()->AllowInputEvents(!(m_ViewportPanelMouseOver && Input::IsMouseButtonDown(MouseButton::Right)));

			auto viewportOffset = ImGui::GetCursorPos(); // includes tab bar
			auto viewportSize = ImGui::GetContentRegionAvail();
			m_ViewportRenderer->SetViewportSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
			m_EditorScene->SetViewportSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
			if (m_RuntimeScene)
				m_RuntimeScene->SetViewportSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
			m_EditorCamera.SetViewportSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

			// Render viewport image
			UI::Image(m_ViewportRenderer->GetFinalPassImage(), viewportSize, { 0, 1 }, { 1, 0 });

			auto windowSize = ImGui::GetWindowSize();
			ImVec2 minBound = ImGui::GetWindowPos();

			// NOTE(Peter): This currently just subtracts 0 because I removed the toolbar window, but I'll keep it in just in case
			minBound.x -= viewportOffset.x;
			minBound.y -= viewportOffset.y;

			ImVec2 maxBound = { minBound.x + windowSize.x, minBound.y + windowSize.y };
			m_ViewportBounds[0] = { minBound.x, minBound.y };
			m_ViewportBounds[1] = { maxBound.x, maxBound.y };

			m_AllowViewportCameraEvents = (ImGui::IsMouseHoveringRect(minBound, maxBound) && m_ViewportPanelFocused) || m_StartedCameraClickInViewport;

			UI_GizmosToolbar();
			UI_CentralToolbar();
			UI_ViewportSettings();

			if (m_ShowGizmos && (m_ShowGizmosInPlayMode || m_CurrentScene != m_RuntimeScene))
				UI_DrawGizmos();

			UI_HandleAssetDrop();
		}
		ImGui::End();
		ImGui::PopStyleVar();

#if CameraDebug
		ImGui::Begin("Camera Debug");
		ImGui::Text(fmt::format("Distance: {}", m_EditorCamera.m_Distance).c_str());
		ImGui::Text(fmt::format("Focal Point: {}, {}, {}", m_EditorCamera.m_FocalPoint.x, m_EditorCamera.m_FocalPoint.y, m_EditorCamera.m_FocalPoint.z).c_str());
		ImGui::Text(fmt::format("Rotation: {}, {}, {}", m_EditorCamera.m_WorldRotation.x, m_EditorCamera.m_WorldRotation.y, m_EditorCamera.m_WorldRotation.z).c_str());
		ImGui::Text(fmt::format("Up Dir: {}, {}, {}", m_EditorCamera.GetUpDirection().x, m_EditorCamera.GetUpDirection().y, m_EditorCamera.GetUpDirection().z).c_str());
		ImGui::Text(fmt::format("Strafe Dir: {}, {}, {}", m_EditorCamera.GetRightDirection().x, m_EditorCamera.GetRightDirection().y, m_EditorCamera.GetRightDirection().z).c_str());
		ImGui::Text(fmt::format("Yaw: {}", m_EditorCamera.m_Yaw).c_str());
		ImGui::Text(fmt::format("Yaw Delta: {}", m_EditorCamera.m_YawDelta).c_str());
		ImGui::Text(fmt::format("Pitch: {}", m_EditorCamera.m_Pitch).c_str());
		ImGui::Text(fmt::format("Pitch Delta: {}", m_EditorCamera.m_PitchDelta).c_str());
		ImGui::Text(fmt::format("Position: ({}, {}, {})", m_EditorCamera.m_Position.x, m_EditorCamera.m_Position.y, m_EditorCamera.m_Position.z).c_str());
		ImGui::Text(fmt::format("Position Delta: ({}, {}, {})", m_EditorCamera.m_PositionDelta.x, m_EditorCamera.m_PositionDelta.y, m_EditorCamera.m_PositionDelta.z).c_str());
		ImGui::Text(fmt::format("View matrix: [{}, {}, {}, {}]", m_EditorCamera.m_ViewMatrix[0].x, m_EditorCamera.m_ViewMatrix[0].y, m_EditorCamera.m_ViewMatrix[0].z, m_EditorCamera.m_ViewMatrix[0].w).c_str());
		ImGui::Text(fmt::format("		      [{}, {}, {}, {}]", m_EditorCamera.m_ViewMatrix[1].x, m_EditorCamera.m_ViewMatrix[1].y, m_EditorCamera.m_ViewMatrix[1].z, m_EditorCamera.m_ViewMatrix[1].w).c_str());
		ImGui::Text(fmt::format("		      [{}, {}, {}, {}]", m_EditorCamera.m_ViewMatrix[2].x, m_EditorCamera.m_ViewMatrix[2].y, m_EditorCamera.m_ViewMatrix[2].z, m_EditorCamera.m_ViewMatrix[2].w).c_str());
		ImGui::Text(fmt::format("		      [{}, {}, {}, {}]", m_EditorCamera.m_ViewMatrix[3].x, m_EditorCamera.m_ViewMatrix[3].y, m_EditorCamera.m_ViewMatrix[3].z, m_EditorCamera.m_ViewMatrix[3].w).c_str());
		ImGui::End();
#endif

		if (m_ShowSecondViewport)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin("Second Viewport");

			m_ViewportPanel2MouseOver = ImGui::IsWindowHovered();
			m_ViewportPanel2Focused = ImGui::IsWindowFocused();

			auto viewportOffset = ImGui::GetCursorPos(); // includes tab bar
			auto viewportSize = ImGui::GetContentRegionAvail();
			if (viewportSize.x > 1 && viewportSize.y > 1)
			{
				m_SecondViewportRenderer->SetViewportSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
				m_SecondEditorCamera.SetViewportSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

				// Render viewport image
				UI::Image(m_SecondViewportRenderer->GetFinalPassImage(), viewportSize, { 0, 1 }, { 1, 0 });

				auto windowSize = ImGui::GetWindowSize();
				ImVec2 minBound = ImGui::GetWindowPos();
				minBound.x += viewportOffset.x;
				minBound.y += viewportOffset.y;

				ImVec2 maxBound = { minBound.x + windowSize.x, minBound.y + windowSize.y };
				m_SecondViewportBounds[0] = { minBound.x, minBound.y };
				m_SecondViewportBounds[1] = { maxBound.x, maxBound.y };

				if (m_ViewportPanel2MouseOver)
					UI_DrawGizmos();

				UI_HandleAssetDrop();
			}
			ImGui::End();
			ImGui::PopStyleVar();
		}
		else
		{
			m_ViewportPanel2MouseOver = false;
		}

		m_PanelManager->OnImGuiRender();

		if (m_ViewportPanelFocused)
			m_FocusedRenderer = m_ViewportRenderer;
		else if (m_ViewportPanel2Focused)
			m_FocusedRenderer = m_SecondViewportRenderer;

		if (m_ShowMetricsTool)
			ImGui::ShowMetricsWindow(&m_ShowMetricsTool);

		if (m_ShowStackTool)
			ImGui::ShowStackToolWindow(&m_ShowStackTool);

		if (m_ShowStyleEditor)
		{
			ImGui::Begin("ImGui Style Editor", &m_ShowStyleEditor);
			ImGui::ShowStyleEditor();
			ImGui::End();
		}

		ImGui::End();

		UI_StatisticsPanel();
		UI_BuildAssetPackDialog();

		UI::RenderMessageBoxes();

		AssetEditorPanel::OnImGuiRender();

		if (strlen(s_OpenProjectFilePathBuffer) > 0)
			OpenProject(s_OpenProjectFilePathBuffer);
	}

	void EditorLayer::OnEvent(Event& e)
	{
		AssetEditorPanel::OnEvent(e);
		m_PanelManager->OnEvent(e);

		if (m_SceneState == SceneState::Edit)
		{
			if (m_AllowViewportCameraEvents)
				m_EditorCamera.OnEvent(e);

			if (m_ViewportPanel2MouseOver)
				m_SecondEditorCamera.OnEvent(e);
		}
		else if (m_SceneState == SceneState::Simulate)
		{
			if (m_AllowViewportCameraEvents)
				m_EditorCamera.OnEvent(e);
		}

		m_CurrentScene->OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event) { return OnKeyPressedEvent(event); });
		dispatcher.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& event) { return OnMouseButtonPressed(event); });
		dispatcher.Dispatch<WindowTitleBarHitTestEvent>([this](WindowTitleBarHitTestEvent& event)
			{
				event.SetHit(UI_TitleBarHitTest(event.GetX(), event.GetY()));
				return true;
			});
		dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event)
			{
				if ((m_SceneState == SceneState::Play) || (m_SceneState == SceneState::Pause))
					OnSceneStop();
				else if (m_SceneState == SceneState::Simulate)
					OnSceneStopSimulation();

				return false; // give other things a chance to react to window close
			});

		dispatcher.Dispatch<EditorExitPlayModeEvent>([this](EditorExitPlayModeEvent& event)
			{
				if ((m_SceneState == SceneState::Play) || (m_SceneState == SceneState::Pause))
					OnSceneStop();
				return true;
			});
	}

	bool EditorLayer::OnKeyPressedEvent(KeyPressedEvent& e)
	{
		if (UI::IsWindowFocused("Viewport") || UI::IsWindowFocused("Scene Hierarchy"))
		{
			if ((m_ViewportPanelMouseOver || m_ViewportPanel2MouseOver) && !Input::IsMouseButtonDown(MouseButton::Right) && m_CurrentScene != m_RuntimeScene)
			{
				switch (e.GetKeyCode())
				{
				case KeyCode::Q:
					m_GizmoType = -1;
					break;
				case KeyCode::W:
					m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
					break;
				case KeyCode::E:
					m_GizmoType = ImGuizmo::OPERATION::ROTATE;
					break;
				case KeyCode::R:
					m_GizmoType = ImGuizmo::OPERATION::SCALE;
					break;
				case KeyCode::F:
				{
					if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
						break;

					// TODO(Peter): Maybe compute average location to focus on? Or maybe cycle through all the selected entities?
					UUID selectedEntityID = SelectionManager::GetSelections(SelectionContext::Scene).front();
					Entity selectedEntity = m_CurrentScene->GetEntityWithUUID(selectedEntityID);
					m_EditorCamera.Focus(m_CurrentScene->GetWorldSpaceTransform(selectedEntity).Translation);
					break;
				}
				}

			}

			switch (e.GetKeyCode())
			{
			case KeyCode::Escape:
				SelectionManager::DeselectAll();
				break;
			case KeyCode::P:
			{
				if (Input::IsKeyDown(KeyCode::LeftAlt))
				{
					if (m_SceneState == SceneState::Play)
						OnSceneStop();
					else
						OnScenePlay();
				}
				break;
			}
			case KeyCode::Delete:
			{
				// NOTE(Peter): Intentional copy since DeleteEntity modifies the selection map
				auto selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
				for (auto entityID : selectedEntities)
					DeleteEntity(m_CurrentScene->TryGetEntityWithUUID(entityID));
				break;
			}
			}
		}

		if (Input::IsKeyDown(X2_KEY_LEFT_CONTROL) && !Input::IsMouseButtonDown(MouseButton::Right))
		{
			switch (e.GetKeyCode())
			{
			case KeyCode::B:
				// Toggle bounding boxes 
				m_ShowBoundingBoxes = !m_ShowBoundingBoxes;
				break;
			case KeyCode::D:
			{
				auto selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
				for (const auto& entityID : selectedEntities)
				{
					Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
					Entity duplicate = m_CurrentScene->DuplicateEntity(entity);
					SelectionManager::Deselect(SelectionContext::Scene, entity.GetUUID());
					SelectionManager::Select(SelectionContext::Scene, duplicate.GetUUID());
				}
				break;
			}
			case KeyCode::G:
				// Toggle grid
				m_ViewportRenderer->GetOptions().ShowGrid = !m_ViewportRenderer->GetOptions().ShowGrid;
				break;
			case KeyCode::P:
			{
				auto& viewportRendererOptions = m_ViewportRenderer->GetOptions();
				viewportRendererOptions.ShowPhysicsColliders = !viewportRendererOptions.ShowPhysicsColliders;
				break;
			}
			case KeyCode::O:
				OpenScene();
				break;
			case KeyCode::S:
				SaveScene();
				break;
			}

			if (Input::IsKeyDown(X2_KEY_LEFT_SHIFT))
			{
				switch (e.GetKeyCode())
				{
				case KeyCode::S:
					SaveSceneAs();
					break;
				}
			}
		}

		if (m_SceneState == SceneState::Play && e.GetKeyCode() == KeyCode::Escape)
			Input::SetCursorMode(CursorMode::Normal);

		if (m_SceneState == SceneState::Play && Input::IsKeyDown(X2_KEY_LEFT_ALT))
		{
			if (e.GetRepeatCount() == 0)
			{
				switch (e.GetKeyCode())
				{
				case KeyCode::C:
					m_EditorCameraInRuntime = !m_EditorCameraInRuntime;
					break;
				}
			}
		}

		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		if (m_CurrentScene == m_RuntimeScene)
			return false;

		if (e.GetMouseButton() != MouseButton::Left)
			return false;

		if (!m_ViewportPanelMouseOver && !m_ViewportPanel2MouseOver)
			return false;

		if (Input::IsKeyDown(KeyCode::LeftAlt) || Input::IsMouseButtonDown(MouseButton::Right))
			return false;

		if (ImGuizmo::IsOver())
			return false;

		ImGui::ClearActiveID();

		std::vector<SelectionData> selectionData;

		auto [mouseX, mouseY] = GetMouseViewportSpace(m_ViewportPanelMouseOver);
		if (mouseX > -1.0f && mouseX < 1.0f && mouseY > -1.0f && mouseY < 1.0f)
		{
			const auto& camera = m_ViewportPanelMouseOver ? m_EditorCamera : m_SecondEditorCamera;
			auto [origin, direction] = CastRay(camera, mouseX, mouseY);

			auto meshEntities = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
			for (auto e : meshEntities)
			{
				Entity entity = { e, m_CurrentScene.Raw() };
				auto& mc = entity.GetComponent<MeshComponent>();
				auto mesh = AssetManager::GetAsset<Mesh>(mc.Mesh);
				if (!mesh || mesh->IsFlagSet(AssetFlag::Missing))
					continue;

				auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
				float lastT = std::numeric_limits<float>::max();
				auto& submesh = submeshes[mc.SubmeshIndex];
				glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
				Ray ray = {
					glm::inverse(transform) * glm::vec4(origin, 1.0f),
					glm::inverse(glm::mat3(transform)) * direction
				};

				float t;
				bool intersects = ray.IntersectsAABB(submesh.BoundingBox, t);
				if (intersects)
				{
					const auto& triangleCache = mesh->GetMeshSource()->GetTriangleCache(mc.SubmeshIndex);
					for (const auto& triangle : triangleCache)
					{
						if (ray.IntersectsTriangle(triangle.V0.Position, triangle.V1.Position, triangle.V2.Position, t))
						{
							selectionData.push_back({ entity, &submesh, t });
							break;
						}
					}
				}
			}

			auto staticMeshEntities = m_CurrentScene->GetAllEntitiesWith<StaticMeshComponent>();
			for (auto e : staticMeshEntities)
			{
				Entity entity = { e, m_CurrentScene.Raw() };
				auto& smc = entity.GetComponent<StaticMeshComponent>();
				auto staticMesh = AssetManager::GetAsset<StaticMesh>(smc.StaticMesh);
				if (!staticMesh || staticMesh->IsFlagSet(AssetFlag::Missing))
					continue;

				auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
				float lastT = std::numeric_limits<float>::max();
				for (uint32_t i = 0; i < submeshes.size(); i++)
				{
					auto& submesh = submeshes[i];
					glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
					Ray ray = {
						glm::inverse(transform * submesh.Transform) * glm::vec4(origin, 1.0f),
						glm::inverse(glm::mat3(transform * submesh.Transform)) * direction
					};

					float t;
					bool intersects = ray.IntersectsAABB(submesh.BoundingBox, t);
					if (intersects)
					{
						const auto& triangleCache = staticMesh->GetMeshSource()->GetTriangleCache(i);
						for (const auto& triangle : triangleCache)
						{
							if (ray.IntersectsTriangle(triangle.V0.Position, triangle.V1.Position, triangle.V2.Position, t))
							{
								selectionData.push_back({ entity, &submesh, t });
								break;
							}
						}
					}
				}
			}

			std::sort(selectionData.begin(), selectionData.end(), [](auto& a, auto& b) { return a.Distance < b.Distance; });

			bool ctrlDown = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);
			bool shiftDown = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);
			if (!ctrlDown)
				SelectionManager::DeselectAll();

			if (!selectionData.empty())
			{
				Entity entity = selectionData.front().Entity;
				if (shiftDown)
				{
					while (entity.GetParent())
					{
						entity = entity.GetParent();
					}
				}
				if (SelectionManager::IsSelected(SelectionContext::Scene, entity.GetUUID()) && ctrlDown)
					SelectionManager::Deselect(SelectionContext::Scene, entity.GetUUID());
				else
					SelectionManager::Select(SelectionContext::Scene, entity.GetUUID());
			}
		}

		return false;
	}

	std::pair<float, float> EditorLayer::GetMouseViewportSpace(bool primaryViewport)
	{
		auto [mx, my] = ImGui::GetMousePos();
		const auto& viewportBounds = primaryViewport ? m_ViewportBounds : m_SecondViewportBounds;
		mx -= viewportBounds[0].x;
		my -= viewportBounds[0].y;
		auto viewportWidth = viewportBounds[1].x - viewportBounds[0].x;
		auto viewportHeight = viewportBounds[1].y - viewportBounds[0].y;

		return { (mx / viewportWidth) * 2.0f - 1.0f, ((my / viewportHeight) * 2.0f - 1.0f) * -1.0f };
	}

	std::pair<glm::vec3, glm::vec3> EditorLayer::CastRay(const EditorCamera& camera, float mx, float my)
	{
		glm::vec4 mouseClipPos = { mx, my, -1.0f, 1.0f };

		auto inverseProj = glm::inverse(camera.GetProjectionMatrix());
		auto inverseView = glm::inverse(glm::mat3(camera.GetViewMatrix()));

		glm::vec4 ray = inverseProj * mouseClipPos;
		glm::vec3 rayPos = camera.GetPosition();
		glm::vec3 rayDir = inverseView * glm::vec3(ray);

		return { rayPos, rayDir };
	}

}
