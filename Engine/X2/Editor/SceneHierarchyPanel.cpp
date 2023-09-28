#include "Precompiled.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <imgui/imgui_internal.h>


#include "X2/Editor/ApplicationSettings.h"
#include "X2/Scene/Prefab.h"
#include "X2/Renderer/Mesh.h"
//#include "X2/Script/ScriptEngine.h"
//#include "X2/Physics/3D/PhysicsScene.h"
//#include "X2/Physics/3D/PhysicsLayer.h"
//#include "X2/Physics/3D/CookingFactory.h"
//#include "X2/Physics/3D/PhysicsSystem.h"
//#include "X2/Audio/AudioEngine.h"
#include "X2/Renderer/UI/Font.h"
//#include "X2/Audio/AudioComponent.h"

#include "X2/Asset/AssetManager.h"

#include "X2/ImGui/ImGui.h"
#include "X2/ImGui/CustomTreeNode.h"
#include "X2/ImGui/ImGuiWidgets.h"
#include "X2/Renderer/Renderer.h"

#include "X2/Core/Debug/Profiler.h"
#include "X2/Core/Event/MouseEvent.h"
#include "X2/Core/Event/KeyEvent.h"
#include "X2/Core/Input.h"

namespace X2 {

	static ImRect s_WindowBounds;
	static bool s_ActivateSearchWidget = false;

