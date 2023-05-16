#include "Component.h"
#include "Core/Logic/Object/GameObject.h"
#include <rttr/registration>
#include <iostream>
#include "Core/Logic/Component/Base/Transform.h"
#include "Core/Logic/Component/Base/Renderer.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Core/Logic/Component/Base/Behaviour.h"


RTTR_REGISTRATION
{
	rttr::registration::class_<Component>("Component");
}

const std::map<rttr::type, Component::ComponentType> Component::TYPE_MAP =
std::map<rttr::type, Component::ComponentType>
({
		{rttr::type::get<Transform>(), Component::ComponentType::TRANSFORM},
		{rttr::type::get<CameraBase>(), Component::ComponentType::CAMERA},
		{rttr::type::get<Renderer>(), Component::ComponentType::RENDERER},
		{rttr::type::get<Behaviour>(), Component::ComponentType::BEHAVIOUR}
	});

const rttr::type Component::COMPONENT_TYPE = rttr::type::get< Component>();

Component::Component()
	: Component(ComponentType::NONE)
{
}

Component::Component(ComponentType type)
	: m_type(type)
	, Object()
	, CrossLinkableNode()
	, m_gameObject(nullptr)
	, m_active(true)
	, m_neverStarted(true)
	, m_neverAwaked(true)
{
}

Component::~Component()
{
}

void Component::awake()
{
	m_neverStarted = true;
	if (m_neverAwaked)
	{
		m_neverAwaked = false;
		onAwake();
	}
}

void Component::update()
{
	if (m_neverStarted)
	{
		m_neverStarted = false;
		onStart();
	}
	onUpdate();
}

void Component::onAwake()
{
}

void Component::onStart()
{
}

void Component::onUpdate()
{
}

void Component::onDestroy()
{
}

bool Component::isActive()
{
	return m_active;
}

void Component::setActive(bool active)
{
	m_active = active;
}

GameObject* Component::getGameObject()
{
	return m_gameObject;
}

Component::ComponentType Component::getComponentType()
{
	return m_type;
}
