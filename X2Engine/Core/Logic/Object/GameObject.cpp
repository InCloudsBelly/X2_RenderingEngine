#include "GameObject.h"
#include "Core/Logic/Component/Component.h"
#include <rttr/registration>
#include "Utils/ChildBrotherTree.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Logic/CoreObject/LogicInstance.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	registration::class_<GameObject>("GameObject");
}

GameObject::GameObject(std::string name)
	: Object()
	, name(name)
	, m_timeSqueueComponentsHead()
	, m_typeSqueueComponentsHeadMap()
	, transform()
{
	transform.m_gameObject = this;
}

GameObject::GameObject()
	: GameObject("New GameObject")
{
}

GameObject::~GameObject()
{

}

void GameObject::addComponent(Component* targetComponent)
{
	m_timeSqueueComponentsHead.Add(targetComponent);
	if (!m_typeSqueueComponentsHeadMap.count(targetComponent->m_type))
	{
		m_typeSqueueComponentsHeadMap[targetComponent->m_type] = std::unique_ptr<CrossLinkableRowHead>(new CrossLinkableRowHead());
	}
	m_typeSqueueComponentsHeadMap[targetComponent->m_type]->Add(targetComponent);
	targetComponent->m_gameObject = this;
}

void GameObject::removeComponent(Component* targetComponent)
{
	if (targetComponent->m_gameObject != this)
	{
		throw(std::runtime_error("Component do not blong to this GameObject."));
	}

	m_timeSqueueComponentsHead.Remove(targetComponent);
	m_typeSqueueComponentsHeadMap[targetComponent->m_type]->Remove(targetComponent);
	targetComponent->m_gameObject = nullptr;

	if (!m_typeSqueueComponentsHeadMap[targetComponent->m_type]->HaveNode())
	{
		m_typeSqueueComponentsHeadMap.erase(targetComponent->m_type);
	}

	if (LogicInstance::g_validComponentInIteration.count(targetComponent))
	{
		LogicInstance::g_validComponentInIteration.erase(targetComponent);
	}
}

Component* GameObject::removeComponent(std::string targetTypeName)
{
	return removeComponent(rttr::type::get_by_name(targetTypeName));
}

Component* GameObject::removeComponent(rttr::type targetType)
{
	if (!targetType)
	{
		throw(std::runtime_error("Do not have " + targetType.get_name().to_string() + "."));
	}

	if (!targetType.is_derived_from(Component::COMPONENT_TYPE))
	{
		throw(std::runtime_error(targetType.get_name().to_string() + " is not a component."));
	}

	for (const auto& pair : Component::TYPE_MAP)
	{
		if ((targetType == pair.first || targetType.is_base_of(pair.first)) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			Component* found = static_cast<Component*>(m_typeSqueueComponentsHeadMap[pair.second]->GetIterator().Node());
			removeComponent(found);
			return found;
		}
		else if (pair.first.is_base_of(targetType) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			for (auto iterator = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator(); iterator.IsValid(); iterator++)
			{
				Component* found = static_cast<Component*>(iterator.Node());
				if (targetType.is_base_of(found->type()))
				{
					removeComponent(found);
					return found;
				}
			}
		}
	}

	throw(std::runtime_error("GameObject " + name + " do not have a " + targetType.get_name().to_string() + " Component."));
}

std::vector<Component*> GameObject::removeComponents(std::string targetTypeName)
{
	return removeComponents(rttr::type::get_by_name(targetTypeName));
}

std::vector<Component*> GameObject::removeComponents(rttr::type targetType)
{
	if (!targetType)
	{
		throw(std::runtime_error("Do not have " + targetType.get_name().to_string() + "."));
	}

	if (!targetType.is_derived_from(Component::COMPONENT_TYPE))
	{
		throw(std::runtime_error(targetType.get_name().to_string() + " is not a component."));
	}

	auto targetComponents = std::vector<Component*>();
	for (const auto& pair : Component::TYPE_MAP)
	{
		if ((targetType == pair.first || targetType.is_base_of(pair.first)) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			for (auto itertor = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator(); itertor.IsValid(); )
			{
				auto foundComponent = static_cast<Component*>(itertor.Node());

				m_timeSqueueComponentsHead.Remove(foundComponent);
				itertor = m_typeSqueueComponentsHeadMap[pair.second]->Remove(itertor);
				foundComponent->m_gameObject = nullptr;

				targetComponents.emplace_back(foundComponent);
			}
			if (!m_typeSqueueComponentsHeadMap[pair.second]->HaveNode())
			{
				m_typeSqueueComponentsHeadMap.erase(pair.second);
			}
		}
		else if (pair.first.is_base_of(targetType) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			for (auto iterator = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator(); iterator.IsValid(); )
			{
				Component* found = static_cast<Component*>(iterator.Node());
				if (targetType.is_base_of(found->type()))
				{
					m_timeSqueueComponentsHead.Remove(found);
					iterator = m_typeSqueueComponentsHeadMap[pair.second]->Remove(iterator);
					found->m_gameObject = nullptr;

					targetComponents.emplace_back(found);
				}
			}
			if (!m_typeSqueueComponentsHeadMap[pair.second]->HaveNode())
			{
				m_typeSqueueComponentsHeadMap.erase(pair.second);
			}
		}
	}

	return targetComponents;
}