	SelectionContext SceneHierarchyPanel::s_ActiveSelectionContext = SelectionContext::Scene;

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context, SelectionContext selectionContext, bool isWindow)
		: m_Context(context), m_SelectionContext(selectionContext), m_IsWindow(isWindow)
	{
		if (m_Context)
			m_Context->SetEntityDestroyedCallback([this](Entity entity) { OnExternalEntityDestroyed(entity); });

		m_ComponentCopyScene = Scene::CreateEmpty();
		m_ComponentCopyEntity = m_ComponentCopyScene->CreateEntity();
	}

	void SceneHierarchyPanel::SetSceneContext(const Ref<Scene>& scene)
	{
		m_Context = scene;
		if (m_Context)
			m_Context->SetEntityDestroyedCallback([this](Entity entity) { OnExternalEntityDestroyed(entity); });
	}

	void SceneHierarchyPanel::OnImGuiRender(bool& isOpen)
	{
		X2_PROFILE_FUNC();
		if (m_IsWindow)
		{
			UI::ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Scene Hierarchy", &isOpen);
		}

		s_ActiveSelectionContext = m_SelectionContext;

		m_IsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		ImRect windowRect = { ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };

		{
			const float edgeOffset = 4.0f;
			UI::ShiftCursorX(edgeOffset * 3.0f);
			UI::ShiftCursorY(edgeOffset * 2.0f);

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - edgeOffset * 3.0f);

			static std::string searchedString;

			if (s_ActivateSearchWidget)
			{
				ImGui::SetKeyboardFocusHere();
				s_ActivateSearchWidget = false;
			}

			UI::Widgets::SearchWidget(searchedString);

			ImGui::Spacing();
			ImGui::Spacing();

			// Entity list
			//------------

			UI::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 0.0f));

			// Alt row colour
			const ImU32 colRowAlt = UI::ColourWithMultipliedValue(Colours::Theme::backgroundDark, 1.3f);
			UI::ScopedColour tableBGAlt(ImGuiCol_TableRowBgAlt, colRowAlt);

			// Table
			{
				// Scrollable Table uses child window internally
				UI::ScopedColour tableBg(ImGuiCol_ChildBg, Colours::Theme::backgroundDark);

				ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX
					| ImGuiTableFlags_Resizable
					| ImGuiTableFlags_Reorderable
					| ImGuiTableFlags_ScrollY
					/*| ImGuiTableFlags_RowBg *//*| ImGuiTableFlags_Sortable*/;

				const int numColumns = 3;
				if (ImGui::BeginTable("##SceneHierarchy-Table", numColumns, tableFlags, ImVec2(ImGui::GetContentRegionAvail())))
				{

					ImGui::TableSetupColumn("Label");
					ImGui::TableSetupColumn("Type");
					ImGui::TableSetupColumn("Visibility");

					// Headers
					{
						const ImU32 colActive = UI::ColourWithMultipliedValue(Colours::Theme::groupHeader, 1.2f);
						UI::ScopedColourStack headerColours(ImGuiCol_HeaderHovered, colActive,
							ImGuiCol_HeaderActive, colActive);

						ImGui::TableSetupScrollFreeze(ImGui::TableGetColumnCount(), 1);

						ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 22.0f);
						for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
						{
							ImGui::TableSetColumnIndex(column);
							const char* column_name = ImGui::TableGetColumnName(column);
							UI::ScopedID columnID(column);

							UI::ShiftCursor(edgeOffset * 3.0f, edgeOffset * 2.0f);
							ImGui::TableHeader(column_name);
							UI::ShiftCursor(-edgeOffset * 3.0f, -edgeOffset * 2.0f);
						}
						ImGui::SetCursorPosX(ImGui::GetCurrentTable()->OuterRect.Min.x);
						UI::Draw::Underline(true, 0.0f, 5.0f);
					}

					// List
					{
						UI::ScopedColourStack entitySelection(ImGuiCol_Header, IM_COL32_DISABLE,
							ImGuiCol_HeaderHovered, IM_COL32_DISABLE,
							ImGuiCol_HeaderActive, IM_COL32_DISABLE);

						for (auto entity : m_Context->GetAllEntitiesWith<IDComponent, RelationshipComponent>())
						{
							Entity e(entity, m_Context.get());
							if (e.GetParentUUID() == 0)
								DrawEntityNode({ entity, m_Context.get() }, searchedString);
						}
					}

					if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
					{
						DrawEntityCreateMenu({});
						ImGui::EndPopup();
					}


					ImGui::EndTable();
				}
			}

			s_WindowBounds = ImGui::GetCurrentWindow()->Rect();
		}

		if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetCurrentWindow()->ID))
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("scene_entity_hierarchy", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

			if (payload)
			{
				size_t count = payload->DataSize / sizeof(UUID);

				for (size_t i = 0; i < count; i++)
				{
					UUID entityID = *(((UUID*)payload->Data) + i);
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					m_Context->UnparentEntity(entity);
				}
			}

			ImGui::EndDragDropTarget();
		}

		{
			UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(2.0, 4.0f));
			ImGui::Begin("Properties");
			m_IsHierarchyOrPropertiesFocused = m_IsWindowFocused || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			DrawComponents(SelectionManager::GetSelections(s_ActiveSelectionContext));
			ImGui::End();
		}

		if (m_IsWindow)
			ImGui::End();
	}

	void SceneHierarchyPanel::OnEvent(Event& event)
	{
		if (!m_IsWindowFocused)
			return;

		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<MouseButtonReleasedEvent>([&](MouseButtonReleasedEvent& e)
			{
				if (ImGui::IsMouseHoveringRect(s_WindowBounds.Min, s_WindowBounds.Max, false) && !ImGui::IsAnyItemHovered())
				{
					m_FirstSelectedRow = -1;
					m_LastSelectedRow = -1;
					SelectionManager::DeselectAll();
					return true;
				}

				return false;
			});

		dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& e)
			{
				if (!m_IsWindowFocused)
					return false;

				switch (e.GetKeyCode())
				{
				case KeyCode::F:
				{
					s_ActivateSearchWidget = true;
					return true;
				}
				case KeyCode::Escape:
				{
					m_FirstSelectedRow = -1;
					m_LastSelectedRow = -1;
					break;
				}
				}

				return false;
			});
	}

	void SceneHierarchyPanel::DrawEntityCreateMenu(Entity parent)
	{
		if (!ImGui::BeginMenu("Create"))
			return;

		Entity newEntity;

		if (ImGui::MenuItem("Empty Entity"))
		{
			newEntity = m_Context->CreateEntity("Empty Entity");
		}

		if (ImGui::BeginMenu("Camera"))
		{
			if (ImGui::MenuItem("From View"))
			{
				newEntity = m_Context->CreateEntity("Camera");
				newEntity.AddComponent<CameraComponent>();

				for (auto& func : m_EntityContextMenuPlugins)
					func(newEntity);
			}

			if (ImGui::MenuItem("At World Origin"))
			{
				newEntity = m_Context->CreateEntity("Camera");
				newEntity.AddComponent<CameraComponent>();
			}

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Text"))
		{
			newEntity = m_Context->CreateEntity("Text");
			auto& textComp = newEntity.AddComponent<TextComponent>();
			textComp.FontHandle = Font::GetDefaultFont()->Handle;
		}

		if (ImGui::MenuItem("Sprite"))
		{
			newEntity = m_Context->CreateEntity("Sprite");
			auto& spriteComp = newEntity.AddComponent<SpriteRendererComponent>();
		}

		if (ImGui::BeginMenu("3D"))
		{
			if (ImGui::MenuItem("Cube"))
			{
				newEntity = m_Context->CreateEntity("Cube");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Cube.xsmesh");
				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<BoxColliderComponent>();
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Cube.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Cube.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Cube.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<BoxColliderComponent>();
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			if (ImGui::MenuItem("Sphere"))
			{
				newEntity = m_Context->CreateEntity("Sphere");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Sphere.xsmesh");
				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<SphereColliderComponent>();
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Sphere.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Sphere.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Sphere.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<SphereColliderComponent>();
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			if (ImGui::MenuItem("Capsule"))
			{
				newEntity = m_Context->CreateEntity("Capsule");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Capsule.xsmesh");

				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<CapsuleColliderComponent>();
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Capsule.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Capsule.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Capsule.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<CapsuleColliderComponent>();
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			if (ImGui::MenuItem("Cylinder"))
			{
				newEntity = m_Context->CreateEntity("Cylinder");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Cylinder.xsmesh");

				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<MeshColliderComponent>();
					//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Cylinder.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Cylinder.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Cylinder.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<MeshColliderComponent>();
							//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			if (ImGui::MenuItem("Torus"))
			{
				newEntity = m_Context->CreateEntity("Torus");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Torus.xsmesh");

				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<MeshColliderComponent>();
					//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Torus.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Torus.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Torus.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<MeshColliderComponent>();
							//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			if (ImGui::MenuItem("Plane"))
			{
				newEntity = m_Context->CreateEntity("Plane");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Plane.xsmesh");

				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<MeshColliderComponent>();
					//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Plane.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Plane.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Plane.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<MeshColliderComponent>();
							//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			if (ImGui::MenuItem("Cone"))
			{
				newEntity = m_Context->CreateEntity("Cone");
				auto mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Cone.xsmesh");

				if (mesh != 0)
				{
					newEntity.AddComponent<StaticMeshComponent>(mesh);
					//newEntity.AddComponent<MeshColliderComponent>();
					//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
				}
				else
				{
					std::string filePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/Source";
					std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
					if (std::filesystem::exists(filePath / std::filesystem::path("Cone.gltf")))
					{
						AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Default/Source/Cone.gltf");
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (asset)
						{
							Ref<StaticMesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>("Cone.xsmesh", targetFilePath, std::dynamic_pointer_cast<MeshSource>(asset));

							newEntity.AddComponent<StaticMeshComponent>(mesh->Handle);
							//newEntity.AddComponent<MeshColliderComponent>();
							//PhysicsSystem::GetOrCreateColliderAsset(newEntity, newEntity.GetComponent<MeshColliderComponent>());
						}
					}
					else
						X2_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Directional Light"))
		{
			newEntity = m_Context->CreateEntity("Directional Light");
			newEntity.AddComponent<DirectionalLightComponent>();
			newEntity.GetComponent<TransformComponent>().SetRotationEuler(glm::radians(glm::vec3{ 80.0f, 10.0f, 0.0f }));
		}

		if (ImGui::MenuItem("Point Light"))
		{
			newEntity = m_Context->CreateEntity("Point Light");
			newEntity.AddComponent<PointLightComponent>();
		}

		if (ImGui::MenuItem("Spot Light"))
		{
			newEntity = m_Context->CreateEntity("Spot Light");
			newEntity.AddComponent<SpotLightComponent>();
			newEntity.GetComponent<TransformComponent>().Translation = glm::vec3{ 0 };
			newEntity.GetComponent<TransformComponent>().SetRotationEuler(glm::radians(glm::vec3{ 90.0f, 0.0f, 0.0f }));
		}

		if (ImGui::MenuItem("Sky Light"))
		{
			newEntity = m_Context->CreateEntity("Sky Light");
			newEntity.AddComponent<SkyLightComponent>();
		}


		if (ImGui::MenuItem("Fog Volume"))
		{
			newEntity = m_Context->CreateEntity("Fog Volume");
			newEntity.AddComponent<FogVolumeComponent>();
		}

		ImGui::Separator();

		/*if (ImGui::MenuItem("Ambient Sound"))
		{
			newEntity = m_Context->CreateEntity("Ambient Sound");
			newEntity.AddComponent<AudioComponent>();
		}*/

		if (newEntity)
		{
			if (parent)
			{
				m_Context->ParentEntity(newEntity, parent);
				newEntity.Transform().Translation = glm::vec3(0.0f);
			}

			SelectionManager::DeselectAll();
			SelectionManager::Select(s_ActiveSelectionContext, newEntity.GetUUID());
		}

		ImGui::EndMenu();
	}

	bool SceneHierarchyPanel::TagSearchRecursive(Entity entity, std::string_view searchFilter, uint32_t maxSearchDepth, uint32_t currentDepth)
	{
		if (searchFilter.empty())
			return false;

		for (auto child : entity.Children())
		{
			Entity e = m_Context->GetEntityWithUUID(child);
			if (e.HasComponent<TagComponent>())
			{
				if (UI::IsMatchingSearch(e.GetComponent<TagComponent>().Tag, searchFilter))
					return true;
			}

			bool found = TagSearchRecursive(e, searchFilter, maxSearchDepth, currentDepth + 1);
			if (found)
				return true;
		}
		return false;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity, const std::string& searchFilter)
	{
		const char* name = "Unnamed Entity";
		if (entity.HasComponent<TagComponent>())
			name = entity.GetComponent<TagComponent>().Tag.c_str();

		const uint32_t maxSearchDepth = 10;
		bool hasChildMatchingSearch = TagSearchRecursive(entity, searchFilter, maxSearchDepth);

		if (!UI::IsMatchingSearch(name, searchFilter) && !hasChildMatchingSearch)
			return;

		const float edgeOffset = 4.0f;
		const float rowHeight = 21.0f;

		// ImGui item height tweaks
		auto* window = ImGui::GetCurrentWindow();
		window->DC.CurrLineSize.y = rowHeight;
		//---------------------------------------------
		ImGui::TableNextRow(0, rowHeight);

		// Label column
		//-------------

		ImGui::TableNextColumn();

		window->DC.CurrLineTextBaseOffset = 3.0f;

		const ImVec2 rowAreaMin = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Min;
		const ImVec2 rowAreaMax = { ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), ImGui::TableGetColumnCount() - 1).Max.x - 20,
									rowAreaMin.y + rowHeight };

		const bool isSelected = SelectionManager::IsSelected(s_ActiveSelectionContext, entity.GetUUID());

		ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		if (hasChildMatchingSearch)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;

		if (entity.Children().empty())
			flags |= ImGuiTreeNodeFlags_Leaf;


		const std::string strID = fmt::format("{0}{1}", name, (uint64_t)entity.GetUUID());

		ImGui::PushClipRect(rowAreaMin, rowAreaMax, false);
		bool isRowHovered, held;
		bool isRowClicked = ImGui::ButtonBehavior(ImRect(rowAreaMin, rowAreaMax), ImGui::GetID(strID.c_str()),
			&isRowHovered, &held, ImGuiButtonFlags_AllowItemOverlap | ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		bool wasRowRightClicked = ImGui::IsMouseReleased(ImGuiMouseButton_Right);

		ImGui::SetItemAllowOverlap();

		ImGui::PopClipRect();

		const bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// Row colouring
		//--------------

		// Fill with light selection colour if any of the child entities selected
		auto isAnyDescendantSelected = [&](Entity ent, auto isAnyDescendantSelected) -> bool
		{
			if (SelectionManager::IsSelected(s_ActiveSelectionContext, ent.GetUUID()))
				return true;

			if (!ent.Children().empty())
			{
				for (auto& childEntityID : ent.Children())
				{
					Entity childEntity = m_Context->GetEntityWithUUID(childEntityID);
					if (isAnyDescendantSelected(childEntity, isAnyDescendantSelected))
						return true;
				}
			}

			return false;
		};

		auto fillRowWithColour = [](const ImColor& colour)
		{
			for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, colour, column);
		};

		if (isSelected)
		{
			if (isWindowFocused || UI::NavigatedTo())
				fillRowWithColour(Colours::Theme::selection);
			else
			{
				const ImColor col = UI::ColourWithMultipliedValue(Colours::Theme::selection, 0.9f);
				fillRowWithColour(UI::ColourWithMultipliedSaturation(col, 0.7f));
			}
		}
		else if (isRowHovered)
		{
			fillRowWithColour(Colours::Theme::groupHeader);
		}
		else if (isAnyDescendantSelected(entity, isAnyDescendantSelected))
		{
			fillRowWithColour(Colours::Theme::selectionMuted);
		}

		// Text colouring
		//---------------

		if (isSelected)
			ImGui::PushStyleColor(ImGuiCol_Text, Colours::Theme::backgroundDark);

		const bool missingMesh = entity.HasComponent<MeshComponent>() && (AssetManager::IsAssetHandleValid(entity.GetComponent<MeshComponent>().Mesh)
			&& AssetManager::GetAsset<Mesh>(entity.GetComponent<MeshComponent>().Mesh) && AssetManager::GetAsset<Mesh>(entity.GetComponent<MeshComponent>().Mesh)->IsFlagSet(AssetFlag::Missing));

		const bool missingStaticMesh = entity.HasComponent<StaticMeshComponent>() && (AssetManager::IsAssetHandleValid(entity.GetComponent<StaticMeshComponent>().StaticMesh)
			&& AssetManager::GetAsset<StaticMesh>(entity.GetComponent<StaticMeshComponent>().StaticMesh) && AssetManager::GetAsset<StaticMesh>(entity.GetComponent<StaticMeshComponent>().StaticMesh)->IsFlagSet(AssetFlag::Missing));

		if (missingMesh || missingStaticMesh)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.3f, 1.0f));

		bool isPrefab = entity.HasComponent<PrefabComponent>();
		bool isValidPrefab = false;
		if (isPrefab)
			isValidPrefab = AssetManager::IsAssetHandleValid(entity.GetComponent<PrefabComponent>().PrefabID);

		if (isPrefab && !isSelected)
			ImGui::PushStyleColor(ImGuiCol_Text, isValidPrefab ? ImVec4(0.32f, 0.7f, 0.87f, 1.0f) : ImVec4(0.87f, 0.17f, 0.17f, 1.0f));

		// Tree node
		//----------
		// TODO: clean up this mess
		ImGuiContext& g = *GImGui;
		auto& style = ImGui::GetStyle();
		const ImVec2 label_size = ImGui::CalcTextSize(strID.c_str(), nullptr, false);
		const ImVec2 padding = ((flags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));
		const float text_offset_x = g.FontSize + padding.x * 2;           // Collapser arrow width + Spacing
		const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);                    // Latch before ItemSize changes it
		const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2 : 0.0f);  // Include collapser
		ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
		const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
		const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
		const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);

		bool previousState = ImGui::TreeNodeBehaviorIsOpen(ImGui::GetID(strID.c_str()));

		if (is_mouse_x_over_arrow && isRowClicked)
			ImGui::SetNextItemOpen(!previousState);

		if (!isSelected && isAnyDescendantSelected(entity, isAnyDescendantSelected))
			ImGui::SetNextItemOpen(true);

		const bool opened = ImGui::TreeNodeWithIcon(nullptr, ImGui::GetID(strID.c_str()), flags, name, nullptr);

		int32_t rowIndex = ImGui::TableGetRowIndex();
		if (rowIndex >= m_FirstSelectedRow && rowIndex <= m_LastSelectedRow && !SelectionManager::IsSelected(entity.GetUUID()) && m_ShiftSelectionRunning)
		{
			SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());

			if (SelectionManager::GetSelectionCount(s_ActiveSelectionContext) == (m_LastSelectedRow - m_FirstSelectedRow) + 1)
			{
				m_ShiftSelectionRunning = false;
			}
		}

		const std::string rightClickPopupID = fmt::format("{0}-ContextMenu", strID);

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem(rightClickPopupID.c_str()))
		{
			{
				UI::ScopedColour colText(ImGuiCol_Text, Colours::Theme::text);
				UI::ScopedColourStack entitySelection(ImGuiCol_Header, Colours::Theme::groupHeader,
					ImGuiCol_HeaderHovered, Colours::Theme::groupHeader,
					ImGuiCol_HeaderActive, Colours::Theme::groupHeader);

				if (!isSelected)
				{
					if (!Input::IsKeyDown(KeyCode::LeftControl))
						SelectionManager::DeselectAll();

					SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());
				}

				if (isPrefab && isValidPrefab)
				{
					if (ImGui::MenuItem("Update Prefab"))
					{
						AssetHandle prefabAssetHandle = entity.GetComponent<PrefabComponent>().PrefabID;
						Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(prefabAssetHandle);
						if (prefab)
							prefab->Create(entity);
						else
							X2_ERROR("Prefab has invalid asset handle: {0}", prefabAssetHandle);
					}
				}

				DrawEntityCreateMenu(entity);

				if (ImGui::MenuItem("Delete"))
					entityDeleted = true;

				ImGui::Separator();

				if (ImGui::MenuItem("Reset Transform to Mesh"))
					m_Context->ResetTransformsToMesh(entity, false);

				if (ImGui::MenuItem("Reset All Transforms to Mesh"))
					m_Context->ResetTransformsToMesh(entity, true);


				if (!m_EntityContextMenuPlugins.empty())
				{
					ImGui::Separator();

					if (ImGui::MenuItem("Set Transform to Editor Camera Transform"))
					{
						for (auto& func : m_EntityContextMenuPlugins)
						{
							func(entity);
						}
					}
				}
			}

			ImGui::EndPopup();
		}

		// Type column
		//------------
		if (isRowClicked)
		{
			if (wasRowRightClicked)
			{
				ImGui::OpenPopup(rightClickPopupID.c_str());
			}
			else
			{
				bool ctrlDown = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);
				bool shiftDown = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);
				if (shiftDown && SelectionManager::GetSelectionCount(s_ActiveSelectionContext) > 0)
				{
					SelectionManager::DeselectAll(s_ActiveSelectionContext);

					if (rowIndex < m_FirstSelectedRow)
					{
						m_LastSelectedRow = m_FirstSelectedRow;
						m_FirstSelectedRow = rowIndex;
					}
					else
					{
						m_LastSelectedRow = rowIndex;
					}

					m_ShiftSelectionRunning = true;
				}
				else if (!ctrlDown || shiftDown)
				{
					SelectionManager::DeselectAll();
					SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());
					m_FirstSelectedRow = rowIndex;
					m_LastSelectedRow = -1;
				}
				else
				{
					if (isSelected)
						SelectionManager::Deselect(s_ActiveSelectionContext, entity.GetUUID());
					else
						SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());
				}
			}

			ImGui::FocusWindow(ImGui::GetCurrentWindow());
		}

		if (missingMesh)
			ImGui::PopStyleColor();

		if (isSelected)
			ImGui::PopStyleColor();


		// Drag & Drop
		//------------
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			const auto& selectedEntities = SelectionManager::GetSelections(s_ActiveSelectionContext);
			UUID entityID = entity.GetUUID();

			if (!SelectionManager::IsSelected(s_ActiveSelectionContext, entityID))
			{
				const char* name = entity.Name().c_str();
				ImGui::TextUnformatted(name);
				ImGui::SetDragDropPayload("scene_entity_hierarchy", &entityID, 1 * sizeof(UUID));
			}
			else
			{
				for (const auto& selectedEntity : selectedEntities)
				{
					Entity e = m_Context->GetEntityWithUUID(selectedEntity);
					const char* name = e.Name().c_str();
					ImGui::TextUnformatted(name);
				}

				ImGui::SetDragDropPayload("scene_entity_hierarchy", selectedEntities.data(), selectedEntities.size() * sizeof(UUID));
			}

			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("scene_entity_hierarchy", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

			if (payload)
			{
				size_t count = payload->DataSize / sizeof(UUID);

				for (size_t i = 0; i < count; i++)
				{
					UUID droppedEntityID = *(((UUID*)payload->Data) + i);
					Entity droppedEntity = m_Context->GetEntityWithUUID(droppedEntityID);
					m_Context->ParentEntity(droppedEntity, entity);
				}
			}

			ImGui::EndDragDropTarget();
		}


		ImGui::TableNextColumn();
		if (isPrefab)
		{
			UI::ShiftCursorX(edgeOffset * 3.0f);

			if (isSelected)
				ImGui::PushStyleColor(ImGuiCol_Text, Colours::Theme::backgroundDark);

			ImGui::TextUnformatted("Prefab");
			ImGui::PopStyleColor();
		}

		// Draw children
		//--------------

		if (opened)
		{
			for (auto child : entity.Children())
				DrawEntityNode(m_Context->GetEntityWithUUID(child), "");

			ImGui::TreePop();
		}

		// Defer deletion until end of node UI
		if (entityDeleted)
		{
			// NOTE(Peter): Intentional copy since DestroyEntity would call EditorLayer::OnEntityDeleted which deselects the entity
			auto selectedEntities = SelectionManager::GetSelections(s_ActiveSelectionContext);
			for (auto entityID : selectedEntities)
				m_Context->DestroyEntity(m_Context->GetEntityWithUUID(entityID));
		}
	}


	enum class VectorAxis
	{
		X = BIT(0),
		Y = BIT(1),
		Z = BIT(2),
		W = BIT(3)
	};

	static bool DrawVec3Control(const std::string& label, glm::vec3& values, bool& manuallyEdited, float resetValue = 0.0f, float columnWidth = 100.0f, uint32_t renderMultiSelectAxes = 0)
	{
		bool modified = false;

		UI::PushID();
		ImGui::TableSetColumnIndex(0);
		UI::ShiftCursor(17.0f, 7.0f);

		ImGui::Text(label.c_str());
		UI::Draw::Underline(false, 0.0f, 2.0f);

		ImGui::TableSetColumnIndex(1);
		UI::ShiftCursor(7.0f, 0.0f);

		{
			const float spacingX = 8.0f;
			UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2{ spacingX, 0.0f });
			UI::ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 2.0f });

			{
				// Begin XYZ area
				UI::ScopedColour padding(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
				UI::ScopedColour frame(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

				ImGui::BeginChild(ImGui::GetID((label + "fr").c_str()),
					ImVec2(ImGui::GetContentRegionAvail().x - spacingX, ImGui::GetFrameHeightWithSpacing() + 8.0f),
					false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			}
			const float framePadding = 2.0f;
			const float outlineSpacing = 1.0f;
			const float lineHeight = GImGui->Font->FontSize + framePadding * 2.0f;
			const ImVec2 buttonSize = { lineHeight + 2.0f, lineHeight };
			const float inputItemWidth = (ImGui::GetContentRegionAvail().x - spacingX) / 3.0f - buttonSize.x;

			UI::ShiftCursor(0.0f, framePadding);

			const ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			auto drawControl = [&](const std::string& label, float& value, const ImVec4& colourN,
				const ImVec4& colourH,
				const ImVec4& colourP, bool renderMultiSelect)
			{
				{
					UI::ScopedStyle buttonFrame(ImGuiStyleVar_FramePadding, ImVec2(framePadding, 0.0f));
					UI::ScopedStyle buttonRounding(ImGuiStyleVar_FrameRounding, 1.0f);
					UI::ScopedColourStack buttonColours(ImGuiCol_Button, colourN,
						ImGuiCol_ButtonHovered, colourH,
						ImGuiCol_ButtonActive, colourP);

					UI::ScopedFont buttonFont(boldFont);

					UI::ShiftCursorY(2.0f);
					if (ImGui::Button(label.c_str(), buttonSize))
					{
						value = resetValue;
						modified = true;
					}
				}

				ImGui::SameLine(0.0f, outlineSpacing);
				ImGui::SetNextItemWidth(inputItemWidth);
				UI::ShiftCursorY(-2.0f);
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, renderMultiSelect);
				bool wasTempInputActive = ImGui::TempInputIsActive(ImGui::GetID(("##" + label).c_str()));
				modified |= UI::DragFloat(("##" + label).c_str(), &value, 0.1f, 0.0f, 0.0f, "%.2f", 0);

				// NOTE(Peter): Ugly hack to make tabbing behave the same as Enter (e.g marking it as manually modified)
				if (modified && Input::IsKeyDown(KeyCode::Tab))
					manuallyEdited = true;

				if (ImGui::TempInputIsActive(ImGui::GetID(("##" + label).c_str())))
					modified = false;

				ImGui::PopItemFlag();

				if (!UI::IsItemDisabled())
					UI::DrawItemActivityOutline(2.0f, true, Colours::Theme::accent);

				if (wasTempInputActive)
					manuallyEdited |= ImGui::IsItemDeactivatedAfterEdit();
			};

			drawControl("X", values.x, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f }, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f }, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f }, renderMultiSelectAxes & (uint32_t)VectorAxis::X);

			ImGui::SameLine(0.0f, outlineSpacing);
			drawControl("Y", values.y, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f }, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f }, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f }, renderMultiSelectAxes & (uint32_t)VectorAxis::Y);

			ImGui::SameLine(0.0f, outlineSpacing);
			drawControl("Z", values.z, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f }, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f }, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f }, renderMultiSelectAxes & (uint32_t)VectorAxis::Z);

			ImGui::EndChild();
		}
		UI::PopID();

		return modified || manuallyEdited;
	}

	template<typename TComponent>
	void DrawMaterialTable(SceneHierarchyPanel* _this, const std::vector<UUID>& entities, Ref<MaterialTable> meshMaterialTable, Ref<MaterialTable> localMaterialTable)
	{
		if (UI::BeginTreeNode("Materials"))
		{
			UI::BeginPropertyGrid();

			if (localMaterialTable->GetMaterialCount() != meshMaterialTable->GetMaterialCount())
				localMaterialTable->SetMaterialCount(meshMaterialTable->GetMaterialCount());

			for (uint32_t i = 0; i < (uint32_t)localMaterialTable->GetMaterialCount(); i++)
			{
				if (i == meshMaterialTable->GetMaterialCount())
					ImGui::Separator();

				bool hasLocalMaterial = localMaterialTable->HasMaterial(i);
				bool hasMeshMaterial = meshMaterialTable->HasMaterial(i);

				std::string label = fmt::format("[Material {0}]", i);

				// NOTE(Peter): Fix for weird ImGui ID bug...
				std::string id = fmt::format("{0}-{1}", label, i);
				ImGui::PushID(id.c_str());

				UI::PropertyAssetReferenceSettings settings;
				if (hasMeshMaterial && !hasLocalMaterial)
				{
					AssetHandle meshMaterialAssetHandle = meshMaterialTable->GetMaterial(i);
					Ref<MaterialAsset> meshMaterialAsset = AssetManager::GetAsset<MaterialAsset>(meshMaterialAssetHandle);
					std::string meshMaterialName = meshMaterialAsset->GetMaterial()->GetName();
					if (meshMaterialName.empty())
						meshMaterialName = "Unnamed Material";

					AssetHandle materialAssetHandle = meshMaterialAsset->Handle;

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, entities.size() > 1 && _this->IsInconsistentPrimitive<AssetHandle, TComponent>([i](const TComponent& component)
						{
							Ref<MaterialTable> materialTable = nullptr;
							if constexpr (std::is_same_v<TComponent, MeshComponent>)
								materialTable = AssetManager::GetAsset<Mesh>(component.Mesh)->GetMaterials();
							else
								materialTable = AssetManager::GetAsset<StaticMesh>(component.StaticMesh)->GetMaterials();

							if (!materialTable || i >= materialTable->GetMaterialCount())
								return (AssetHandle)0;

							return materialTable->GetMaterial(i);
						}));

					UI::PropertyAssetReferenceTarget<MaterialAsset>(label.c_str(), meshMaterialName.c_str(), materialAssetHandle, [_this, &entities, i, localMaterialTable](Ref<MaterialAsset> materialAsset) mutable
						{
							Ref<Scene> context = _this->GetSceneContext();

							for (auto entityID : entities)
							{
								Entity entity = context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<TComponent>();

								if (materialAsset == 0)
									component.MaterialTable->ClearMaterial(i);
								else
									component.MaterialTable->SetMaterial(i, materialAsset->Handle);
							}
						}, settings);

					ImGui::PopItemFlag();
				}
				else
				{
					// hasMeshMaterial is false, hasLocalMaterial could be true or false
					AssetHandle materialAssetHandle = 0;
					if (hasLocalMaterial)
					{
						materialAssetHandle = localMaterialTable->GetMaterial(i);
						settings.AdvanceToNextColumn = false;
						settings.WidthOffset = ImGui::GetStyle().ItemSpacing.x + 28.0f;
					}

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, entities.size() > 1 && _this->IsInconsistentPrimitive<AssetHandle, TComponent>([i, localMaterialTable](const TComponent& component)
						{
							Ref<MaterialTable> materialTable = component.MaterialTable;

							if (!materialTable || i >= materialTable->GetMaterialCount())
								return (AssetHandle)0;

							if (!materialTable->HasMaterial(i))
								return (AssetHandle)0;

							return materialTable->GetMaterial(i);
						}));

					UI::PropertyAssetReferenceTarget<MaterialAsset>(label.c_str(), nullptr, materialAssetHandle, [_this, &entities, i, localMaterialTable](Ref<MaterialAsset> materialAsset) mutable
						{
							Ref<Scene> context = _this->GetSceneContext();

							for (auto entityID : entities)
							{
								Entity entity = context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<TComponent>();

								if (materialAsset == 0)
									component.MaterialTable->ClearMaterial(i);
								else
									component.MaterialTable->SetMaterial(i, materialAsset->Handle);
							}
						}, settings);

					ImGui::PopItemFlag();
				}

				if (hasLocalMaterial)
				{
					ImGui::SameLine();
					float prevItemHeight = ImGui::GetItemRectSize().y;
					if (ImGui::Button(UI::GenerateLabelID("X"), { prevItemHeight, prevItemHeight }))
					{
						Ref<Scene> context = _this->GetSceneContext();

						for (auto entityID : entities)
						{
							Entity entity = context->GetEntityWithUUID(entityID);
							auto& component = entity.GetComponent<TComponent>();

							component.MaterialTable->ClearMaterial(i);
						}
					}
					ImGui::NextColumn();
				}

				ImGui::PopID();
			}

			UI::EndPropertyGrid();
			UI::EndTreeNode();
		}
	}

	template<typename TComponent, typename... TIncompatibleComponents>
	void DrawSimpleAddComponentButton(SceneHierarchyPanel* _this, const std::string& name, Ref<VulkanTexture2D> icon = nullptr)
	{
		bool canAddComponent = false;

		for (const auto& entityID : SelectionManager::GetSelections(SceneHierarchyPanel::GetActiveSelectionContext()))
		{
			Entity entity = _this->GetSceneContext()->GetEntityWithUUID(entityID);
			if (!entity.HasComponent<TComponent>())
			{
				canAddComponent = true;
				break;
			}
		}

		if (!canAddComponent)
			return;

		if (icon == nullptr)
			icon = EditorResources::AssetIcon;

		const float rowHeight = 25.0f;
		auto* window = ImGui::GetCurrentWindow();
		window->DC.CurrLineSize.y = rowHeight;
		ImGui::TableNextRow(0, rowHeight);
		ImGui::TableSetColumnIndex(0);

		window->DC.CurrLineTextBaseOffset = 3.0f;

		const ImVec2 rowAreaMin = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Min;
		const ImVec2 rowAreaMax = { ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), ImGui::TableGetColumnCount() - 1).Max.x - 20,
									rowAreaMin.y + rowHeight };

		ImGui::PushClipRect(rowAreaMin, rowAreaMax, false);
		bool isRowHovered, held;
		bool isRowClicked = ImGui::ButtonBehavior(ImRect(rowAreaMin, rowAreaMax), ImGui::GetID(name.c_str()), &isRowHovered, &held, ImGuiButtonFlags_AllowItemOverlap);
		ImGui::SetItemAllowOverlap();
		ImGui::PopClipRect();

		auto fillRowWithColour = [](const ImColor& colour)
		{
			for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, colour, column);
		};

		if (isRowHovered)
			fillRowWithColour(Colours::Theme::background);

		UI::ShiftCursor(1.5f, 1.5f);
		UI::Image(icon, { rowHeight - 3.0f, rowHeight - 3.0f });
		UI::ShiftCursor(-1.5f, -1.5f);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1);
		ImGui::TextUnformatted(name.c_str());

		if (isRowClicked)
		{
			for (const auto& entityID : SelectionManager::GetSelections(SceneHierarchyPanel::GetActiveSelectionContext()))
			{
				Entity entity = _this->GetSceneContext()->GetEntityWithUUID(entityID);

				if (sizeof...(TIncompatibleComponents) > 0 && entity.HasComponent<TIncompatibleComponents...>())
					continue;

				if (!entity.HasComponent<TComponent>())
					entity.AddComponent<TComponent>();
			}

			ImGui::CloseCurrentPopup();
		}
	}

	template<typename TComponent, typename... TIncompatibleComponents, typename OnAddedFunction>
	void DrawAddComponentButton(SceneHierarchyPanel* _this, const std::string& name, OnAddedFunction onComponentAdded, Ref<VulkanTexture2D> icon = nullptr)
	{
		bool canAddComponent = false;

		for (const auto& entityID : SelectionManager::GetSelections(SceneHierarchyPanel::GetActiveSelectionContext()))
		{
			Entity entity = _this->GetSceneContext()->GetEntityWithUUID(entityID);
			if (!entity.HasComponent<TComponent>())
			{
				canAddComponent = true;
				break;
			}
		}

		if (!canAddComponent)
			return;

		if (icon == nullptr)
			icon = EditorResources::AssetIcon;

		const float rowHeight = 25.0f;
		auto* window = ImGui::GetCurrentWindow();
		window->DC.CurrLineSize.y = rowHeight;
		ImGui::TableNextRow(0, rowHeight);
		ImGui::TableSetColumnIndex(0);

		window->DC.CurrLineTextBaseOffset = 3.0f;

		const ImVec2 rowAreaMin = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Min;
		const ImVec2 rowAreaMax = { ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), ImGui::TableGetColumnCount() - 1).Max.x - 20,
									rowAreaMin.y + rowHeight };

		ImGui::PushClipRect(rowAreaMin, rowAreaMax, false);
		bool isRowHovered, held;
		bool isRowClicked = ImGui::ButtonBehavior(ImRect(rowAreaMin, rowAreaMax), ImGui::GetID(name.c_str()), &isRowHovered, &held, ImGuiButtonFlags_AllowItemOverlap);
		ImGui::SetItemAllowOverlap();
		ImGui::PopClipRect();

		auto fillRowWithColour = [](const ImColor& colour)
		{
			for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, colour, column);
		};

		if (isRowHovered)
			fillRowWithColour(Colours::Theme::background);

		UI::ShiftCursor(1.5f, 1.5f);
		UI::Image(icon, { rowHeight - 3.0f, rowHeight - 3.0f });
		UI::ShiftCursor(-1.5f, -1.5f);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1);
		ImGui::TextUnformatted(name.c_str());

		if (isRowClicked)
		{
			for (const auto& entityID : SelectionManager::GetSelections(SceneHierarchyPanel::GetActiveSelectionContext()))
			{
				Entity entity = _this->GetSceneContext()->GetEntityWithUUID(entityID);

				if (sizeof...(TIncompatibleComponents) > 0 && entity.HasComponent<TIncompatibleComponents...>())
					continue;

				if (!entity.HasComponent<TComponent>())
				{
					auto& component = entity.AddComponent<TComponent>();
					onComponentAdded(entity, component);
				}
			}

			ImGui::CloseCurrentPopup();
		}
	}

	void SceneHierarchyPanel::DrawComponents(const std::vector<UUID>& entityIDs)
	{
		if (entityIDs.size() == 0)
			return;

		ImGui::AlignTextToFramePadding();

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		UI::ShiftCursor(4.0f, 4.0f);

		bool isHoveringID = false;
		const bool isMultiSelect = entityIDs.size() > 1;

		Entity firstEntity = m_Context->GetEntityWithUUID(entityIDs[0]);

		// Draw Tag Field
		{
			const float iconOffset = 6.0f;
			UI::ShiftCursor(4.0f, iconOffset);
			UI::Image(EditorResources::PencilIcon, ImVec2((float)EditorResources::PencilIcon->GetWidth(), (float)EditorResources::PencilIcon->GetHeight()),
				ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
				ImColor(128, 128, 128, 255).Value);

			ImGui::SameLine(0.0f, 4.0f);
			UI::ShiftCursorY(-iconOffset);

			const bool inconsistentTag = IsInconsistentString<TagComponent>([](const TagComponent& tagComponent) { return tagComponent.Tag; });
			const std::string& tag = (isMultiSelect && inconsistentTag) ? "---" : firstEntity.Name();

			char buffer[256];
			memset(buffer, 0, 256);
			buffer[0] = 0; // Setting the first byte to 0 makes checking if string is empty easier later.
			memcpy(buffer, tag.c_str(), tag.length());
			ImGui::PushItemWidth(contentRegionAvailable.x * 0.5f);
			UI::ScopedStyle frameBorder(ImGuiStyleVar_FrameBorderSize, 0.0f);
			UI::ScopedColour frameColour(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
			UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);

			if (Input::IsKeyDown(KeyCode::F2) && (m_IsHierarchyOrPropertiesFocused || UI::IsWindowFocused("Viewport")) && !ImGui::IsAnyItemActive())
				ImGui::SetKeyboardFocusHere();

			if (ImGui::InputText("##Tag", buffer, 256))
			{
				for (auto entityID : entityIDs)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					if (buffer[0] == 0)
						memcpy(buffer, "Unnamed Entity", 16); // if the entity has no name, the name will be set to Unnamed Entity, this prevents invisible entities in SHP.

					entity.GetComponent<TagComponent>().Tag = buffer;
				}
			}

			UI::DrawItemActivityOutline(2.0f, false, Colours::Theme::accent);

			isHoveringID = ImGui::IsItemHovered();

			ImGui::PopItemWidth();
		}

		ImGui::SameLine();
		UI::ShiftCursorX(-5.0f);

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

		ImVec2 addTextSize = ImGui::CalcTextSize(" ADD        ");
		addTextSize.x += GImGui->Style.FramePadding.x * 2.0f;

		{
			UI::ScopedColourStack addCompButtonColours(ImGuiCol_Button, IM_COL32(70, 70, 70, 200),
				ImGuiCol_ButtonHovered, IM_COL32(70, 70, 70, 255),
				ImGuiCol_ButtonActive, IM_COL32(70, 70, 70, 150));

			ImGui::SameLine(contentRegionAvailable.x - (addTextSize.x + GImGui->Style.FramePadding.x));
			if (ImGui::Button(" ADD       ", ImVec2(addTextSize.x + 4.0f, lineHeight + 2.0f)))
				ImGui::OpenPopup("AddComponentPanel");

			const float pad = 4.0f;
			const float iconWidth = ImGui::GetFrameHeight() - pad * 2.0f;
			const float iconHeight = iconWidth;
			ImVec2 iconPos = ImGui::GetItemRectMax();
			iconPos.x -= iconWidth + pad;
			iconPos.y -= iconHeight + pad;
			ImGui::SetCursorScreenPos(iconPos);
			UI::ShiftCursor(-5.0f, -1.0f);

			UI::Image(EditorResources::PlusIcon, ImVec2(iconWidth, iconHeight),
				ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
				ImColor(160, 160, 160, 255).Value);
		}

		float addComponentPanelStartY = ImGui::GetCursorScreenPos().y;

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		{
			UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);
			UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(5, 10));
			UI::ScopedStyle windowRounding(ImGuiStyleVar_PopupRounding, 4.0f);
			UI::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0));

			static float addComponentPanelWidth = 250.0f;
			ImVec2 windowPos = ImGui::GetWindowPos();
			const float maxHeight = ImGui::GetContentRegionMax().y - 60.0f;

			ImGui::SetNextWindowPos({ windowPos.x + addComponentPanelWidth / 1.3f, addComponentPanelStartY });
			ImGui::SetNextWindowSizeConstraints({ 50.0f, 50.0f }, { addComponentPanelWidth, maxHeight });
			if (ImGui::BeginPopup("AddComponentPanel", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking))
			{
				// Setup Table
				if (ImGui::BeginTable("##component_table", 2, ImGuiTableFlags_SizingStretchSame))
				{
					ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, addComponentPanelWidth * 0.15f);
					ImGui::TableSetupColumn("ComponentNames", ImGuiTableColumnFlags_WidthFixed, addComponentPanelWidth * 0.85f);

					DrawSimpleAddComponentButton<CameraComponent>(this, "Camera", EditorResources::CameraIcon);
					DrawSimpleAddComponentButton<MeshComponent, StaticMeshComponent>(this, "Mesh", EditorResources::MeshIcon);
					DrawSimpleAddComponentButton<StaticMeshComponent, MeshComponent>(this, "Static Mesh", EditorResources::StaticMeshIcon);
					DrawSimpleAddComponentButton<DirectionalLightComponent>(this, "Directional Light", EditorResources::DirectionalLightIcon);
					DrawSimpleAddComponentButton<PointLightComponent>(this, "Point Light", EditorResources::PointLightIcon);
					DrawSimpleAddComponentButton<SpotLightComponent>(this, "Spot Light", EditorResources::SpotLightIcon);
					DrawSimpleAddComponentButton<SkyLightComponent>(this, "Sky Light", EditorResources::SkyLightIcon);
					DrawSimpleAddComponentButton<SpriteRendererComponent>(this, "Sprite Renderer", EditorResources::SpriteIcon);
					DrawSimpleAddComponentButton<FogVolumeComponent>(this, "Fog Volume", EditorResources::SpriteIcon);
					DrawAddComponentButton<TextComponent>(this, "Text", [](Entity entity, TextComponent& tc)
						{
							tc.FontHandle = Font::GetDefaultFont()->Handle;
						}, EditorResources::TextIcon);
				
					DrawSimpleAddComponentButton<CharacterControllerComponent>(this, "Character Controller", EditorResources::CharacterControllerIcon);
				

					ImGui::EndTable();
				}

				ImGui::EndPopup();
			}
		}

		const auto& editorSettings = ApplicationSettings::Get();
		if (editorSettings.AdvancedMode)
		{
			DrawComponent<PrefabComponent>("Prefab", [&](PrefabComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
				{
					UI::BeginPropertyGrid();

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<UUID, PrefabComponent>([](const PrefabComponent& other) { return other.PrefabID; }));
					if (UI::PropertyInput("Prefab ID", (uint64_t&)firstComponent.PrefabID))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<PrefabComponent>().PrefabID = firstComponent.PrefabID;
						}
					}
					ImGui::PopItemFlag();

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<UUID, PrefabComponent>([](const PrefabComponent& other) { return other.EntityID; }));
					if (UI::PropertyInput("Entity ID", (uint64_t&)firstComponent.EntityID))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<PrefabComponent>().EntityID = firstComponent.EntityID;
						}
					}

					ImGui::PopItemFlag();

					UI::EndPropertyGrid();
				});
		}

		DrawComponent<TransformComponent>("Transform", [&](TransformComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
				UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

				ImGui::BeginTable("transformComponent", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoClip);
				ImGui::TableSetupColumn("label_column", 0, 100.0f);
				ImGui::TableSetupColumn("value_column", ImGuiTableColumnFlags_IndentEnable | ImGuiTableColumnFlags_NoClip, ImGui::GetContentRegionAvail().x - 100.0f);

				bool translationManuallyEdited = false;
				bool rotationManuallyEdited = false;
				bool scaleManuallyEdited = false;

				if (isMultiEdit)
				{
					uint32_t translationAxes = GetInconsistentVectorAxis<glm::vec3, TransformComponent>([](const TransformComponent& other) { return other.Translation; });
					uint32_t rotationAxes = GetInconsistentVectorAxis<glm::vec3, TransformComponent>([](const TransformComponent& other) { return other.GetRotationEuler(); });
					uint32_t scaleAxes = GetInconsistentVectorAxis<glm::vec3, TransformComponent>([](const TransformComponent& other) { return other.Scale; });

					glm::vec3 translation = firstComponent.Translation;
					glm::vec3 rotation = glm::degrees(firstComponent.GetRotationEuler());
					glm::vec3 scale = firstComponent.Scale;

					glm::vec3 oldTranslation = translation;
					glm::vec3 oldRotation = rotation;
					glm::vec3 oldScale = scale;

					ImGui::TableNextRow();
					bool changed = DrawVec3Control("Translation", translation, translationManuallyEdited, 0.0f, 100.0f, translationAxes);

					ImGui::TableNextRow();
					changed |= DrawVec3Control("Rotation", rotation, rotationManuallyEdited, 0.0f, 100.0f, rotationAxes);

					ImGui::TableNextRow();
					changed |= DrawVec3Control("Scale", scale, scaleManuallyEdited, 1.0f, 100.0f, scaleAxes);

					if (changed)
					{
						if (translationManuallyEdited || rotationManuallyEdited || scaleManuallyEdited)
						{
							translationAxes = GetInconsistentVectorAxis(translation, oldTranslation);
							rotationAxes = GetInconsistentVectorAxis(rotation, oldRotation);
							scaleAxes = GetInconsistentVectorAxis(scale, oldScale);

							for (auto& entityID : entities)
							{
								Entity entity = m_Context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<TransformComponent>();

								if ((translationAxes & (uint32_t)VectorAxis::X) != 0)
									component.Translation.x = translation.x;
								if ((translationAxes & (uint32_t)VectorAxis::Y) != 0)
									component.Translation.y = translation.y;
								if ((translationAxes & (uint32_t)VectorAxis::Z) != 0)
									component.Translation.z = translation.z;

								glm::vec3 componentRotation = glm::degrees(component.GetRotationEuler());
								if ((rotationAxes & (uint32_t)VectorAxis::X) != 0)
									componentRotation.x = glm::radians(rotation.x);
								if ((rotationAxes & (uint32_t)VectorAxis::Y) != 0)
									componentRotation.y = glm::radians(rotation.y);
								if ((rotationAxes & (uint32_t)VectorAxis::Z) != 0)
									componentRotation.z = glm::radians(rotation.z);
								component.SetRotationEuler(componentRotation);

								if ((scaleAxes & (uint32_t)VectorAxis::X) != 0)
									component.Scale.x = scale.x;
								if ((scaleAxes & (uint32_t)VectorAxis::Y) != 0)
									component.Scale.y = scale.y;
								if ((scaleAxes & (uint32_t)VectorAxis::Z) != 0)
									component.Scale.z = scale.z;
							}
						}
						else
						{
							glm::vec3 translationDiff = translation - oldTranslation;
							glm::vec3 rotationDiff = rotation - oldRotation;
							glm::vec3 scaleDiff = scale - oldScale;

							for (auto& entityID : entities)
							{
								Entity entity = m_Context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<TransformComponent>();

								component.Translation += translationDiff;
								glm::vec3 componentRotation = component.GetRotationEuler();
								componentRotation += glm::radians(rotationDiff);
								component.SetRotationEuler(componentRotation);
								component.Scale += scaleDiff;
							}
						}
					}
				}
				else
				{
					Entity entity = m_Context->GetEntityWithUUID(entities[0]);
					auto& component = entity.GetComponent<TransformComponent>();

					ImGui::TableNextRow();
					DrawVec3Control("Translation", component.Translation, translationManuallyEdited);

					ImGui::TableNextRow();
					glm::vec3 rotation = glm::degrees(component.GetRotationEuler());
					if (DrawVec3Control("Rotation", rotation, rotationManuallyEdited))
						component.SetRotationEuler(glm::radians(rotation));

					ImGui::TableNextRow();
					DrawVec3Control("Scale", component.Scale, scaleManuallyEdited, 1.0f);
				}

				ImGui::EndTable();

				UI::ShiftCursorY(-8.0f);
				UI::Draw::Underline();

				UI::ShiftCursorY(18.0f);
			}, EditorResources::TransformIcon);

		DrawComponent<MeshComponent>("Mesh", [&](MeshComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				AssetHandle meshHandle = firstComponent.Mesh;
				Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(meshHandle);
				UI::BeginPropertyGrid();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<AssetHandle, MeshComponent>([](const MeshComponent& other) { return other.Mesh; }));
				UI::PropertyAssetReferenceError error;
				if (UI::PropertyAssetReferenceWithConversion<Mesh, MeshSource>("Mesh", meshHandle,
					[=](Ref<MeshSource> meshAsset)
					{
						if (m_MeshAssetConvertCallback && !isMultiEdit)
							m_MeshAssetConvertCallback(m_Context->GetEntityWithUUID(entities[0]), meshAsset);
					}, &error))
				{
					mesh = AssetManager::GetAsset<Mesh>(meshHandle);
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						auto& mc = entity.GetComponent<MeshComponent>();
						mc.Mesh = meshHandle;
						//mc.BoneEntityIds = m_Context->FindBoneEntityIds(entity, entity.GetParent(), mesh);
						if (mesh)
						{
							// Validate submesh index	
							mc.SubmeshIndex = glm::clamp<uint32_t>(mc.SubmeshIndex, 0, (uint32_t)mesh->GetMeshSource()->GetSubmeshes().size() - 1);
							// TODO(Yan): maybe prompt for this, this isn't always expected behaviour	
							// 
							//if (entity.HasComponent<MeshColliderComponent>())
							//{
							//	//CookingFactory::CookMesh(mcc.CollisionMesh);	
							//}
						}
					}
				}
					ImGui::PopItemFlag();
					if (error == UI::PropertyAssetReferenceError::InvalidMetadata)
					{
						if (m_InvalidMetadataCallback && !isMultiEdit)
							m_InvalidMetadataCallback(m_Context->GetEntityWithUUID(entities[0]), UI::s_PropertyAssetReferenceAssetHandle);
					}
					if (mesh)
					{
						uint32_t submeshIndex = firstComponent.SubmeshIndex;
						ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<uint32_t, MeshComponent>([](const MeshComponent& other) { return other.SubmeshIndex; }));
						if (UI::Property("Submesh Index", submeshIndex, 0, (uint32_t)mesh->GetMeshSource()->GetSubmeshes().size() - 1))
						{
							for (auto& entityID : entities)
							{
								Entity entity = m_Context->GetEntityWithUUID(entityID);
								auto& mc = entity.GetComponent<MeshComponent>();
								mc.SubmeshIndex = glm::clamp<uint32_t>(submeshIndex, 0, (uint32_t)mesh->GetMeshSource()->GetSubmeshes().size() - 1);
							}
						}
						ImGui::PopItemFlag();
					}
					UI::EndPropertyGrid();
					if (mesh && mesh->IsValid())
						DrawMaterialTable<MeshComponent>(this, entities, mesh->GetMaterials(), firstComponent.MaterialTable);
			}, EditorResources::MeshIcon);

		DrawComponent<StaticMeshComponent>("Static Mesh", [&](StaticMeshComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				Ref<StaticMesh> mesh = AssetManager::GetAsset<StaticMesh>(firstComponent.StaticMesh);
				AssetHandle meshHandle = firstComponent.StaticMesh;

				UI::BeginPropertyGrid();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<AssetHandle, StaticMeshComponent>([](const StaticMeshComponent& other) { return other.StaticMesh; }));

				UI::PropertyAssetReferenceError error;
				if (UI::PropertyAssetReferenceWithConversion<StaticMesh, MeshSource>("Static Mesh", meshHandle,
					[=](Ref<MeshSource> meshAsset)
					{
						if (m_MeshAssetConvertCallback && !isMultiEdit)
							m_MeshAssetConvertCallback(m_Context->GetEntityWithUUID(entities[0]), meshAsset);
					}, &error))
				{
					mesh = AssetManager::GetAsset<StaticMesh>(meshHandle);

					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						auto& mc = entity.GetComponent<StaticMeshComponent>();
						mc.StaticMesh = meshHandle;

						// TODO(Yan): maybe prompt for this, this isn't always expected behaviour
						/*if (entity.HasComponent<MeshColliderComponent>() && mesh)
						{
							auto& mcc = entity.GetComponent<MeshColliderComponent>();
							mcc.CollisionMesh = mc.StaticMesh;
							CookingFactory::CookMesh(mcc.CollisionMesh);
						}*/
					}
				}

					ImGui::PopItemFlag();

					if (error == UI::PropertyAssetReferenceError::InvalidMetadata)
					{
						if (m_InvalidMetadataCallback && !isMultiEdit)
							m_InvalidMetadataCallback(m_Context->GetEntityWithUUID(entities[0]), UI::s_PropertyAssetReferenceAssetHandle);
					}

					UI::EndPropertyGrid();

					if (mesh && mesh->IsValid())
						DrawMaterialTable<StaticMeshComponent>(this, entities, mesh->GetMaterials(), firstComponent.MaterialTable);
			}, EditorResources::StaticMeshIcon);

		
		DrawComponent<CameraComponent>("Camera", [&](CameraComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				// Projection Type
				const char* projTypeStrings[] = { "Perspective", "Orthographic" };
				int currentProj = (int)firstComponent.Camera.GetProjectionType();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<int, CameraComponent>([](const CameraComponent& other) { return (int)other.Camera.GetProjectionType(); }));
				if (UI::PropertyDropdown("Projection", projTypeStrings, 2, &currentProj))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetProjectionType((SceneCamera::ProjectionType)currentProj);
					}
				}
				ImGui::PopItemFlag();

				// Perspective parameters
				if (firstComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float verticalFOV = firstComponent.Camera.GetDegPerspectiveVerticalFOV();
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetDegPerspectiveVerticalFOV(); }));
					if (UI::Property("Vertical FOV", verticalFOV))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<CameraComponent>().Camera.SetDegPerspectiveVerticalFOV(verticalFOV);
						}
					}
					ImGui::PopItemFlag();

					float nearClip = firstComponent.Camera.GetPerspectiveNearClip();
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetPerspectiveNearClip(); }));
					if (UI::Property("Near Clip", nearClip))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(nearClip);
						}
					}
					ImGui::PopItemFlag();

					float farClip = firstComponent.Camera.GetPerspectiveFarClip();
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetPerspectiveFarClip(); }));
					if (UI::Property("Far Clip", farClip))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(farClip);
						}
					}
					ImGui::PopItemFlag();
				}

				// Orthographic parameters
				else if (firstComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = firstComponent.Camera.GetOrthographicSize();
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetOrthographicSize(); }));
					if (UI::Property("Size", orthoSize))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<CameraComponent>().Camera.SetOrthographicSize(orthoSize);
						}
					}
					ImGui::PopItemFlag();

					float nearClip = firstComponent.Camera.GetOrthographicNearClip();
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetOrthographicNearClip(); }));
					if (UI::Property("Near Clip", nearClip))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<CameraComponent>().Camera.SetOrthographicNearClip(nearClip);
						}
					}
					ImGui::PopItemFlag();

					float farClip = firstComponent.Camera.GetOrthographicFarClip();
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetOrthographicFarClip(); }));
					if (UI::Property("Far Clip", farClip))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<CameraComponent>().Camera.SetOrthographicFarClip(farClip);
						}
					}
					ImGui::PopItemFlag();
				}

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<bool, CameraComponent>([](const CameraComponent& other) { return other.Primary; }));
				if (UI::Property("Main Camera", firstComponent.Primary))
				{
					// Does this even make sense???
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Primary = firstComponent.Primary;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();
			}, EditorResources::CameraIcon);

		//DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](SpriteRendererComponent& firstComponent, std::vector<Entity>& entities, const bool isMultiEdit)
		//{
		//}, s_GearIcon);

		DrawComponent<TextComponent>("Text", [&](TextComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				const bool inconsistentText = IsInconsistentString<TextComponent>([](const TextComponent& other) { return other.TextString; });

				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && inconsistentText);
				if (UI::PropertyMultiline("Text String", firstComponent.TextString))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						auto& textComponent = entity.GetComponent<TextComponent>();
						textComponent.TextString = firstComponent.TextString;
						textComponent.TextHash = std::hash<std::string>()(textComponent.TextString);
					}
				}
				ImGui::PopItemFlag();

				{
					UI::PropertyAssetReferenceSettings settings;
					bool customFont = firstComponent.FontHandle != Font::GetDefaultFont()->Handle;
					if (customFont)
					{
						settings.AdvanceToNextColumn = false;
						settings.WidthOffset = ImGui::GetStyle().ItemSpacing.x + 28.0f;
					}

					const bool inconsistentFont = IsInconsistentPrimitive<AssetHandle, TextComponent>([](const TextComponent& other) { return other.FontHandle; });
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && inconsistentFont);
					if (UI::PropertyAssetReference<Font>("Font", firstComponent.FontHandle, nullptr, settings))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<TextComponent>().FontHandle = firstComponent.FontHandle;
						}
					}
					ImGui::PopItemFlag();

					if (customFont)
					{
						ImGui::SameLine();
						float prevItemHeight = ImGui::GetItemRectSize().y;
						if (ImGui::Button("X", { prevItemHeight, prevItemHeight }))
						{
							for (auto& entityID : entities)
							{
								Entity entity = m_Context->GetEntityWithUUID(entityID);
								entity.GetComponent<TextComponent>().FontHandle = Font::GetDefaultFont()->Handle;
							}
						}
						ImGui::NextColumn();
					}
				}

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec4, TextComponent>([](const TextComponent& other) { return other.Color; }));
				if (UI::PropertyColor("Color", firstComponent.Color))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<TextComponent>().Color = firstComponent.Color;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, TextComponent>([](const TextComponent& other) { return other.LineSpacing; }));
				if (UI::Property("Line Spacing", firstComponent.LineSpacing, 0.01f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<TextComponent>().LineSpacing = firstComponent.LineSpacing;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, TextComponent>([](const TextComponent& other) { return other.Kerning; }));
				if (UI::Property("Kerning", firstComponent.Kerning, 0.01f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<TextComponent>().Kerning = firstComponent.Kerning;
					}
				}
				ImGui::PopItemFlag();

				UI::Separator();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, TextComponent>([](const TextComponent& other) { return other.MaxWidth; }));
				if (UI::Property("Max Width", firstComponent.MaxWidth))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<TextComponent>().MaxWidth = firstComponent.MaxWidth;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();

			}, EditorResources::TextIcon);

		DrawComponent<DirectionalLightComponent>("Directional Light", [&](DirectionalLightComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit&& IsInconsistentPrimitive<glm::vec3, DirectionalLightComponent>([](const DirectionalLightComponent& other) { return other.Direction; }));
				bool directionManuallyEdited = false;
				if (UI::Property("Direction", firstComponent.Direction, 0.1f, -50.0f, 50.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<DirectionalLightComponent>().Direction = firstComponent.Direction;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec3, DirectionalLightComponent>([](const DirectionalLightComponent& other) { return other.Radiance; }));
				if (UI::PropertyColor("Radiance", firstComponent.Radiance))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<DirectionalLightComponent>().Radiance = firstComponent.Radiance;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, DirectionalLightComponent>([](const DirectionalLightComponent& other) { return other.Intensity; }));
				if (UI::Property("Intensity", firstComponent.Intensity, 0.1f, 0.0f, 1000.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<DirectionalLightComponent>().Intensity = firstComponent.Intensity;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<bool, DirectionalLightComponent>([](const DirectionalLightComponent& other) { return other.CastShadows; }));
				if (UI::Property("Cast Shadows", firstComponent.CastShadows))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<DirectionalLightComponent>().CastShadows = firstComponent.CastShadows;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<bool, DirectionalLightComponent>([](const DirectionalLightComponent& other) { return other.SoftShadows; }));
				if (UI::Property("Soft Shadows", firstComponent.SoftShadows))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<DirectionalLightComponent>().SoftShadows = firstComponent.SoftShadows;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, DirectionalLightComponent>([](const DirectionalLightComponent& other) { return other.LightSize; }));
				if (UI::Property("Source Size", firstComponent.LightSize))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<DirectionalLightComponent>().LightSize = firstComponent.LightSize;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();
			}, EditorResources::DirectionalLightIcon);

		DrawComponent<PointLightComponent>("Point Light", [&](PointLightComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec3, PointLightComponent>([](const PointLightComponent& other) { return other.Radiance; }));
				if (UI::PropertyColor("Radiance", firstComponent.Radiance))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<PointLightComponent>().Radiance = firstComponent.Radiance;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, PointLightComponent>([](const PointLightComponent& other) { return other.Intensity; }));
				if (UI::Property("Intensity", firstComponent.Intensity, 0.05f, 0.0f, 500.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<PointLightComponent>().Intensity = firstComponent.Intensity;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, PointLightComponent>([](const PointLightComponent& other) { return other.Radius; }));
				if (UI::Property("Radius", firstComponent.Radius, 0.1f, 0.0f, std::numeric_limits<float>::max()))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<PointLightComponent>().Radius = firstComponent.Radius;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, PointLightComponent>([](const PointLightComponent& other) { return other.Falloff; }));
				if (UI::Property("Falloff", firstComponent.Falloff, 0.005f, 0.0f, 1.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<PointLightComponent>().Falloff = firstComponent.Falloff;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();
			}, EditorResources::PointLightIcon);

		DrawComponent<SpotLightComponent>("Spot Light", [&](SpotLightComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit&& IsInconsistentPrimitive<glm::vec3, SpotLightComponent>([](const SpotLightComponent& other) { return other.Direction; }));
				bool directionManuallyEdited = false;
				if (UI::Property("Direction", firstComponent.Direction, 0.1f, -50.0f, 50.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().Direction = firstComponent.Direction;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec3, SpotLightComponent>([](const SpotLightComponent& other) { return other.Radiance; }));
				if (UI::PropertyColor("Radiance", firstComponent.Radiance))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().Radiance = firstComponent.Radiance;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SpotLightComponent>([](const SpotLightComponent& other) { return other.Intensity; }));
				if (UI::Property("Intensity", firstComponent.Intensity, 0.1f, 0.0f, 1000.0f))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().Intensity = glm::clamp(firstComponent.Intensity, 0.0f, 1000.0f);
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SpotLightComponent>([](const SpotLightComponent& other) { return other.Angle; }));
				if (UI::Property("Angle", firstComponent.Angle))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().Angle = glm::clamp(firstComponent.Angle, 0.1f, 180.0f);
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SpotLightComponent>([](const SpotLightComponent& other) { return other.AngleAttenuation; }));
				if (UI::Property("Angle Attenuation", firstComponent.AngleAttenuation, 0.01f, 0.0f, std::numeric_limits<float>::max()))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().AngleAttenuation = glm::max(firstComponent.AngleAttenuation, 0.0f);
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SpotLightComponent>([](const SpotLightComponent& other) { return other.Falloff; }));
				if (UI::Property("Falloff", firstComponent.Falloff, 0.01f, 0.0f, 1.0f))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().Falloff = glm::clamp(firstComponent.Falloff, 0.0f, 1.0f);
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SpotLightComponent>([](const SpotLightComponent& other) { return other.Range; }));
				if (UI::Property("Range", firstComponent.Range, 0.1f, 0.0f, std::numeric_limits<float>::max()))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().Range = glm::max(firstComponent.Range, 0.0f);
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<bool, SpotLightComponent>([](const SpotLightComponent& other) { return other.CastsShadows; }));
				if (UI::Property("Cast Shadows", firstComponent.CastsShadows))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().CastsShadows = firstComponent.CastsShadows;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<bool, SpotLightComponent>([](const SpotLightComponent& other) { return other.SoftShadows; }));
				if (UI::Property("Soft Shadows", firstComponent.SoftShadows))
				{
					for (auto entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpotLightComponent>().SoftShadows = firstComponent.SoftShadows;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();
			}, EditorResources::SpotLightIcon);

		DrawComponent<SkyLightComponent>("Sky Light", [&](SkyLightComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<AssetHandle, SkyLightComponent>([](const SkyLightComponent& other) { return other.SceneEnvironment; }));
				if (UI::PropertyAssetReference<Environment>("Environment Map", firstComponent.SceneEnvironment))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SkyLightComponent>().SceneEnvironment = firstComponent.SceneEnvironment;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SkyLightComponent>([](const SkyLightComponent& other) { return other.Intensity; }));
				if (UI::Property("Intensity", firstComponent.Intensity, 0.01f, 0.0f, 5.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SkyLightComponent>().Intensity = firstComponent.Intensity;
					}
				}
				ImGui::PopItemFlag();

				if (AssetManager::IsAssetHandleValid(firstComponent.SceneEnvironment))
				{
					auto environment = AssetManager::GetAsset<Environment>(firstComponent.SceneEnvironment);
					bool lodChanged = false;
					if (environment && environment->RadianceMap)
					{
						ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<uint32_t, SkyLightComponent>([](const SkyLightComponent& other)
							{
								auto otherEnv = AssetManager::GetAsset<Environment>(other.SceneEnvironment);
								return otherEnv->RadianceMap->GetMipLevelCount();
							}));

						lodChanged = UI::PropertySlider("Lod", firstComponent.Lod, 0.0f, static_cast<float>(environment->RadianceMap->GetMipLevelCount()));
						ImGui::PopItemFlag();
					}
					else
					{
						UI::BeginDisabled();
						UI::PropertySlider("Lod", firstComponent.Lod, 0.0f, 10.0f);
						UI::EndDisabled();
					}
				}

				ImGui::Separator();

				const bool isInconsistentDynamicSky = IsInconsistentPrimitive<bool, SkyLightComponent>([](const SkyLightComponent& other) { return other.DynamicSky; });
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && isInconsistentDynamicSky);
				if (UI::Property("Dynamic Sky", firstComponent.DynamicSky))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SkyLightComponent>().DynamicSky = firstComponent.DynamicSky;
					}
				}
				ImGui::PopItemFlag();

				if (!isInconsistentDynamicSky || !isMultiEdit)
				{
					bool changed = false;

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SkyLightComponent>([](const SkyLightComponent& other) { return other.TurbidityAzimuthInclination.x; }));
					if (UI::Property("Turbidity", firstComponent.TurbidityAzimuthInclination.x, 0.01f, 1.8f, std::numeric_limits<float>::max()))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.x = firstComponent.TurbidityAzimuthInclination.x;
						}

						changed = true;
					}
					ImGui::PopItemFlag();

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SkyLightComponent>([](const SkyLightComponent& other) { return other.TurbidityAzimuthInclination.y; }));
					if (UI::Property("Azimuth", firstComponent.TurbidityAzimuthInclination.y, 0.01f))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.y = firstComponent.TurbidityAzimuthInclination.y;
						}

						changed = true;
					}
					ImGui::PopItemFlag();

					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SkyLightComponent>([](const SkyLightComponent& other) { return other.TurbidityAzimuthInclination.z; }));
					if (UI::Property("Inclination", firstComponent.TurbidityAzimuthInclination.z, 0.01f))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.z = firstComponent.TurbidityAzimuthInclination.z;
						}

						changed = true;
					}
					ImGui::PopItemFlag();

					if (changed)
					{
						if (AssetManager::IsMemoryAsset(firstComponent.SceneEnvironment))
						{
							Ref<VulkanTextureCube> preethamEnv = Renderer::CreatePreethamSky(firstComponent.TurbidityAzimuthInclination.x, firstComponent.TurbidityAzimuthInclination.y, firstComponent.TurbidityAzimuthInclination.z);
							Ref<Environment> env = AssetManager::GetAsset<Environment>(firstComponent.SceneEnvironment);
							if (env)
							{
								env->RadianceMap = preethamEnv;
								env->IrradianceMap = preethamEnv;
							}
						}
						else
						{
							Ref<VulkanTextureCube> preethamEnv = Renderer::CreatePreethamSky(firstComponent.TurbidityAzimuthInclination.x, firstComponent.TurbidityAzimuthInclination.y, firstComponent.TurbidityAzimuthInclination.z);
							firstComponent.SceneEnvironment = AssetManager::CreateMemoryOnlyAsset<Environment>(preethamEnv, preethamEnv, preethamEnv);
						}

						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<SkyLightComponent>().SceneEnvironment = firstComponent.SceneEnvironment;
						}
					}
				}
				UI::EndPropertyGrid();
			}, EditorResources::SkyLightIcon);

		
		DrawComponent<FogVolumeComponent>("FogVolume", [&](FogVolumeComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit&& IsInconsistentPrimitive<float, FogVolumeComponent>([](const FogVolumeComponent& other) { return other.fogDensity; }));
				if (UI::Property("FogDensity", firstComponent.fogDensity, 0.05f, 0.05f, 20.0f))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<FogVolumeComponent>().fogDensity = firstComponent.fogDensity;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();

			}, EditorResources::SpriteIcon);

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", [&](SpriteRendererComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
			{
				UI::BeginPropertyGrid();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec4, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.Color; }));
				if (UI::PropertyColor("Color", firstComponent.Color))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpriteRendererComponent>().Color = firstComponent.Color;
					}
				}
				ImGui::PopItemFlag();

				{
					UI::PropertyAssetReferenceSettings settings;
					bool textureSet = firstComponent.Texture != 0;
					if (textureSet)
					{
						settings.AdvanceToNextColumn = false;
						settings.WidthOffset = ImGui::GetStyle().ItemSpacing.x + 28.0f;
					}

					const bool inconsistentTexture = IsInconsistentPrimitive<AssetHandle, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.Texture; });
					ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && inconsistentTexture);
					if (UI::PropertyAssetReference<VulkanTexture2D>("Texture", firstComponent.Texture, nullptr, settings))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<SpriteRendererComponent>().Texture = firstComponent.Texture;
						}
					}
					ImGui::PopItemFlag();

					if (textureSet)
					{
						ImGui::SameLine();
						float prevItemHeight = ImGui::GetItemRectSize().y;
						if (ImGui::Button("X", { prevItemHeight, prevItemHeight }))
						{
							for (auto& entityID : entities)
							{
								Entity entity = m_Context->GetEntityWithUUID(entityID);
								entity.GetComponent<SpriteRendererComponent>().Texture = 0;
							}
						}
						ImGui::NextColumn();
					}
				}

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.TilingFactor; }));
				if (UI::Property("Tiling Factor", firstComponent.TilingFactor))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpriteRendererComponent>().TilingFactor = firstComponent.TilingFactor;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec2, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.UVStart; }));
				if (UI::Property("UV Start", firstComponent.UVStart))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpriteRendererComponent>().UVStart = firstComponent.UVStart;
					}
				}
				ImGui::PopItemFlag();

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<glm::vec2, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.UVEnd; }));
				if (UI::Property("UV End", firstComponent.UVEnd))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpriteRendererComponent>().UVEnd = firstComponent.UVEnd;
					}
				}
				ImGui::PopItemFlag();

				UI::EndPropertyGrid();
			}, EditorResources::SpriteIcon);

	}

	void SceneHierarchyPanel::OnExternalEntityDestroyed(Entity entity)
	{
		m_EntityDeletedCallback(entity);
	}

}
