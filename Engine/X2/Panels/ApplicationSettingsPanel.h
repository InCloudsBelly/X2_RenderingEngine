#pragma once

#include "X2/Editor/EditorPanel.h"
#include "X2/Vulkan/VulkanTexture.h"

#include <functional>

namespace X2 {

	struct SettingsPage
	{
		using PageRenderFunction = std::function<void()>;

		const char* Name;
		PageRenderFunction RenderFunction;
	};

	class ApplicationSettingsPanel : public EditorPanel
	{
	public:
		ApplicationSettingsPanel();
		~ApplicationSettingsPanel();

		virtual void OnImGuiRender(bool& isOpen) override;

	private:
		void DrawPageList();

		void DrawRendererPage();
		//void DrawScriptingPage();
		void DrawX2Page();
	private:
		uint32_t m_CurrentPage = 0;
		std::vector<SettingsPage> m_Pages;
	};

}
