#pragma once

#include "X2/Editor/EditorPanel.h"
#include "X2/Renderer/SceneRenderer.h"

namespace X2 {

	class SceneRendererPanel : public EditorPanel
	{
	public:
		SceneRendererPanel() = default;
		virtual ~SceneRendererPanel() = default;

		void SetContext(const Ref<SceneRenderer>& context) { m_Context = context; }
		virtual void OnImGuiRender(bool& isOpen) override;
	private:
		Ref<SceneRenderer> m_Context;
	};

}
