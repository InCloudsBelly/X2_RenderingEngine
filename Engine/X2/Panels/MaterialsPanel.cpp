#include "Precompiled.h"
#include "MaterialsPanel.h"

#include "X2/ImGui/ImGui.h"

#include "X2/Renderer/Renderer.h"
#include "X2/Asset/AssetManager.h"
#include "X2/Editor/SelectionManager.h"

namespace X2 {

	MaterialsPanel::MaterialsPanel()
	{
		m_CheckerBoardTexture = CreateRef<VulkanTexture2D>(TextureSpecification(), std::filesystem::path(std::string(PROJECT_ROOT)+"Resources/Editor/Checkerboard.tga"));
	}

	MaterialsPanel::~MaterialsPanel() {}

	void MaterialsPanel::SetSceneContext(const Ref<Scene>& context)
	{
		m_Context = context;
	}

	void MaterialsPanel::OnImGuiRender(bool& isOpen)
	{
		if (SelectionManager::GetSelectionCount(SelectionContext::Scene) > 0)
		{
			m_SelectedEntity = m_Context->GetEntityWithUUID(SelectionManager::GetSelections(SelectionContext::Scene).front());
		}
		else
		{
			m_SelectedEntity = {};
		}

		const bool hasValidEntity = m_SelectedEntity && m_SelectedEntity.HasAny<MeshComponent, StaticMeshComponent>();

		ImGui::SetNextWindowSize(ImVec2(200.0f, 300.0f), ImGuiCond_Appearing);
		if (ImGui::Begin("Materials", &isOpen) && hasValidEntity)
		{
			const bool hasDynamicMesh = m_SelectedEntity.HasComponent<MeshComponent>() && AssetManager::IsAssetHandleValid(m_SelectedEntity.GetComponent<MeshComponent>().Mesh);
			const bool hasStaticMesh = m_SelectedEntity.HasComponent<StaticMeshComponent>() && AssetManager::IsAssetHandleValid(m_SelectedEntity.GetComponent<StaticMeshComponent>().StaticMesh);

			if (hasDynamicMesh || hasStaticMesh)
			{
				Ref<MaterialTable> meshMaterialTable, componentMaterialTable;

				if (m_SelectedEntity.HasComponent<MeshComponent>())
				{
					const auto& meshComponent = m_SelectedEntity.GetComponent<MeshComponent>();
					componentMaterialTable = meshComponent.MaterialTable;
					auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.Mesh);
					if (mesh)
						meshMaterialTable = mesh->GetMaterials();
				}
				else if (m_SelectedEntity.HasComponent<StaticMeshComponent>())
				{
					const auto& staticMeshComponent = m_SelectedEntity.GetComponent<StaticMeshComponent>();
					componentMaterialTable = staticMeshComponent.MaterialTable;
					auto mesh = AssetManager::GetAsset<StaticMesh>(staticMeshComponent.StaticMesh);
					if (mesh)
						meshMaterialTable = mesh->GetMaterials();
				}

				//X2_CORE_VERIFY(meshMaterialTable != nullptr && componentMaterialTable != nullptr);
				if (componentMaterialTable)
				{
					if (meshMaterialTable)
					{
						if (componentMaterialTable->GetMaterialCount() < meshMaterialTable->GetMaterialCount())
							componentMaterialTable->SetMaterialCount(meshMaterialTable->GetMaterialCount());
					}

					for (size_t i = 0; i < componentMaterialTable->GetMaterialCount(); i++)
					{
						bool hasComponentMaterial = componentMaterialTable->HasMaterial(i);
						bool hasMeshMaterial = meshMaterialTable && meshMaterialTable->HasMaterial(i);

						if (hasMeshMaterial && !hasComponentMaterial)
							RenderMaterial(i, meshMaterialTable->GetMaterial(i));
						else if (hasComponentMaterial)
							RenderMaterial(i, componentMaterialTable->GetMaterial(i));
					}
				}
			}
		}
		ImGui::End();
	}

	void MaterialsPanel::RenderMaterial(size_t materialIndex, AssetHandle materialAssetHandle)
	{
		Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(materialAssetHandle);
		auto& material = materialAsset->GetMaterial();
		auto shader = material->GetShader();

		Ref<VulkanShader> transparentShader = Renderer::GetShaderLibrary()->Get("PBR_Transparent");
		bool transparent = shader == transparentShader;

		std::string name = material->GetName();
		if (name.empty())
			name = "Unnamed Material";

		name = fmt::format("{0} ({1}", name, material->GetShader()->GetName());

		if (!UI::PropertyGridHeader(name, materialIndex == 0))
			return;

		// Textures ------------------------------------------------------------------------------
		{
			// Albedo
			if (ImGui::CollapsingHeader("Albedo", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

				auto& albedoColor = material->GetVector3("u_MaterialUniforms.AlbedoColor");
				Ref<VulkanTexture2D> albedoMap = material->TryGetTexture2D("u_AlbedoTexture");
				bool hasAlbedoMap = albedoMap && (albedoMap.get() != Renderer::GetWhiteTexture().get()) && albedoMap->GetImage();
				Ref<VulkanTexture2D> albedoUITexture = hasAlbedoMap ? albedoMap : m_CheckerBoardTexture;

				ImVec2 textureCursorPos = ImGui::GetCursorPos();

				UI::Image(albedoUITexture, ImVec2(64, 64));

				if (ImGui::BeginDragDropTarget())
				{
					auto data = ImGui::AcceptDragDropPayload("asset_payload");
					if (data)
					{
						int count = data->DataSize / sizeof(AssetHandle);

						for (int i = 0; i < count; i++)
						{
							if (count > 1)
								break;

							AssetHandle assetHandle = *(((AssetHandle*)data->Data) + i);
							Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
							if (!asset || asset->GetAssetType() != AssetType::Texture)
								break;

							albedoMap = std::dynamic_pointer_cast<VulkanTexture2D>(asset);
							material->Set("u_AlbedoTexture", albedoMap);
							//needsSerialize = true;
						}
					}

					ImGui::EndDragDropTarget();
				}

				ImGui::PopStyleVar();

				if (ImGui::IsItemHovered())
				{
					if (hasAlbedoMap)
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						std::string pathString = albedoMap->GetPath().string();
						ImGui::TextUnformatted(pathString.c_str());
						ImGui::PopTextWrapPos();
						UI::Image(albedoUITexture, ImVec2(384, 384));
						ImGui::EndTooltip();
					}

					if (ImGui::IsItemClicked())
					{
						std::string filepath = FileSystem::OpenFileDialog("").string();

						if (!filepath.empty())
						{
							TextureSpecification spec;
							spec.SRGB = true;
							albedoMap = AssetManager::CreateMemoryOnlyAssetReturnAsset<VulkanTexture2D>(spec, filepath);
							//albedoMap = CreateRef<VulkanTexture2D>(spec, filepath);
							material->Set("u_AlbedoTexture", albedoMap);
						}
					}
				}

				ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
				ImGui::SameLine();
				ImVec2 properCursorPos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(textureCursorPos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				if (hasAlbedoMap && ImGui::Button("X##AlbedoMap", ImVec2(18, 18)))
				{
					materialAsset->ClearAlbedoMap();
					//needsSerialize = true;
				}
				ImGui::PopStyleVar();
				ImGui::SetCursorPos(properCursorPos);

				if (ImGui::ColorEdit3("Color##Albedo", glm::value_ptr(albedoColor), ImGuiColorEditFlags_NoInputs))
					material->Set("u_MaterialUniforms.AlbedoColor", albedoColor);

				//if (ImGui::IsItemDeactivated())
				//	needsSerialize = true;

				//float& emissive = material->GetFloat("u_MaterialUniforms.Emission");
				//ImGui::SameLine();
				//ImGui::SetNextItemWidth(100.0f);
				//if (ImGui::DragFloat("Emission", &emissive, 0.1f, 0.0f, 20.0f))
				//	material->Set("u_MaterialUniforms.Emission", emissive);
				
				//if (ImGui::IsItemDeactivated())
				//	needsSerialize = true;

				ImGui::SetCursorPos(nextRowCursorPos);
			}
		}
		if (!transparent)
		{
			{
				// Normals
				if (ImGui::CollapsingHeader("Normals", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

					bool useNormalMap = material->GetFloat("u_MaterialUniforms.UseNormalMap");
					Ref<VulkanTexture2D> normalMap = material->TryGetTexture2D("u_NormalTexture");

					bool hasNormalMap = normalMap && (normalMap.get()!= Renderer::GetWhiteTexture().get()) && normalMap->GetImage();
					Ref<VulkanTexture2D> normalUITexture = hasNormalMap ? normalMap : m_CheckerBoardTexture;

					ImVec2 textureCursorPos = ImGui::GetCursorPos();

					UI::Image(normalUITexture, ImVec2(64, 64));

					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload("asset_payload");
						if (data)
						{
							int count = data->DataSize / sizeof(AssetHandle);

							for (int i = 0; i < count; i++)
							{
								if (count > 1)
									break;

								AssetHandle assetHandle = *(((AssetHandle*)data->Data) + i);
								Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
								if (!asset || asset->GetAssetType() != AssetType::Texture)
									break;

								normalMap = std::dynamic_pointer_cast<VulkanTexture2D>(asset);
								material->Set("u_NormalTexture", normalMap);
								material->Set("u_MaterialUniforms.UseNormalMap", true);
								//needsSerialize = true;
							}
						}

						ImGui::EndDragDropTarget();
					}

					ImGui::PopStyleVar();

					if (ImGui::IsItemHovered())
					{
						if (hasNormalMap)
						{
							ImGui::BeginTooltip();
							ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
							std::string pathString = normalMap->GetPath().string();
							ImGui::TextUnformatted(pathString.c_str());
							ImGui::PopTextWrapPos();
							UI::Image(normalUITexture, ImVec2(384, 384));
							ImGui::EndTooltip();
						}

						if (ImGui::IsItemClicked())
						{
							std::string filepath = FileSystem::OpenFileDialog("").string();

							if (!filepath.empty())
							{
								normalMap = AssetManager::CreateMemoryOnlyAssetReturnAsset<VulkanTexture2D>(TextureSpecification(), filepath);
								material->Set("u_NormalTexture", normalMap);
								material->Set("u_MaterialUniforms.UseNormalMap", true);
							}
						}
					}

					ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
					ImGui::SameLine();
					ImVec2 properCursorPos = ImGui::GetCursorPos();
					ImGui::SetCursorPos(textureCursorPos);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
					if (hasNormalMap && ImGui::Button("X##NormalMap", ImVec2(18, 18)))
					{
						materialAsset->ClearNormalMap();
						//needsSerialize = true;
					}
					ImGui::PopStyleVar();
					ImGui::SetCursorPos(properCursorPos);

					if (ImGui::Checkbox("Use##NormalMap", &useNormalMap))
						material->Set("u_MaterialUniforms.UseNormalMap", useNormalMap);
					//if (ImGui::IsItemDeactivated())
					//	needsSerialize = true;

					ImGui::SetCursorPos(nextRowCursorPos);
				}
			}
			{
				// MetallicRoughness
				if (ImGui::CollapsingHeader("MetallicRoughness", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

					float& roughnessValue = material->GetFloat("u_MaterialUniforms.Roughness");
					float& metallicValue = material->GetFloat("u_MaterialUniforms.Metalness");

					Ref<VulkanTexture2D> metallicRoughnessMap = material->TryGetTexture2D("u_MetallicRoughnessTexture");

					bool hasMetallicRoughnessMap = metallicRoughnessMap && (metallicRoughnessMap.get() != Renderer::GetWhiteTexture().get()) && metallicRoughnessMap->GetImage();
					Ref<VulkanTexture2D> roughnessUITexture = hasMetallicRoughnessMap ? metallicRoughnessMap : m_CheckerBoardTexture;

					ImVec2 textureCursorPos = ImGui::GetCursorPos();

					UI::Image(roughnessUITexture, ImVec2(64, 64));

					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload("asset_payload");
						if (data)
						{
							int count = data->DataSize / sizeof(AssetHandle);

							for (int i = 0; i < count; i++)
							{
								if (count > 1)
									break;

								AssetHandle assetHandle = *(((AssetHandle*)data->Data) + i);
								Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
								if (!asset || asset->GetAssetType() != AssetType::Texture)
									break;

								metallicRoughnessMap = std::dynamic_pointer_cast<VulkanTexture2D>(asset);
								material->Set("u_MetallicRoughnessTexture", metallicRoughnessMap);
								//needsSerialize = true;
							}
						}

						ImGui::EndDragDropTarget();
					}

					ImGui::PopStyleVar();

					if (ImGui::IsItemHovered())
					{
						if (hasMetallicRoughnessMap)
						{
							ImGui::BeginTooltip();
							ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
							std::string pathString = metallicRoughnessMap->GetPath().string();
							ImGui::TextUnformatted(pathString.c_str());
							ImGui::PopTextWrapPos();
							UI::Image(roughnessUITexture, ImVec2(384, 384));
							ImGui::EndTooltip();
						}

						if (ImGui::IsItemClicked())
						{
							std::string filepath = FileSystem::OpenFileDialog("").string();

							if (!filepath.empty())
							{
								metallicRoughnessMap = AssetManager::CreateMemoryOnlyAssetReturnAsset<VulkanTexture2D>(TextureSpecification(), filepath);
								material->Set("u_MetallicRoughnessTexture", metallicRoughnessMap);
							}
						}
					}

					ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
					ImGui::SameLine();
					ImVec2 properCursorPos = ImGui::GetCursorPos();
					ImGui::SetCursorPos(textureCursorPos);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
					if (hasMetallicRoughnessMap && ImGui::Button("X##MetallicRoughnessMap", ImVec2(18, 18)))
					{
						materialAsset->ClearMetallicRoughnessMap();
						//needsSerialize = true;
					}
					ImGui::PopStyleVar();
					ImGui::SetCursorPos(properCursorPos);
					ImGui::SetNextItemWidth(200.0f);
					if (ImGui::SliderFloat("Roughness Value##RoughnessInput", &roughnessValue, 0.0f, 1.0f))
						material->Set("u_MaterialUniforms.Roughness", roughnessValue);
					
					ImGui::SetCursorPosX(properCursorPos.x);
					ImGui::SetCursorPosY(properCursorPos.y + 1.5 * ImGui::GetTextLineHeight());
					ImGui::SetNextItemWidth(200.0f);
					if (ImGui::SliderFloat("Metalness Value##MetallicInput", &metallicValue, 0.0f, 1.0f))
						material->Set("u_MaterialUniforms.Metalness", metallicValue);
					//if (ImGui::IsItemDeactivated())
					//	needsSerialize = true;
					ImGui::SetCursorPos(nextRowCursorPos);
				}
			}

			{
				//Emission
				if (ImGui::CollapsingHeader("Emission", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

					float& emissionValue = material->GetFloat("u_MaterialUniforms.Emission");

					Ref<VulkanTexture2D> emissionMap = material->TryGetTexture2D("u_EmissionTexture");

					bool hasEmissionMap = emissionMap && (emissionMap.get() != Renderer::GetBlackTexture().get()) && emissionMap->GetImage();
					Ref<VulkanTexture2D> roughnessUITexture = hasEmissionMap ? emissionMap : m_CheckerBoardTexture;

					ImVec2 textureCursorPos = ImGui::GetCursorPos();

					UI::Image(roughnessUITexture, ImVec2(64, 64));

					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload("asset_payload");
						if (data)
						{
							int count = data->DataSize / sizeof(AssetHandle);

							for (int i = 0; i < count; i++)
							{
								if (count > 1)
									break;

								AssetHandle assetHandle = *(((AssetHandle*)data->Data) + i);
								Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
								if (!asset || asset->GetAssetType() != AssetType::Texture)
									break;

								emissionMap = std::dynamic_pointer_cast<VulkanTexture2D>(asset);
								material->Set("u_EmissionTexture", emissionMap);
								//needsSerialize = true;
							}
						}

						ImGui::EndDragDropTarget();
					}

					ImGui::PopStyleVar();

					if (ImGui::IsItemHovered())
					{
						if (hasEmissionMap)
						{
							ImGui::BeginTooltip();
							ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
							std::string pathString = emissionMap->GetPath().string();
							ImGui::TextUnformatted(pathString.c_str());
							ImGui::PopTextWrapPos();
							UI::Image(roughnessUITexture, ImVec2(384, 384));
							ImGui::EndTooltip();
						}

						if (ImGui::IsItemClicked())
						{
							std::string filepath = FileSystem::OpenFileDialog("").string();

							if (!filepath.empty())
							{
								emissionMap = AssetManager::CreateMemoryOnlyAssetReturnAsset<VulkanTexture2D>(TextureSpecification(), filepath);
								material->Set("u_EmissionTexture", emissionMap);
							}
						}
					}

					ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
					ImGui::SameLine();
					ImVec2 properCursorPos = ImGui::GetCursorPos();
					ImGui::SetCursorPos(textureCursorPos);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
					if (hasEmissionMap && ImGui::Button("X##EmissionMap", ImVec2(18, 18)))
					{
						materialAsset->ClearEmissionsMap();
						//needsSerialize = true;
					}
					ImGui::PopStyleVar();
					ImGui::SetCursorPos(properCursorPos);
					ImGui::SetNextItemWidth(200.0f);
					if (ImGui::SliderFloat("Emission Value##EmissionInput", &emissionValue, 0.0f, 1.0f))
						material->Set("u_MaterialUniforms.Emission", emissionValue);

					//if (ImGui::IsItemDeactivated())
					//	needsSerialize = true;
					ImGui::SetCursorPos(nextRowCursorPos);
				}
			}

		}

		UI::EndTreeNode();
	}

}
