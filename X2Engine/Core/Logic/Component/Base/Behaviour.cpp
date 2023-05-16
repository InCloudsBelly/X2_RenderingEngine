#include "Core/Logic/Component/Base/Behaviour.h"
RTTR_REGISTRATION
{
	rttr::registration::class_<Behaviour>("Behaviour");
}

Behaviour::Behaviour()
	: Component(ComponentType::BEHAVIOUR)
{
}

Behaviour::~Behaviour()
{
}

void Behaviour::onAwake()
{
}

void Behaviour::onStart()
{
}

void Behaviour::onUpdate()
{
}

void Behaviour::onDestroy()
{
}
