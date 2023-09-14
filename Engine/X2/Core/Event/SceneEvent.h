#pragma once

#include "Event.h"
#include "X2/Scene/Scene.h"
#include "X2/Scene/Entity.h"

#include "X2/Editor/SelectionManager.h"

#include <sstream>

namespace X2 {

	class SceneEvent : public Event
	{
	public:
		const Scene* GetScene() const { return m_Scene; }
		Scene* GetScene() { return m_Scene; }

		EVENT_CLASS_CATEGORY(EventCategoryApplication | EventCategoryScene)
	protected:
		SceneEvent(Scene* scene)
			: m_Scene(scene) {}

		Scene* m_Scene;
	};

	class ScenePreStartEvent : public SceneEvent
	{
	public:
		ScenePreStartEvent(Scene* scene)
			: SceneEvent(scene) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "ScenePreStartEvent: " << m_Scene->GetName();
			return ss.str();
		}

		EVENT_CLASS_TYPE(ScenePreStart)
	};

	class ScenePostStartEvent : public SceneEvent
	{
	public:
		ScenePostStartEvent(Scene* scene)
			: SceneEvent(scene) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "ScenePostStartEvent: " << m_Scene->GetName();
			return ss.str();
		}

		EVENT_CLASS_TYPE(ScenePostStart)
	};

	class ScenePreStopEvent : public SceneEvent
	{
	public:
		ScenePreStopEvent(Scene* scene)
			: SceneEvent(scene) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "ScenePreStopEvent: " << m_Scene->GetName();
			return ss.str();
		}

		EVENT_CLASS_TYPE(ScenePreStop)
	};

	class ScenePostStopEvent : public SceneEvent
	{
	public:
		ScenePostStopEvent(Scene* scene)
			: SceneEvent(scene) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "ScenePostStopEvent: " << m_Scene->GetName();
			return ss.str();
		}

		EVENT_CLASS_TYPE(ScenePostStop)
	};

	// TODO(Peter): Probably move this somewhere else...
	class SelectionChangedEvent : public Event
	{
	public:
		SelectionChangedEvent(SelectionContext contextID, UUID selectionID, bool selected)
			: m_Context(contextID), m_SelectionID(selectionID), m_Selected(selected)
		{
		}

		SelectionContext GetContextID() const { return m_Context; }
		UUID GetSelectionID() const { return m_SelectionID; }
		bool IsSelected() const { return m_Selected; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "EntitySelectionChangedEvent: Context(" << (int32_t)m_Context << "), Selection(" << m_SelectionID << "), " << m_Selected;
			return ss.str();
		}

		EVENT_CLASS_CATEGORY(EventCategoryScene)
			EVENT_CLASS_TYPE(SelectionChanged)
	private:
		SelectionContext m_Context;
		UUID m_SelectionID;
		bool m_Selected;
	};

}
