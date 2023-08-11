#pragma once

#include "X2/Editor/EditorPanel.h"
#include "X2/Scene/Entity.h"
#include "X2/Vulkan/VulkanTexture.h"

namespace X2 {

	class MaterialsPanel : public EditorPanel
	{
	public:
		MaterialsPanel();
		~MaterialsPanel();

		virtual void SetSceneContext(const Ref<Scene>& context) override;
		virtual void OnImGuiRender(bool& isOpen) override;

	private:
		void RenderMaterial(size_t materialIndex, AssetHandle materialAssetHandle);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
		Ref<VulkanTexture2D> m_CheckerBoardTexture;
	};

}
