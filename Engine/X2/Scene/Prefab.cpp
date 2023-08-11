#include "Precompiled.h"
#include "Prefab.h"

#include "Scene.h"
//#include "Hazel/Audio/AudioComponent.h"

#include "X2/Asset/AssetImporter.h"
#include "X2/Asset/AssetManager.h"

//#include "X2/Script/ScriptEngine.h"

namespace X2 {

	Entity Prefab::CreatePrefabFromEntity(Entity entity)
	{
		X2_CORE_ASSERT(Handle);

		Entity newEntity = m_Scene->CreateEntity();

		// Add PrefabComponent
		newEntity.AddComponent<PrefabComponent>(Handle, newEntity.GetComponent<IDComponent>().ID);

		entity.m_Scene->CopyComponentIfExists<TagComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<TransformComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<MeshComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<StaticMeshComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<AnimationComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<DirectionalLightComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<SpotLightComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<PointLightComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<SkyLightComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<ScriptComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<CameraComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<SpriteRendererComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<TextComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<RigidBody2DComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<BoxCollider2DComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<CircleCollider2DComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<RigidBodyComponent>(newEntity, m_Scene->m_Registry, entity);
		entity.m_Scene->CopyComponentIfExists<CharacterControllerComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<FixedJointComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<BoxColliderComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<SphereColliderComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<CapsuleColliderComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<MeshColliderComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<AudioComponent>(newEntity, m_Scene->m_Registry, entity);
		//entity.m_Scene->CopyComponentIfExists<AudioListenerComponent>(newEntity, m_Scene->m_Registry, entity);

#if _DEBUG && 0
		// Check that nothing has been forgotten...
		bool foundAll = true;
		entity.m_Scene->m_Registry.visit(entity, [&](entt::id_type type) {
			if (type != entt::type_index<RelationshipComponent>().value())
			{
				bool foundOne = false;
				m_Scene->m_Registry.visit(newEntity, [type, &foundOne](entt::id_type newType) {if (newType == type) foundOne = true; });
				foundAll = foundAll && foundOne;
			}
			});
		X2_CORE_ASSERT(foundAll, "At least one component was not duplicated - have you added a new component type and not dealt with it here?");
#endif

		for (auto childId : entity.Children())
		{
			Entity childDuplicate = CreatePrefabFromEntity(entity.m_Scene->GetEntityWithUUID(childId));

			childDuplicate.SetParentUUID(newEntity.GetUUID());
			newEntity.Children().push_back(childDuplicate.GetUUID());
		}

	/*	if (newEntity.HasComponent<ScriptComponent>())
			ScriptEngine::DuplicateScriptInstance(entity, newEntity);*/

		return newEntity;
	}

	Prefab::Prefab()
	{
		m_Scene = Scene::CreateEmpty();
	}

	Prefab::~Prefab()
	{
	}

	void Prefab::Create(Entity entity, bool serialize)
	{
		// Create new scene
		m_Scene = Scene::CreateEmpty();
		m_Entity = CreatePrefabFromEntity(entity);

		// Need to rebuild mapping of meshes -> bones because the prefab entity
		// will have newly generated UUIDs for all the bone entities.
		// This has to be done after the entire hierarchy of entities in the prefab
		// has been created.
		
		//m_Scene->BuildBoneEntityIds(m_Entity);

		if (serialize)
			AssetImporter::Serialize(this);
	}

	std::unordered_set<AssetHandle> Prefab::GetAssetList(bool recursive)
	{
		std::unordered_set<AssetHandle> prefabAssetList = m_Scene->GetAssetList();
		if (!recursive)
			return prefabAssetList;

		for (AssetHandle handle : prefabAssetList)
		{
			if (!AssetManager::IsAssetHandleValid(handle))
				continue;

			const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(handle);
			if (metadata.Type == AssetType::Prefab)
			{
				Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(handle);
				std::unordered_set<AssetHandle> childPrefabAssetList = prefab->GetAssetList(true);
				prefabAssetList.insert(childPrefabAssetList.begin(), childPrefabAssetList.end());
			}
		}

		return prefabAssetList;
	}

}
