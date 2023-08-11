#pragma once

#include "X2/Core/Ref.h"
#include "X2/Scene/Scene.h"
#include "X2/Project/Project.h"
#include "X2/Core/Event/Event.h"

namespace X2 {

	class EditorPanel : public RefCounted
	{
	public:
		virtual ~EditorPanel() = default;

		virtual void OnImGuiRender(bool& isOpen) = 0;
		virtual void OnEvent(Event& e) {}
		virtual void OnProjectChanged(const Ref<Project>& project) {}
		virtual void SetSceneContext(const Ref<Scene>& context) {}
	};

}