Component* GameObject::getComponent(rttr::type targetType)
{
	if (!targetType)
	{
		throw(std::runtime_error("Do not have " + targetType.get_name().to_string() + "."));
	}

	if (!Component::COMPONENT_TYPE.is_base_of(targetType))
	{
		throw(std::runtime_error(targetType.get_name().to_string() + " is not a component."));
	}

	for (const auto& pair : Component::TYPE_MAP)
	{
		if ((targetType == pair.first || targetType.is_base_of(pair.first)) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			auto node = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator().Node();
			Component* found = static_cast<Component*>(node);
			return found;
		}
		else if (pair.first.is_base_of(targetType) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			for (auto iterator = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator(); iterator.IsValid(); iterator++)
			{
				Component* found = static_cast<Component*>(iterator.Node());
				if (targetType.is_base_of(found->type())) return found;
			}
		}
	}

	throw(std::runtime_error("GameObject " + name + " do not have a " + targetType.get_name().to_string() + " Component."));
}

Component* GameObject::getComponent(std::string targetTypeName)
{
	return getComponent(rttr::type::get_by_name(targetTypeName));
}

std::vector<Component*> GameObject::getComponents(rttr::type targetType)
{
	if (!targetType)
	{
		throw(std::runtime_error("Do not have " + targetType.get_name().to_string() + "."));
	}

	if (!targetType.is_derived_from(Component::COMPONENT_TYPE))
	{
		throw(std::runtime_error(targetType.get_name().to_string() + " is not a component."));
	}

	auto targetComponents = std::vector<Component*>();

	for (const auto& pair : Component::TYPE_MAP)
	{
		if ((targetType == pair.first || targetType.is_base_of(pair.first)) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			for (auto itertor = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator(); itertor.IsValid(); )
			{
				auto foundComponent = static_cast<Component*>(itertor.Node());

				targetComponents.emplace_back(foundComponent);
			}
		}
		else if (pair.first.is_base_of(targetType) && m_typeSqueueComponentsHeadMap.count(pair.second))
		{
			for (auto iterator = m_typeSqueueComponentsHeadMap[pair.second]->GetIterator(); iterator.IsValid(); iterator++)
			{
				Component* found = static_cast<Component*>(iterator.Node());
				if (targetType.is_base_of(found->type()))
				{
					targetComponents.emplace_back(found);
				}
			}
		}
	}

	return targetComponents;
}

std::vector<Component*> GameObject::getComponents(std::string targetTypeName)
{
	return getComponents(rttr::type::get_by_name(targetTypeName));
}

GameObject* GameObject::getParent()
{
	auto targetTransform = transform.Parent();
	return targetTransform ? targetTransform->getGameObject() : nullptr;
}

GameObject* GameObject::getChild()
{
	auto targetTransform = transform.Child();
	return targetTransform ? targetTransform->getGameObject() : nullptr;
}

GameObject* GameObject::getBrother()
{
	auto targetTransform = transform.Brother();
	return targetTransform ? targetTransform->getGameObject() : nullptr;
}

void GameObject::addChild(GameObject* child)
{
	transform.AddChild(&child->transform);
}

void GameObject::removeChild(GameObject* child)
{
	if (child->getParent() == this)
	{
		child->removeSelf();
	}
}

void GameObject::removeSelf()
{
	static_cast<ChildBrotherTree<Transform>*>(&transform)->RemoveSelf();
	if (LogicInstance::g_validGameObjectInIteration.count(this))
	{
		LogicInstance::g_validGameObjectInIteration.erase(this);
	}
}