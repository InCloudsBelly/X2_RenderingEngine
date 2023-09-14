#include "Precompiled.h"
#include "DefaultAssetEditors.h"
#include "X2/Asset/AssetImporter.h"
#include "X2/Asset/AssetManager.h"
#include "X2/Renderer/Renderer.h"
#include "X2/Editor/SelectionManager.h"

#include "imgui_internal.h"

namespace X2 {


	MaterialEditor::MaterialEditor()
		: AssetEditor("Material Editor")
	{
	}

	void MaterialEditor::OnOpen()
	{
		if (!m_MaterialAsset)
			SetOpen(false);
	}

	void MaterialEditor::OnClose()
	{
		m_MaterialAsset = nullptr;
	}

	void MaterialEditor::Render()
	{
		auto material = m_MaterialAsset->GetMaterial();

		Ref<VulkanShader> shader = material->GetShader();
		bool needsSerialize = false;

		Ref<VulkanShader> transparentShader = Renderer::GetShaderLibrary()->Get("PBR_Transparent");
		bool transparent = shader == transparentShader;
		UI::BeginPropertyGrid();
		UI::PushID();
		if (UI::Property("Transparent", transparent))
		{
			if (transparent)
				m_MaterialAsset->SetMaterial(CreateRef<VulkanMaterial>(transparentShader));
			else
				m_MaterialAsset->SetMaterial(CreateRef<VulkanMaterial>(Renderer::GetShaderLibrary()->Get("PBR_Static")));

			m_MaterialAsset->m_Transparent = transparent;
			m_MaterialAsset->SetDefaults();

			material = m_MaterialAsset->GetMaterial();
			shader = material->GetShader();
		}
		UI::PopID();
		UI::EndPropertyGrid();

		ImGui::Text("VulkanShader: %s", material->GetShader()->GetName().c_str());

		// Albedo
		if (ImGui::CollapsingHeader("Albedo", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

			auto& albedoColor = material->GetVector3("u_MaterialUniforms.AlbedoColor");
			Ref<VulkanTexture2D> albedoMap = material->TryGetTexture2D("u_AlbedoTexture");
			bool hasAlbedoMap = albedoMap ? (albedoMap.get()!= Renderer::GetWhiteTexture().get()) && albedoMap->GetImage() : false;
			Ref<VulkanTexture2D> albedoUITexture = hasAlbedoMap ? albedoMap : EditorResources::CheckerboardTexture;

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
						needsSerialize = true;
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
					std::string filepath = albedoMap->GetPath().string();
					ImGui::TextUnformatted(filepath.c_str());
					ImGui::PopTextWrapPos();
					UI::Image(albedoUITexture, ImVec2(384, 384));
					ImGui::EndTooltip();
				}
				if (ImGui::IsItemClicked())
				{
				}
			}

			ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
			ImGui::SameLine();
			ImVec2 properCursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(textureCursorPos);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			if (hasAlbedoMap && ImGui::Button("X##AlbedoMap", ImVec2(18, 18)))
			{
				m_MaterialAsset->ClearAlbedoMap();
				needsSerialize = true;
			}
			ImGui::PopStyleVar();
			ImGui::SetCursorPos(properCursorPos);

			ImGui::ColorEdit3("Color##Albedo", glm::value_ptr(albedoColor), ImGuiColorEditFlags_NoInputs);
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;

			float& emissive = material->GetFloat("u_MaterialUniforms.Emission");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100.0f);
			ImGui::DragFloat("Emission", &emissive, 0.1f, 0.0f, 20.0f);
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;

