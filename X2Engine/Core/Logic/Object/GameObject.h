#pragma once
#include "Core/Logic/Object/Object.h"
#include "Utils/CrossLinkableNode.h"
#include "Core/Logic/Component/Component.h"
#include "Core/Logic/Component/Base/Transform.h"

class Component;
class GameObject final : public Object
{
	friend class Engine;
private:
	CrossLinkableColHead m_timeSqueueComponentsHead;
	std::map<Component::ComponentType, std::unique_ptr< CrossLinkableRowHead>> m_typeSqueueComponentsHeadMap;


	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;
	GameObject(GameObject&&) = delete;
	GameObject& operator=(GameObject&&) = delete;
public:
	std::string name;
	Transform transform;

	GameObject(std::string name);
	GameObject();
	virtual ~GameObject();

	//Add
	void addComponent(Component* component);
	//Remove
	void removeComponent(Component* component);
	Component* removeComponent(std::string typeName);
	Component* removeComponent(rttr::type targetType);
	template<typename TType>
	TType* removeComponent();
	std::vector<Component*> removeComponents(std::string typeName);
	std::vector<Component*> removeComponents(rttr::type targetType);
	template<typename TType>
	std::vector <TType*> removeComponents();
	//Get
	Component* getComponent(rttr::type targetType);
	Component* getComponent(std::string targetTypeName);
	template<typename TType>
	TType* getComponent();
	std::vector<Component*> getComponents(rttr::type targetType);
	std::vector <Component*> getComponents(std::string targetTypeName);
	GameObject* getParent();
	GameObject* getChild();
	GameObject* getBrother();
	template<typename TType>
	std::vector <TType*> getComponents();

	void addChild(GameObject* child);
	void removeChild(GameObject* child);
	void removeSelf();

	RTTR_ENABLE(Object)
};
template<typename TType>
inline TType* GameObject::removeComponent()
{
	return dynamic_cast<TType*>(removeComponent(rttr::type::get<TType>()));
}
template<typename TType>
inline std::vector<TType*> GameObject::removeComponents()
{
	auto foundComponents = removeComponents(rttr::type::get<TType>());
	auto targetComponents = std::vector<TType*>(foundComponents.size());

	for (uint32_t i = 0; i < targetComponents.size(); i++)
	{
		targetComponents[i] = dynamic_cast<TType*>(foundComponents[i]);
	}

	return targetComponents;
}
template<typename TType>
inline TType* GameObject::getComponent()
{
	return dynamic_cast<TType*>(getComponent(rttr::type::get<TType>()));
}
template<typename TType>
inline std::vector<TType*> GameObject::getComponents()
{
	auto foundComponents = getComponents(rttr::type::get<TType>());
	auto targetComponents = std::vector<TType*>(foundComponents.size());

	for (uint32_t i = 0; i < targetComponents.size(); i++)
	{
		targetComponents[i] = dynamic_cast<TType*>(foundComponents[i]);
	}

	return targetComponents;
}