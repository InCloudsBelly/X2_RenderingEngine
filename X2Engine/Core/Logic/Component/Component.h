#pragma once

#include "Core/Logic/Object/Object.h"
#include "Utils/CrossLinkableNode.h"
#include <map>

class Component
	: public Object
	, private CrossLinkableNode
{
	friend class GameObject;
	friend class Engine;
	friend class Instance;
public:
	enum class ComponentType
	{
		NONE,
		TRANSFORM,
		CAMERA,
		RENDERER,
		LIGHT,
		BEHAVIOUR,
	};
private:
	bool m_active;
	bool m_neverStarted;
	bool m_neverAwaked;
	Component(const Component&) = delete;
	Component& operator=(const Component&) = delete;
	Component(Component&&) = delete;
	Component& operator=(Component&&) = delete;
protected:
	const static std::map<rttr::type, ComponentType> TYPE_MAP;
	const static rttr::type COMPONENT_TYPE;
	ComponentType m_type;
	GameObject* m_gameObject;
	Component();
	Component(ComponentType type);
	virtual ~Component();
	void awake();
	void update();
	virtual void onAwake();
	virtual void onStart();
	virtual void onUpdate();
	virtual void onDestroy();
public:
	bool isActive();
	void setActive(bool active);
	GameObject* getGameObject();
	ComponentType getComponentType();

	RTTR_ENABLE(Object)
};