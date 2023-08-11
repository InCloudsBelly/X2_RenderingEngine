#pragma once

#include <glm/glm.hpp>

#include "Scene.h"

#include "Components.h"

namespace X2 {

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_EntityHandle(handle), m_Scene(scene) {}

		~Entity() {}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			X2_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
			return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent()
		{
			X2_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		const T& GetComponent() const
		{
			X2_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename... T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.has<T...>(m_EntityHandle);
		}

		template<typename... T>
		bool HasComponent() const
		{
			return m_Scene->m_Registry.has<T...>(m_EntityHandle);
		}

		template<typename...T>
		bool HasAny()
		{
			return m_Scene->m_Registry.any<T...>(m_EntityHandle);
		}

		template<typename...T>
		bool HasAny() const
		{
			return m_Scene->m_Registry.any<T...>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent()
		{
			X2_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponentIfExists()
		{
			m_Scene->m_Registry.remove_if_exists<T>(m_EntityHandle);
		}

		TransformComponent& Transform() { return m_Scene->m_Registry.get<TransformComponent>(m_EntityHandle); }
		const glm::mat4& Transform() const { return m_Scene->m_Registry.get<TransformComponent>(m_EntityHandle).GetTransform(); }

		std::string& Name() { return HasComponent<TagComponent>() ? GetComponent<TagComponent>().Tag : NoName; }
		const std::string& Name() const { return HasComponent<TagComponent>() ? GetComponent<TagComponent>().Tag : NoName; }

		operator uint32_t () const { return (uint32_t)m_EntityHandle; }
		operator entt::entity() const { return m_EntityHandle; }
		operator bool() const { return (m_EntityHandle != entt::null) && m_Scene && m_Scene->m_Registry.valid(m_EntityHandle); }

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}

		Entity GetParent() const
		{
			return m_Scene->TryGetEntityWithUUID(GetParentUUID());
		}

		void SetParent(Entity parent)
		{
			Entity currentParent = GetParent();
			if (currentParent == parent)
				return;

			// If changing parent, remove child from existing parent
			if (currentParent)
				currentParent.RemoveChild(*this);

			// Setting to null is okay
			SetParentUUID(parent.GetUUID());

			if (parent)
			{
				auto& parentChildren = parent.Children();
				UUID uuid = GetUUID();
				if (std::find(parentChildren.begin(), parentChildren.end(), uuid) == parentChildren.end())
					parentChildren.emplace_back(GetUUID());
			}
		}

		void SetParentUUID(UUID parent) { GetComponent<RelationshipComponent>().ParentHandle = parent; }
		UUID GetParentUUID() const { return GetComponent<RelationshipComponent>().ParentHandle; }
		std::vector<UUID>& Children() { return GetComponent<RelationshipComponent>().Children; }
		const std::vector<UUID>& Children() const { return GetComponent<RelationshipComponent>().Children; }

		bool RemoveChild(Entity child)
		{
			UUID childId = child.GetUUID();
			std::vector<UUID>& children = Children();
			auto it = std::find(children.begin(), children.end(), childId);
			if (it != children.end())
			{
				children.erase(it);
				return true;
			}

			return false;
		}

		bool IsAncesterOf(Entity entity) const
		{
			const auto& children = Children();

			if (children.empty())
				return false;

			for (UUID child : children)
			{
				if (child == entity.GetUUID())
					return true;
			}

			for (UUID child : children)
			{
				if (m_Scene->GetEntityWithUUID(child).IsAncesterOf(entity))
					return true;
			}

			return false;
		}

		bool IsDescendantOf(Entity entity) const { return entity.IsAncesterOf(*this); }

		UUID GetUUID() const { return GetComponent<IDComponent>().ID; }
		UUID GetSceneUUID() const { return m_Scene->GetUUID(); }
	private:
		Entity(const std::string& name);
	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr;

		inline static std::string NoName = "Unnamed";

		friend class Prefab;
		friend class Scene;
		friend class SceneSerializer;
		friend class ScriptEngine;
	};

}
