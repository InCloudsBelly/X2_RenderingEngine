#pragma once

#include "X2/Core/Log.h"
#include "X2/Project/Project.h"
#include "X2/Scene/Scene.h"

#include "X2/Editor/EditorPanel.h"

namespace X2 {

	class ProjectSettingsWindow : public EditorPanel
	{
	public:
		ProjectSettingsWindow();
		~ProjectSettingsWindow();

		virtual void OnImGuiRender(bool& isOpen) override;
		virtual void OnProjectChanged(const Ref<Project>& project) override;

	private:
		void RenderGeneralSettings();
		void RenderRendererSettings();
		//void RenderAudioSettings();
		//void RenderScriptingSettings();
		//void RenderPhysicsSettings();
		void RenderLogSettings();
	private:
		Ref<Project> m_Project;
		AssetHandle m_DefaultScene;
		int32_t m_SelectedLayer = -1;
		char m_NewLayerNameBuffer[255];

	};

}
