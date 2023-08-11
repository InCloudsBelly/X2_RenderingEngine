#pragma once

#include "AssetEditorPanel.h"
#include "X2/Renderer/Mesh.h"

#include "X2/Scene/Prefab.h"
#include "X2/Editor/SceneHierarchyPanel.h"


namespace X2 {


	class MaterialEditor : public AssetEditor
	{
	public:
		MaterialEditor();

		virtual void SetAsset(const Ref<Asset>& asset) override { m_MaterialAsset = (Ref<MaterialAsset>)asset; }

	private:
		virtual void OnOpen() override;
		virtual void OnClose() override;
		virtual void Render() override;

	private:
		Ref<MaterialAsset> m_MaterialAsset;
	};

	class PrefabEditor : public AssetEditor
	{
	public:
		PrefabEditor();

		virtual void SetAsset(const Ref<Asset>& asset) override { m_Prefab = (Ref<Prefab>)asset; m_SceneHierarchyPanel.SetSceneContext(m_Prefab->m_Scene); }

	private:
		virtual void OnOpen() override;
		virtual void OnClose() override;
		virtual void Render() override;

	private:
		Ref<Prefab> m_Prefab;
		SceneHierarchyPanel m_SceneHierarchyPanel;
	};

	class TextureViewer : public AssetEditor
	{
	public:
		TextureViewer();

		virtual void SetAsset(const Ref<Asset>& asset) override { m_Asset = (Ref<Texture>)asset; }

	private:
		virtual void OnOpen() override;
		virtual void OnClose() override;
		virtual void Render() override;

	private:
		Ref<Texture> m_Asset;
	};

}