			ImGui::SetCursorPos(nextRowCursorPos);
		}

		if (transparent)
		{
			float& transparency = material->GetFloat("u_MaterialUniforms.Transparency");

			UI::BeginPropertyGrid();
			UI::Property("Transparency", transparency, 0.01f, 0.0f, 1.0f);
			UI::EndPropertyGrid();
		}
		else
		{
			// Textures ------------------------------------------------------------------------------
			{
				// Normals
				if (ImGui::CollapsingHeader("Normals", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
					bool useNormalMap = material->GetFloat("u_MaterialUniforms.UseNormalMap");
					Ref<VulkanTexture2D> normalMap = material->TryGetTexture2D("u_NormalTexture");

					bool hasNormalMap = normalMap ? (normalMap.get() != Renderer::GetWhiteTexture().get()) && normalMap->GetImage() : false;
					ImVec2 textureCursorPos = ImGui::GetCursorPos();
					UI::Image(hasNormalMap ? normalMap : EditorResources::CheckerboardTexture, ImVec2(64, 64));

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
								needsSerialize = true;
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
							std::string filepath = normalMap->GetPath().string();
							ImGui::TextUnformatted(filepath.c_str());
							ImGui::PopTextWrapPos();
							UI::Image(normalMap, ImVec2(384, 384));
							ImGui::EndTooltip();
						}
						if (ImGui::IsItemClicked())
						{
						}
					}
					ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
					ImGui::SameLine();
					ImVec2 properCursorPos = ImGui::GetCursorPos();
					ImGui::SetCursorPos(textureCursorPos);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
					if (hasNormalMap && ImGui::Button("X##NormalMap", ImVec2(18, 18)))
					{
						m_MaterialAsset->ClearNormalMap();
						needsSerialize = true;
					}
					ImGui::PopStyleVar();
					ImGui::SetCursorPos(properCursorPos);

					if (ImGui::Checkbox("Use##NormalMap", &useNormalMap))
						material->Set("u_MaterialUniforms.UseNormalMap", useNormalMap);

					if (ImGui::IsItemDeactivated())
						needsSerialize = true;

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
					bool hasMetallicRoughnessMap = metallicRoughnessMap ? (metallicRoughnessMap.get() != Renderer::GetWhiteTexture().get()) && metallicRoughnessMap->GetImage() : false;
					ImVec2 textureCursorPos = ImGui::GetCursorPos();
					UI::Image(hasMetallicRoughnessMap ? metallicRoughnessMap : EditorResources::CheckerboardTexture, ImVec2(64, 64));

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
								needsSerialize = true;
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
							std::string filepath = metallicRoughnessMap->GetPath().string();
							ImGui::TextUnformatted(filepath.c_str());
							ImGui::PopTextWrapPos();
							UI::Image(metallicRoughnessMap, ImVec2(384, 384));
							ImGui::EndTooltip();
						}
						if (ImGui::IsItemClicked())
						{

						}
					}
					ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
					ImGui::SameLine();
					ImVec2 properCursorPos = ImGui::GetCursorPos();
					ImGui::SetCursorPos(textureCursorPos);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
					if (hasMetallicRoughnessMap && ImGui::Button("X##MetallicRoughnessMap", ImVec2(18, 18)))
					{
						m_MaterialAsset->ClearMetallicRoughnessMap();
						needsSerialize = true;
					}
					ImGui::PopStyleVar();
					ImGui::SetCursorPos(properCursorPos);
					ImGui::SetNextItemWidth(200.0f);
					ImGui::SliderFloat("Roughness Value##RoughnessInput", &roughnessValue, 0.0f, 1.0f);
					ImGui::SetNextItemWidth(200.0f);
					ImGui::SliderFloat("Roughness Value##MetallicInput", &metallicValue, 0.0f, 1.0f);
					if (ImGui::IsItemDeactivated())
						needsSerialize = true;
					ImGui::SetCursorPos(nextRowCursorPos);
				}
			}

			UI::BeginPropertyGrid();

			bool castsShadows = m_MaterialAsset->IsShadowCasting();
			if (UI::Property("Casts shadows", castsShadows))
				m_MaterialAsset->SetShadowCasting(castsShadows);

			UI::EndPropertyGrid();
		}

		if (needsSerialize)
			AssetImporter::Serialize(m_MaterialAsset.get());
	}

	TextureViewer::TextureViewer()
		: AssetEditor("Edit Texture")
	{
		SetMinSize(200, 600);
		SetMaxSize(500, 1000);
	}

	void TextureViewer::OnOpen()
	{
		if (!m_Asset)
			SetOpen(false);
	}

	void TextureViewer::OnClose()
	{
		m_Asset = nullptr;
	}

	void TextureViewer::Render()
	{
		float textureWidth = (float)m_Asset->GetWidth();
		float textureHeight = (float)m_Asset->GetHeight();
		//float bitsPerPixel = Texture::GetBPP(m_Asset->GetFormat());
		float imageSize = ImGui::GetWindowWidth() - 40;
		imageSize = glm::min(imageSize, 500.0f);

		ImGui::SetCursorPosX(20);
		//ImGui::Image((ImTextureID)m_Asset->GetRendererID(), { imageSize, imageSize });

		UI::BeginPropertyGrid();
		UI::BeginDisabled();
		UI::Property("Width", textureWidth);
		UI::Property("Height", textureHeight);
		// UI::Property("Bits", bitsPerPixel, 0.1f, 0.0f, 0.0f, true); // TODO: Format
		UI::EndDisabled();
		UI::EndPropertyGrid();
	}



	PrefabEditor::PrefabEditor()
		: AssetEditor("Prefab Editor"), m_SceneHierarchyPanel(nullptr, SelectionContext::PrefabEditor, false)
	{
	}

	void PrefabEditor::OnOpen()
	{
		SelectionManager::DeselectAll();
	}

	void PrefabEditor::OnClose()
	{
		SelectionManager::DeselectAll(SelectionContext::PrefabEditor);
	}

	void PrefabEditor::Render()
	{
		// Need to do this in order to ensure that the scene hierarchy panel doesn't close immediately.
		// There's been some structural changes since the addition of the PanelManager.
		bool isOpen = true;
		m_SceneHierarchyPanel.OnImGuiRender(isOpen);
	}


}
