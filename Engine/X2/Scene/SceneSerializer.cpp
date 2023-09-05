#include "Precompiled.h"
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"

//#include "X2/Animation/AnimationController.h"
//#include "X2/Animation/Skeleton.h"

#include "X2/Asset/AssetManager.h"

//#include "X2/Audio/AudioComponent.h"
//#include "X2/Audio/AudioEngine.h"

//#include "X2/Renderer/MeshFactory.h"

//#include "X2/Physics/3D/CookingFactory.h"
//#include "X2/Physics/3D/PhysicsLayer.h"
//#include "X2/Physics/3D/PhysicsSystem.h"

//#include "X2/Renderer/UI/Font.h"

//#include "X2/Script/ScriptEngine.h"
//#include "X2/Script/ScriptUtils.h"

#include "X2/Utilities/YAMLSerializationHelpers.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

//#include <mono/metadata/blob.h>

#include "yaml-cpp/yaml.h"

#include <filesystem>
#include <fstream>

namespace X2 {

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		UUID uuid = entity.GetComponent<IDComponent>().ID;
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity";
		out << YAML::Value << uuid;

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<RelationshipComponent>())
		{
			auto& relationshipComponent = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "Parent" << YAML::Value << relationshipComponent.ParentHandle;

			out << YAML::Key << "Children";
			out << YAML::Value << YAML::BeginSeq;

			for (auto child : relationshipComponent.Children)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Handle" << YAML::Value << child;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}

		if (entity.HasComponent<PrefabComponent>())
		{
			out << YAML::Key << "PrefabComponent";
			out << YAML::BeginMap; // PrefabComponent

			auto& prefabComponent = entity.GetComponent<PrefabComponent>();
			out << YAML::Key << "Prefab" << YAML::Value << prefabComponent.PrefabID;
			out << YAML::Key << "Entity" << YAML::Value << prefabComponent.EntityID;

			out << YAML::EndMap; // PrefabComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& transform = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Position" << YAML::Value << transform.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << transform.RotationEuler;
			out << YAML::Key << "Scale" << YAML::Value << transform.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		//if (entity.HasComponent<ScriptComponent>())
		//{
		//	out << YAML::Key << "ScriptComponent";
		//	out << YAML::BeginMap; // ScriptComponent

		//	const auto& sc = entity.GetComponent<ScriptComponent>();

		//	ManagedClass* scriptClass = ScriptCache::GetManagedClassByID(ScriptEngine::GetScriptClassIDFromComponent(sc));
		//	out << YAML::Key << "ClassHandle" << YAML::Value << sc.ScriptClassHandle;
		//	out << YAML::Key << "Name" << YAML::Value << (scriptClass ? scriptClass->FullName : "Null");

		//	if (sc.FieldIDs.size() > 0)
		//	{
		//		out << YAML::Key << "StoredFields" << YAML::Value;
		//		out << YAML::BeginSeq;

		//		for (auto fieldID : sc.FieldIDs)
		//		{
		//			FieldInfo* fieldInfo = ScriptCache::GetFieldByID(fieldID);

		//			if (!fieldInfo->IsWritable())
		//				continue;

		//			Ref<FieldStorageBase> storage = ScriptEngine::GetFieldStorage(entity, fieldID);

		//			if (!storage)
		//				continue;

		//			out << YAML::BeginMap; // Field
		//			out << YAML::Key << "ID" << YAML::Value << fieldInfo->ID;
		//			out << YAML::Key << "Name" << YAML::Value << fieldInfo->Name; // This is only here for the sake of debugging. All we need is the ID
		//			out << YAML::Key << "Type" << YAML::Value << FieldUtils::FieldTypeToString(fieldInfo->Type);

		//			if (fieldInfo->IsArray())
		//				out << YAML::Key << "Length" << YAML::Value << storage.As<ArrayFieldStorage>()->GetLength(); // Not strictly necessary but useful for readability

		//			out << YAML::Key << "Data" << YAML::Value;

		//			if (fieldInfo->IsArray())
		//			{
		//				out << YAML::BeginSeq;

		//				Ref<ArrayFieldStorage> arrayStorage = storage.As<ArrayFieldStorage>();

		//				for (uintptr_t i = 0; i < arrayStorage->GetLength(); i++)
		//				{
		//					switch (fieldInfo->Type)
		//					{
		//					case FieldType::Bool:
		//					{
		//						out << arrayStorage->GetValue<bool>(i);
		//						break;
		//					}
		//					case FieldType::Int8:
		//					{
		//						out << arrayStorage->GetValue<int8_t>(i);
		//						break;
		//					}
		//					case FieldType::Int16:
		//					{
		//						out << arrayStorage->GetValue<int16_t>(i);
		//						break;
		//					}
		//					case FieldType::Int32:
		//					{
		//						out << arrayStorage->GetValue<int32_t>(i);
		//						break;
		//					}
		//					case FieldType::Int64:
		//					{
		//						out << arrayStorage->GetValue<int64_t>(i);
		//						break;
		//					}
		//					case FieldType::UInt8:
		//					{
		//						out << arrayStorage->GetValue<uint8_t>(i);
		//						break;
		//					}
		//					case FieldType::UInt16:
		//					{
		//						out << arrayStorage->GetValue<uint16_t>(i);
		//						break;
		//					}
		//					case FieldType::UInt32:
		//					{
		//						out << arrayStorage->GetValue<uint32_t>(i);
		//						break;
		//					}
		//					case FieldType::UInt64:
		//					{
		//						out << arrayStorage->GetValue<uint64_t>(i);
		//						break;
		//					}
		//					case FieldType::Float:
		//					{
		//						out << arrayStorage->GetValue<float>(i);
		//						break;
		//					}
		//					case FieldType::Double:
		//					{
		//						out << arrayStorage->GetValue<double>(i);
		//						break;
		//					}
		//					case FieldType::String:
		//					{
		//						out << arrayStorage->GetValue<std::string>(i);
		//						break;
		//					}
		//					case FieldType::Vector2:
		//					{
		//						out << arrayStorage->GetValue<glm::vec2>(i);
		//						break;
		//					}
		//					case FieldType::Vector3:
		//					{
		//						out << arrayStorage->GetValue<glm::vec3>(i);
		//						break;
		//					}
		//					case FieldType::Vector4:
		//					{
		//						out << arrayStorage->GetValue<glm::vec4>(i);
		//						break;
		//					}
		//					case FieldType::Prefab:
		//					case FieldType::Entity:
		//					case FieldType::Mesh:
		//					case FieldType::StaticMesh:
		//					case FieldType::Material:
		//					case FieldType::PhysicsMaterial:
		//					case FieldType::Texture2D:
		//					{
		//						out << arrayStorage->GetValue<UUID>(i);
		//						break;
		//					}
		//					}
		//				}

		//				out << YAML::EndSeq;
		//			}
		//			else
		//			{
		//				Ref<FieldStorage> fieldStorage = storage.As<FieldStorage>();
		//				switch (fieldInfo->Type)
		//				{
		//				case FieldType::Bool:
		//				{
		//					out << fieldStorage->GetValue<bool>();
		//					break;
		//				}
		//				case FieldType::Int8:
		//				{
		//					out << fieldStorage->GetValue<int8_t>();
		//					break;
		//				}
		//				case FieldType::Int16:
		//				{
		//					out << fieldStorage->GetValue<int16_t>();
		//					break;
		//				}
		//				case FieldType::Int32:
		//				{
		//					out << fieldStorage->GetValue<int32_t>();
		//					break;
		//				}
		//				case FieldType::Int64:
		//				{
		//					out << fieldStorage->GetValue<int64_t>();
		//					break;
		//				}
		//				case FieldType::UInt8:
		//				{
		//					out << fieldStorage->GetValue<uint8_t>();
		//					break;
		//				}
		//				case FieldType::UInt16:
		//				{
		//					out << fieldStorage->GetValue<uint16_t>();
		//					break;
		//				}
		//				case FieldType::UInt32:
		//				{
		//					out << fieldStorage->GetValue<uint32_t>();
		//					break;
		//				}
		//				case FieldType::UInt64:
		//				{
		//					out << fieldStorage->GetValue<uint64_t>();
		//					break;
		//				}
		//				case FieldType::Float:
		//				{
		//					out << fieldStorage->GetValue<float>();
		//					break;
		//				}
		//				case FieldType::Double:
		//				{
		//					out << fieldStorage->GetValue<double>();
		//					break;
		//				}
		//				case FieldType::String:
		//				{
		//					out << fieldStorage->GetValue<std::string>();
		//					break;
		//				}
		//				case FieldType::Vector2:
		//				{
		//					out << fieldStorage->GetValue<glm::vec2>();
		//					break;
		//				}
		//				case FieldType::Vector3:
		//				{
		//					out << fieldStorage->GetValue<glm::vec3>();
		//					break;
		//				}
		//				case FieldType::Vector4:
		//				{
		//					out << fieldStorage->GetValue<glm::vec4>();
		//					break;
		//				}
		//				case FieldType::Prefab:
		//				case FieldType::Entity:
		//				case FieldType::Mesh:
		//				case FieldType::StaticMesh:
		//				case FieldType::Material:
		//				case FieldType::PhysicsMaterial:
		//				case FieldType::Texture2D:
		//				{
		//					out << fieldStorage->GetValue<UUID>();
		//					break;
		//				}
		//				}
		//			}
		//			out << YAML::EndMap; // Field
		//		}
		//		out << YAML::EndSeq;
		//	}

		//	out << YAML::EndMap; // ScriptComponent
		//}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; // MeshComponent

			auto& mc = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "AssetID" << YAML::Value << mc.Mesh;
			out << YAML::Key << "SubmeshIndex" << YAML::Value << mc.SubmeshIndex;

			auto materialTable = mc.MaterialTable;
			if (materialTable->GetMaterialCount() > 0)
			{
				out << YAML::Key << "MaterialTable" << YAML::Value << YAML::BeginMap; // MaterialTable

				for (uint32_t i = 0; i < materialTable->GetMaterialCount(); i++)
				{
					AssetHandle handle = (materialTable->HasMaterial(i) ? materialTable->GetMaterial(i) : (AssetHandle)0);
					out << YAML::Key << i << YAML::Value << handle;

				}

				out << YAML::EndMap; // MaterialTable
			}

			if (!mc.BoneEntityIds.empty())
				out << YAML::Key << "BoneEntities" << YAML::Value << YAML::Flow << mc.BoneEntityIds;

			out << YAML::Key << "Visible" << YAML::Value << mc.Visible;
			out << YAML::EndMap; // MeshComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; // StaticMeshComponent

			auto& smc = entity.GetComponent<StaticMeshComponent>();
			out << YAML::Key << "AssetID" << YAML::Value << smc.StaticMesh;

			auto materialTable = smc.MaterialTable;
			if (materialTable->GetMaterialCount() > 0)
			{
				out << YAML::Key << "MaterialTable" << YAML::Value << YAML::BeginMap; // MaterialTable

				for (uint32_t i = 0; i < materialTable->GetMaterialCount(); i++)
				{
					AssetHandle handle = (materialTable->HasMaterial(i) ? materialTable->GetMaterial(i) : (AssetHandle)0);
					out << YAML::Key << i << YAML::Value << handle;

				}

				out << YAML::EndMap; // MaterialTable
			}

			out << YAML::Key << "Visible" << YAML::Value << smc.Visible;
			out << YAML::EndMap; // StaticMeshComponent
		}

		//if (entity.HasComponent<AnimationComponent>())
		//{
		//	out << YAML::Key << "AnimationComponent";
		//	out << YAML::BeginMap; // AnimationComponent

		//	auto& anim = entity.GetComponent<AnimationComponent>();
		//	if (anim.AnimationController)
		//	{
		//		out << YAML::Key << "AnimationController" << YAML::Value << anim.AnimationController;
		//	}
		//	if (!anim.BoneEntityIds.empty())
		//	{
		//		out << YAML::Key << "BoneEntities" << YAML::Value << YAML::Flow << anim.BoneEntityIds;
		//	}
		//	out << YAML::Key << "EnableAnimation" << YAML::Value << anim.AnimationData->m_IsAnimationPlaying;
		//	out << YAML::Key << "PlaybackSpeed" << YAML::Value << anim.AnimationData->m_PlaybackSpeed;
		//	out << YAML::Key << "StateIndex" << YAML::Value << anim.AnimationData->m_StateIndex;
		//	out << YAML::Key << "AnimationTime" << YAML::Value << anim.AnimationData->m_AnimationTime;
		//	out << YAML::Key << "EnableRootMotion" << YAML::Value << anim.EnableRootMotion;
		//	out << YAML::Key << "RootMotionTarget" << YAML::Value << anim.RootMotionTarget;

		//	out << YAML::EndMap; // AnimationComponent
		//}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;
			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetDegPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera
			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; // DirectionalLightComponent

			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Intensity" << YAML::Value << directionalLightComponent.Intensity;
			out << YAML::Key << "Radiance" << YAML::Value << directionalLightComponent.Radiance;
			out << YAML::Key << "CastShadows" << YAML::Value << directionalLightComponent.CastShadows;
			out << YAML::Key << "SoftShadows" << YAML::Value << directionalLightComponent.SoftShadows;
			out << YAML::Key << "LightSize" << YAML::Value << directionalLightComponent.LightSize;
			out << YAML::Key << "ShadowAmount" << YAML::Value << directionalLightComponent.ShadowAmount;

			out << YAML::EndMap; // DirectionalLightComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; // PointLightComponent

			auto& lightComponent = entity.GetComponent<PointLightComponent>();
			out << YAML::Key << "Radiance" << YAML::Value << lightComponent.Radiance;
			out << YAML::Key << "Intensity" << YAML::Value << lightComponent.Intensity;
			out << YAML::Key << "CastShadows" << YAML::Value << lightComponent.CastsShadows;
			out << YAML::Key << "SoftShadows" << YAML::Value << lightComponent.SoftShadows;
			out << YAML::Key << "MinRadius" << YAML::Value << lightComponent.MinRadius;
			out << YAML::Key << "Radius" << YAML::Value << lightComponent.Radius;
			out << YAML::Key << "LightSize" << YAML::Value << lightComponent.LightSize;
			out << YAML::Key << "Falloff" << YAML::Value << lightComponent.Falloff;

			out << YAML::EndMap; // PointLightComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap; // SpotLightComponent

			const auto& lightComponent = entity.GetComponent<SpotLightComponent>();
			out << YAML::Key << "Radiance" << YAML::Value << lightComponent.Radiance;
			out << YAML::Key << "Angle" << YAML::Value << lightComponent.Angle;
			out << YAML::Key << "AngleAttenuation" << YAML::Value << lightComponent.AngleAttenuation;
			out << YAML::Key << "CastsShadows" << YAML::Value << lightComponent.CastsShadows;
			out << YAML::Key << "SoftShadows" << YAML::Value << lightComponent.SoftShadows;
			out << YAML::Key << "Falloff" << YAML::Value << lightComponent.Falloff;
			out << YAML::Key << "Intensity" << YAML::Value << lightComponent.Intensity;
			out << YAML::Key << "Range" << YAML::Value << lightComponent.Range;

			out << YAML::EndMap; // SpotLightComponent
		}

		if (entity.HasComponent<SkyLightComponent>())
		{
			out << YAML::Key << "SkyLightComponent";
			out << YAML::BeginMap; // SkyLightComponent

			auto& skyLightComponent = entity.GetComponent<SkyLightComponent>();
			out << YAML::Key << "EnvironmentMap" << YAML::Value << (AssetManager::IsMemoryAsset(skyLightComponent.SceneEnvironment) ? (AssetHandle)0 : skyLightComponent.SceneEnvironment);
			out << YAML::Key << "Intensity" << YAML::Value << skyLightComponent.Intensity;
			out << YAML::Key << "Lod" << YAML::Value << skyLightComponent.Lod;
			out << YAML::Key << "DynamicSky" << YAML::Value << skyLightComponent.DynamicSky;
			if (skyLightComponent.DynamicSky)
				out << YAML::Key << "TurbidityAzimuthInclination" << YAML::Value << skyLightComponent.TurbidityAzimuthInclination;

			out << YAML::EndMap; // SkyLightComponent
		}

		if (entity.HasComponent<FogVolumeComponent>())
		{
			out << YAML::Key << "FogVolumeComponent";
			out << YAML::BeginMap; // FogVolumeComponent
			out << YAML::EndMap; // FogVolumeComponent
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent

			auto& spriteRendererComponent = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;

			out << YAML::Key << "Texture" << YAML::Value << spriteRendererComponent.Texture;
			out << YAML::Key << "TilingFactor" << YAML::Value << spriteRendererComponent.TilingFactor;
			out << YAML::Key << "UVStart" << YAML::Value << spriteRendererComponent.UVStart;
			out << YAML::Key << "UVEnd" << YAML::Value << spriteRendererComponent.UVEnd;

			out << YAML::EndMap; // SpriteRendererComponent
		}

		if (entity.HasComponent<TextComponent>())
		{
			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap; // TextComponent

			auto& textComponent = entity.GetComponent<TextComponent>();
			out << YAML::Key << "TextString" << YAML::Value << textComponent.TextString;
			out << YAML::Key << "FontHandle" << YAML::Value << textComponent.FontHandle;
			out << YAML::Key << "Color" << YAML::Value << textComponent.Color;
			out << YAML::Key << "LineSpacing" << YAML::Value << textComponent.LineSpacing;
			out << YAML::Key << "Kerning" << YAML::Value << textComponent.Kerning;
			out << YAML::Key << "MaxWidth" << YAML::Value << textComponent.MaxWidth;

			out << YAML::EndMap; // TextComponent
		}

		//if (entity.HasComponent<RigidBody2DComponent>())
		//{
		//	out << YAML::Key << "RigidBody2DComponent";
		//	out << YAML::BeginMap; // RigidBody2DComponent

		//	auto& rigidbody2DComponent = entity.GetComponent<RigidBody2DComponent>();
		//	out << YAML::Key << "BodyType" << YAML::Value << (int)rigidbody2DComponent.BodyType;
		//	out << YAML::Key << "FixedRotation" << YAML::Value << rigidbody2DComponent.FixedRotation;

		//	out << YAML::EndMap; // RigidBody2DComponent
		//}

		//if (entity.HasComponent<BoxCollider2DComponent>())
		//{
		//	out << YAML::Key << "BoxCollider2DComponent";
		//	out << YAML::BeginMap; // BoxCollider2DComponent

		//	auto& boxCollider2DComponent = entity.GetComponent<BoxCollider2DComponent>();
		//	out << YAML::Key << "Offset" << YAML::Value << boxCollider2DComponent.Offset;
		//	out << YAML::Key << "Size" << YAML::Value << boxCollider2DComponent.Size;
		//	out << YAML::Key << "Density" << YAML::Value << boxCollider2DComponent.Density;
		//	out << YAML::Key << "Friction" << YAML::Value << boxCollider2DComponent.Friction;

		//	out << YAML::EndMap; // BoxCollider2DComponent
		//}

		//if (entity.HasComponent<CircleCollider2DComponent>())
		//{
		//	out << YAML::Key << "CircleCollider2DComponent";
		//	out << YAML::BeginMap; // CircleCollider2DComponent

		//	auto& circleCollider2DComponent = entity.GetComponent<CircleCollider2DComponent>();
		//	out << YAML::Key << "Offset" << YAML::Value << circleCollider2DComponent.Offset;
		//	out << YAML::Key << "Radius" << YAML::Value << circleCollider2DComponent.Radius;
		//	out << YAML::Key << "Density" << YAML::Value << circleCollider2DComponent.Density;
		//	out << YAML::Key << "Friction" << YAML::Value << circleCollider2DComponent.Friction;

		//	out << YAML::EndMap; // CircleCollider2DComponent
		//}

		//if (entity.HasComponent<RigidBodyComponent>())
		//{
		//	out << YAML::Key << "RigidBodyComponent";
		//	out << YAML::BeginMap; // RigidBodyComponent

		//	auto& rigidbodyComponent = entity.GetComponent<RigidBodyComponent>();
		//	out << YAML::Key << "BodyType" << YAML::Value << (int)rigidbodyComponent.BodyType;
		//	out << YAML::Key << "LayerID" << YAML::Value << rigidbodyComponent.LayerID;
		//	out << YAML::Key << "Mass" << YAML::Value << rigidbodyComponent.Mass;
		//	out << YAML::Key << "LinearDrag" << YAML::Value << rigidbodyComponent.LinearDrag;
		//	out << YAML::Key << "AngularDrag" << YAML::Value << rigidbodyComponent.AngularDrag;
		//	out << YAML::Key << "DisableGravity" << YAML::Value << rigidbodyComponent.DisableGravity;
		//	out << YAML::Key << "IsKinematic" << YAML::Value << rigidbodyComponent.IsKinematic;
		//	out << YAML::Key << "CollisionDetection" << YAML::Value << (uint32_t)rigidbodyComponent.CollisionDetection;
		//	out << YAML::Key << "LockFlags" << YAML::Value << rigidbodyComponent.LockFlags;

		//	out << YAML::EndMap; // RigidBodyComponent
		//}

		if (entity.HasComponent<CharacterControllerComponent>())
		{
			out << YAML::Key << "CharacterControllerComponent";
			out << YAML::BeginMap; // CharacterControllerComponent

			auto& ccc = entity.GetComponent<CharacterControllerComponent>();
			out << YAML::Key << "Layer" << YAML::Value << ccc.LayerID;
			out << YAML::Key << "DisableGravity" << YAML::Value << ccc.DisableGravity;
			out << YAML::Key << "SlopeLimit" << YAML::Value << ccc.SlopeLimitDeg;
			out << YAML::Key << "StepOffset" << YAML::Value << ccc.StepOffset;

			out << YAML::EndMap; // CharacterControllerComponent
		}

		//if (entity.HasComponent<FixedJointComponent>())
		//{
		//	out << YAML::Key << "FixedJointComponent";
		//	out << YAML::BeginMap; // FixedJointComponent

		//	auto& fjc = entity.GetComponent<FixedJointComponent>();
		//	out << YAML::Key << "ConnectedEntity" << YAML::Value << fjc.ConnectedEntity;
		//	out << YAML::Key << "IsBreakable" << YAML::Value << fjc.IsBreakable;
		//	out << YAML::Key << "BreakForce" << YAML::Value << fjc.BreakForce;
		//	out << YAML::Key << "BreakTorque" << YAML::Value << fjc.BreakTorque;
		//	out << YAML::Key << "EnableCollision" << YAML::Value << fjc.EnableCollision;
		//	out << YAML::Key << "EnablePreProcessing" << YAML::Value << fjc.EnablePreProcessing;


		//	out << YAML::EndMap; // FixedJointComponent
		//}

		//if (entity.HasComponent<BoxColliderComponent>())
		//{
		//	out << YAML::Key << "BoxColliderComponent";
		//	out << YAML::BeginMap; // BoxColliderComponent

		//	auto& boxColliderComponent = entity.GetComponent<BoxColliderComponent>();
		//	out << YAML::Key << "Offset" << YAML::Value << boxColliderComponent.Offset;
		//	out << YAML::Key << "HalfSize" << YAML::Value << boxColliderComponent.HalfSize;
		//	out << YAML::Key << "IsTrigger" << YAML::Value << boxColliderComponent.IsTrigger;

		//	if (boxColliderComponent.Material)
		//		out << YAML::Key << "Material" << YAML::Value << boxColliderComponent.Material;
		//	else
		//		out << YAML::Key << "Material" << YAML::Value << 0;

		//	out << YAML::EndMap; // BoxColliderComponent
		//}

		//if (entity.HasComponent<SphereColliderComponent>())
		//{
		//	out << YAML::Key << "SphereColliderComponent";
		//	out << YAML::BeginMap; // SphereColliderComponent

		//	auto& sphereColliderComponent = entity.GetComponent<SphereColliderComponent>();
		//	out << YAML::Key << "Radius" << YAML::Value << sphereColliderComponent.Radius;
		//	out << YAML::Key << "IsTrigger" << YAML::Value << sphereColliderComponent.IsTrigger;
		//	out << YAML::Key << "Offset" << YAML::Value << sphereColliderComponent.Offset;

		//	if (sphereColliderComponent.Material)
		//		out << YAML::Key << "Material" << YAML::Value << sphereColliderComponent.Material;
		//	else
		//		out << YAML::Key << "Material" << YAML::Value << 0;

		//	out << YAML::EndMap; // SphereColliderComponent
		//}

		//if (entity.HasComponent<CapsuleColliderComponent>())
		//{
		//	out << YAML::Key << "CapsuleColliderComponent";
		//	out << YAML::BeginMap; // CapsuleColliderComponent

		//	auto& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();
		//	out << YAML::Key << "Radius" << YAML::Value << capsuleColliderComponent.Radius;
		//	out << YAML::Key << "Height" << YAML::Value << capsuleColliderComponent.Height;
		//	out << YAML::Key << "Offset" << YAML::Value << capsuleColliderComponent.Offset;
		//	out << YAML::Key << "IsTrigger" << YAML::Value << capsuleColliderComponent.IsTrigger;

		//	if (capsuleColliderComponent.Material)
		//		out << YAML::Key << "Material" << YAML::Value << capsuleColliderComponent.Material;
		//	else
		//		out << YAML::Key << "Material" << YAML::Value << 0;

		//	out << YAML::EndMap; // CapsuleColliderComponent
		//}

		//if (entity.HasComponent<MeshColliderComponent>())
		//{
		//	out << YAML::Key << "MeshColliderComponent";
		//	out << YAML::BeginMap; // MeshColliderComponent

		//	auto& meshColliderComponent = entity.GetComponent<MeshColliderComponent>();

		//	out << YAML::Key << "ColliderHandle" << YAML::Value << meshColliderComponent.ColliderAsset;
		//	out << YAML::Key << "IsTrigger" << YAML::Value << meshColliderComponent.IsTrigger;
		//	out << YAML::Key << "UseSharedShape" << YAML::Value << meshColliderComponent.UseSharedShape;
		//	out << YAML::Key << "OverrideMaterial" << YAML::Value << meshColliderComponent.OverrideMaterial;
		//	out << YAML::Key << "CollisionComplexity" << YAML::Value << (uint8_t)meshColliderComponent.CollisionComplexity;
		//	out << YAML::EndMap; // MeshColliderComponent
		//}

//		if (entity.HasComponent<AudioComponent>())
//		{
//#define X2_SERIALIZE_PROPERTY(propName, propVal, outputData) outputData << YAML::Key << #propName << YAML::Value << propVal
//
//			out << YAML::Key << "AudioComponent";
//			out << YAML::BeginMap; // AudioComponent
//			auto& audioComponent = entity.GetComponent<AudioComponent>();
//
//			X2_SERIALIZE_PROPERTY(StartEvent, audioComponent.StartEvent, out);
//			X2_SERIALIZE_PROPERTY(StartCommandID, audioComponent.StartCommandID, out);
//
//			X2_SERIALIZE_PROPERTY(PlayOnAwake, audioComponent.bPlayOnAwake, out);
//			X2_SERIALIZE_PROPERTY(StopIfEntityDestroyed, audioComponent.bStopWhenEntityDestroyed, out);
//			X2_SERIALIZE_PROPERTY(VolumeMultiplier, audioComponent.VolumeMultiplier, out);
//			X2_SERIALIZE_PROPERTY(PitchMultiplier, audioComponent.PitchMultiplier, out);
//			X2_SERIALIZE_PROPERTY(AutoDestroy, audioComponent.bAutoDestroy, out);
//
//			out << YAML::EndMap; // AudioComponent
//		}
//
//		if (entity.HasComponent<AudioListenerComponent>())
//		{
//			out << YAML::Key << "AudioListenerComponent";
//			out << YAML::BeginMap; // AudioComponent
//			auto& audioListenerComponent = entity.GetComponent<AudioListenerComponent>();
//			X2_SERIALIZE_PROPERTY(Active, audioListenerComponent.Active, out);
//			X2_SERIALIZE_PROPERTY(ConeInnerAngle, audioListenerComponent.ConeInnerAngleInRadians, out);
//			X2_SERIALIZE_PROPERTY(ConeOuterAngle, audioListenerComponent.ConeOuterAngleInRadians, out);
//			X2_SERIALIZE_PROPERTY(ConeOuterGain, audioListenerComponent.ConeOuterGain, out);
//			out << YAML::EndMap; // AudioComponent
//
//#undef X2_SERIALIZE_PROPERTY
//		}

		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		YAML::Emitter out;
		SerializeToYAML(out);

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	void SceneSerializer::SerializeToYAML(YAML::Emitter& out)
	{
		// set all animation components to time 0 (unless they have explicitly been paused)
		// as otherwise each time we serialize then all of the bone entity transforms will be slightly different
		// which is annoying (e.g. for version control of scene files)
		/*auto entities = m_Scene->GetAllEntitiesWith<AnimationComponent>();
		for (auto e : entities)
		{
			Entity entity = { e, m_Scene.Raw() };
			auto& anim = entity.GetComponent<AnimationComponent>();
			if (anim.AnimationData->m_IsAnimationPlaying)
				anim.AnimationData->m_AnimationTime = 0.0f;
		}*/
		//m_Scene->UpdateAnimation(0.0f, false);

		out << YAML::BeginMap;
		out << YAML::Key << "Scene";
		out << YAML::Value << m_Scene->GetName();

		out << YAML::Key << "Entities";
		out << YAML::Value << YAML::BeginSeq;

		// Sort entities by UUID (for better serializing)
		std::map<UUID, entt::entity> sortedEntityMap;
		auto idComponentView = m_Scene->m_Registry.view<IDComponent>();
		for (auto entity : idComponentView)
			sortedEntityMap[idComponentView.get<IDComponent>(entity).ID] = entity;

		// Serialize sorted entities
		for (auto [id, entity] : sortedEntityMap)
			SerializeEntity(out, { entity, m_Scene.Raw() });

		out << YAML::EndSeq;

		// Scene Audio
		//MiniAudioEngine::Get().SerializeSceneAudio(out, m_Scene);

		out << YAML::EndMap;
	}

	bool SceneSerializer::DeserializeFromYAML(const std::string& yamlString)
	{
		YAML::Node data = YAML::Load(yamlString);
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		X2_CORE_INFO_TAG("AssetManager", "Deserializing scene '{0}'", sceneName);
		m_Scene->SetName(sceneName);

		auto entities = data["Entities"];
		if (entities)
			DeserializeEntities(entities, m_Scene);

		/*auto sceneAudio = data["SceneAudio"];
		if (sceneAudio)
			MiniAudioEngine::Get().DeserializeSceneAudio(sceneAudio);*/

		// Sort IdComponent by by entity handle (which is essentially the order in which they were created)
		// This ensures a consistent ordering when iterating IdComponent (for example: when rendering scene hierarchy panel)
		m_Scene->m_Registry.sort<IDComponent>([this](const auto lhs, const auto rhs)
			{
				auto lhsEntity = m_Scene->m_EntityIDMap.find(lhs.ID);
				auto rhsEntity = m_Scene->m_EntityIDMap.find(rhs.ID);
				return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
			});

		return true;
	}

	void SceneSerializer::SerializeRuntime(const std::filesystem::path& filepath)
	{
		// Not implemented
		X2_CORE_ASSERT(false);
	}

	void SceneSerializer::DeserializeEntities(YAML::Node& entitiesNode, Ref<Scene> scene)
	{
		for (auto entity : entitiesNode)
		{
			uint64_t uuid = entity["Entity"].as<uint64_t>();

			std::string name;
			auto tagComponent = entity["TagComponent"];
			if (tagComponent)
				name = tagComponent["Tag"].as<std::string>();

			//X2_CORE_INFO("Deserialized Entity '{0}' with ID '{1}'", name, uuid);

			Entity deserializedEntity = scene->CreateEntityWithID(uuid, name, false);

			auto& relationshipComponent = deserializedEntity.GetComponent<RelationshipComponent>();
			uint64_t parentHandle = entity["Parent"] ? entity["Parent"].as<uint64_t>() : 0;
			relationshipComponent.ParentHandle = parentHandle;

			auto children = entity["Children"];
			if (children)
			{
				for (auto child : children)
				{
					uint64_t childHandle = child["Handle"].as<uint64_t>();
					relationshipComponent.Children.push_back(childHandle);
				}
			}

			auto prefabComponent = entity["PrefabComponent"];
			if (prefabComponent)
			{
				auto& pb = deserializedEntity.AddComponent<PrefabComponent>();

				pb.PrefabID = prefabComponent["Prefab"].as<uint64_t>();
				pb.EntityID = prefabComponent["Entity"].as<uint64_t>();
			}

			auto transformComponent = entity["TransformComponent"];
			if (transformComponent)
			{
				// Entities always have transforms
				auto& transform = deserializedEntity.GetComponent<TransformComponent>();
				transform.Translation = transformComponent["Position"].as<glm::vec3>(glm::vec3(0.0f));
				auto& rotationNode = transformComponent["Rotation"];
				// Early versions of X2 saved rotations as quaternions
				if (rotationNode.size() == 4)
				{
					transform.SetRotation(transformComponent["Rotation"].as<glm::quat>(glm::quat()));
				}
				else
				{
					X2_CORE_ASSERT(rotationNode.size() == 3);
					transform.SetRotationEuler(transformComponent["Rotation"].as<glm::vec3>(glm::vec3(0.0f)));
				}
				transform.Scale = transformComponent["Scale"].as<glm::vec3>();
			}

		/*	auto scriptComponent = entity["ScriptComponent"];
			if (scriptComponent)
			{
				AssetHandle scriptAssetHandle = scriptComponent["ClassHandle"] ? scriptComponent["ClassHandle"].as<AssetHandle>(AssetHandle(0)) : AssetHandle(0);
				std::string moduleName = scriptComponent["ModuleName"] ? scriptComponent["ModuleName"].as<std::string>("") : "";
				std::string name = scriptComponent["Name"].as<std::string>("");

				if (scriptAssetHandle == 0 && !moduleName.empty())
					scriptAssetHandle = X2_SCRIPT_CLASS_ID(moduleName);

				if (scriptAssetHandle != 0)
				{
					ScriptComponent& sc = deserializedEntity.AddComponent<ScriptComponent>(scriptAssetHandle);
					ScriptEngine::InitializeScriptEntity(deserializedEntity);

					if (sc.FieldIDs.size() > 0)
					{
						auto storedFields = scriptComponent["StoredFields"];
						if (storedFields)
						{
							for (auto field : storedFields)
							{
								uint32_t id = field["ID"].as<uint32_t>(0);
								std::string fullName = field["Name"].as<std::string>();
								std::string name = Utils::String::SubStr(fullName, fullName.find(':') + 1);
								std::string typeStr = field["Type"].as<std::string>("");
								FieldInfo* fieldData = ScriptCache::GetFieldByID(id);
								Ref<FieldStorageBase> storage = ScriptEngine::GetFieldStorage(deserializedEntity, id);

								if (storage == nullptr)
								{
									id = Hash::GenerateFNVHash(name);
									storage = ScriptEngine::GetFieldStorage(deserializedEntity, id);
								}

								if (storage == nullptr)
								{
									X2_CONSOLE_LOG_WARN("Serialized C# field {0} doesn't exist in script cache! This could be because the script field no longer exists or because it's been renamed.", name);
								}
								else
								{
									auto dataNode = field["Data"];

									if (fieldData->IsArray() && dataNode.IsSequence())
									{
										Ref<ArrayFieldStorage> arrayStorage = storage.As<ArrayFieldStorage>();
										arrayStorage->Resize(dataNode.size());

										for (uintptr_t i = 0; i < dataNode.size(); i++)
										{
											switch (fieldData->Type)
											{
											case FieldType::Bool:
											{
												arrayStorage->SetValue(i, dataNode[i].as<bool>());
												break;
											}
											case FieldType::Int8:
											{
												arrayStorage->SetValue(i, static_cast<int8_t>(dataNode[i].as<int16_t>()));
												break;
											}
											case FieldType::Int16:
											{
												arrayStorage->SetValue(i, dataNode[i].as<int16_t>());
												break;
											}
											case FieldType::Int32:
											{
												arrayStorage->SetValue(i, dataNode[i].as<int32_t>());
												break;
											}
											case FieldType::Int64:
											{
												arrayStorage->SetValue(i, dataNode[i].as<int64_t>());
												break;
											}
											case FieldType::UInt8:
											{
												arrayStorage->SetValue(i, dataNode[i].as<uint8_t>());
												break;
											}
											case FieldType::UInt16:
											{
												arrayStorage->SetValue(i, dataNode[i].as<uint16_t>());
												break;
											}
											case FieldType::UInt32:
											{
												arrayStorage->SetValue(i, dataNode[i].as<uint32_t>());
												break;
											}
											case FieldType::UInt64:
											{
												arrayStorage->SetValue(i, dataNode[i].as<uint64_t>());
												break;
											}
											case FieldType::Float:
											{
												arrayStorage->SetValue(i, dataNode[i].as<float>());
												break;
											}
											case FieldType::Double:
											{
												arrayStorage->SetValue(i, dataNode[i].as<double>());
												break;
											}
											case FieldType::String:
											{
												arrayStorage->SetValue(i, dataNode[i].as<std::string>());
												break;
											}
											case FieldType::Vector2:
											{
												arrayStorage->SetValue(i, dataNode[i].as<glm::vec2>());
												break;
											}
											case FieldType::Vector3:
											{
												arrayStorage->SetValue(i, dataNode[i].as<glm::vec3>());
												break;
											}
											case FieldType::Vector4:
											{
												arrayStorage->SetValue(i, dataNode[i].as<glm::vec4>());
												break;
											}
											case FieldType::Prefab:
											case FieldType::Entity:
											case FieldType::Mesh:
											case FieldType::StaticMesh:
											case FieldType::Material:
											case FieldType::PhysicsMaterial:
											case FieldType::Texture2D:
											{
												arrayStorage->SetValue(i, dataNode[i].as<UUID>());
												break;
											}
											}
										}
									}
									else
									{
										Ref<FieldStorage> fieldStorage = storage.As<FieldStorage>();
										switch (fieldData->Type)
										{
										case FieldType::Bool:
										{
											fieldStorage->SetValue(dataNode.as<bool>());
											break;
										}
										case FieldType::Int8:
										{
											fieldStorage->SetValue(static_cast<int8_t>(dataNode.as<int16_t>()));
											break;
										}
										case FieldType::Int16:
										{
											fieldStorage->SetValue(dataNode.as<int16_t>());
											break;
										}
										case FieldType::Int32:
										{
											fieldStorage->SetValue(dataNode.as<int32_t>());
											break;
										}
										case FieldType::Int64:
										{
											fieldStorage->SetValue(dataNode.as<int64_t>());
											break;
										}
										case FieldType::UInt8:
										{
											fieldStorage->SetValue(dataNode.as<uint8_t>());
											break;
										}
										case FieldType::UInt16:
										{
											fieldStorage->SetValue(dataNode.as<uint16_t>());
											break;
										}
										case FieldType::UInt32:
										{
											fieldStorage->SetValue(dataNode.as<uint32_t>());
											break;
										}
										case FieldType::UInt64:
										{
											fieldStorage->SetValue(dataNode.as<uint64_t>());
											break;
										}
										case FieldType::Float:
										{
											fieldStorage->SetValue(dataNode.as<float>());
											break;
										}
										case FieldType::Double:
										{
											fieldStorage->SetValue(dataNode.as<double>());
											break;
										}
										case FieldType::String:
										{
											fieldStorage->SetValue(dataNode.as<std::string>());
											break;
										}
										case FieldType::Vector2:
										{
											fieldStorage->SetValue(dataNode.as<glm::vec2>());
											break;
										}
										case FieldType::Vector3:
										{
											fieldStorage->SetValue(dataNode.as<glm::vec3>());
											break;
										}
										case FieldType::Vector4:
										{
											fieldStorage->SetValue(dataNode.as<glm::vec4>());
											break;
										}
										case FieldType::Prefab:
										case FieldType::Entity:
										case FieldType::Mesh:
										case FieldType::StaticMesh:
										case FieldType::Material:
										case FieldType::PhysicsMaterial:
										case FieldType::Texture2D:
										{
											fieldStorage->SetValue(dataNode.as<UUID>());
											break;
										}
										}
									}
								}
							}
						}
					}
				}
				else
				{
					X2_CORE_ERROR("Failed to deserialize ScriptComponent for entity '{0}'! Couldn't find a valid ModuleName or ClassHandle field/value!", deserializedEntity.Name());
				}
			}*/

			auto meshComponent = entity["MeshComponent"];
			if (meshComponent)
			{
				auto& component = deserializedEntity.AddComponent<MeshComponent>();

				AssetHandle assetHandle = meshComponent["AssetID"].as<uint64_t>();
				if (AssetManager::IsAssetHandleValid(assetHandle))
				{
					AssetType type = AssetManager::GetAssetType(assetHandle);
					if (type == AssetType::Mesh)
						component.Mesh = assetHandle;
#if DEPRECTATED
					else if (type == AssetType::MeshSource)
					{
						// Create new mesh
						Ref<MeshSource> meshAsset = AssetManager::GetAsset<MeshSource>(assetHandle);
						std::filesystem::path meshPath = metadata.FilePath;
						std::filesystem::path meshDirectory = Project::GetMeshPath();
						std::string filename = fmt::format("{0}.xmesh", meshPath.stem().string());
						Ref<Mesh> mesh = Project::GetEditorAssetManager()->CreateNewAsset<Mesh>(filename, meshDirectory.string(), meshAsset);
						component.Mesh = mesh->Handle;
						AssetImporter::Serialize(mesh);
					}
#endif
				}
				/*else
				{
					component.Mesh = Ref<Asset>::Create().As<Mesh>();
					component.Mesh->SetFlag(AssetFlag::Missing, true);

					std::string filepath = meshComponent["AssetPath"] ? meshComponent["AssetPath"].as<std::string>() : "";
					if (filepath.empty())
					{
						X2_CORE_ERROR("Tried to load non-existent mesh in Entity: {0}", deserializedEntity.GetUUID());
					}
					else
					{
						X2_CORE_ERROR("Tried to load invalid mesh '{0}' in Entity {1}", filepath, deserializedEntity.GetUUID());
						component.Mesh->SetFlag(AssetFlag::Invalid, true);
					}
				}*/

				if (meshComponent["SubmeshIndex"])
					component.SubmeshIndex = meshComponent["SubmeshIndex"].as<uint32_t>();

				if (meshComponent["MaterialTable"])
				{
					YAML::Node materialTableNode = meshComponent["MaterialTable"];
					for (auto materialEntry : materialTableNode)
					{
						uint32_t index = materialEntry.first.as<uint32_t>();
						AssetHandle materialAsset = materialEntry.second.as<AssetHandle>();
						if (materialAsset && AssetManager::IsAssetHandleValid(materialAsset))
							component.MaterialTable->SetMaterial(index, materialAsset);
					}
				}

				component.BoneEntityIds = meshComponent["BoneEntities"].as<std::vector<UUID>>(std::vector<UUID>{});
				if (meshComponent["Visible"])
					component.Visible = meshComponent["Visible"].as<bool>();
			}

			auto staticMeshComponent = entity["StaticMeshComponent"];
			if (staticMeshComponent)
			{
				auto& component = deserializedEntity.AddComponent<StaticMeshComponent>();

				AssetHandle assetHandle = staticMeshComponent["AssetID"].as<uint64_t>();
				if (AssetManager::IsAssetHandleValid(assetHandle))
					component.StaticMesh = assetHandle;

				if (staticMeshComponent["MaterialTable"])
				{
					YAML::Node materialTableNode = staticMeshComponent["MaterialTable"];
					for (auto materialEntry : materialTableNode)
					{
						uint32_t index = materialEntry.first.as<uint32_t>();
						AssetHandle materialAsset = materialEntry.second.as<AssetHandle>(0);
						if (materialAsset && AssetManager::IsAssetHandleValid(materialAsset))
							component.MaterialTable->SetMaterial(index, materialAsset);
					}
				}

				if (staticMeshComponent["Visible"])
					component.Visible = staticMeshComponent["Visible"].as<bool>();
			}

			/*auto animationComponent = entity["AnimationComponent"];
			if (animationComponent)
			{
				auto& component = deserializedEntity.AddComponent<AnimationComponent>();
				if (animationComponent["AnimationController"])
				{
					AssetHandle animationControllerHandle = animationComponent["AnimationController"].as<uint64_t>();
					if (AssetManager::IsAssetHandleValid(animationControllerHandle))
					{
						AssetType type = AssetManager::GetAssetType(animationControllerHandle);
						if (type == AssetType::AnimationController)
						{
							component.AnimationController = animationControllerHandle;
							auto animationController = AssetManager::GetAsset<AnimationController>(animationControllerHandle);
							if (animationController && animationController->IsValid() && animationController->GetSkeletonAsset())
							{
								const auto& skeleton = animationController->GetSkeletonAsset()->GetSkeleton();
								component.AnimationData->Resize(skeleton.GetNumBones());
								component.AnimationData->SetLocalTransforms(skeleton.GetBoneTranslations(), skeleton.GetBoneRotations(), skeleton.GetBoneScales());
							}
						}
					}
				}
				component.BoneEntityIds = animationComponent["BoneEntities"].as<std::vector<UUID>>(std::vector<UUID>{});
				component.AnimationData->m_IsAnimationPlaying = animationComponent["EnableAnimation"].as<bool>(component.AnimationData->m_IsAnimationPlaying);
				component.AnimationData->m_PlaybackSpeed = animationComponent["PlaybackSpeed"].as<float>(component.AnimationData->m_PlaybackSpeed);
				component.AnimationData->m_AnimationTime = animationComponent["AnimationTime"].as<float>(component.AnimationData->m_AnimationTime);
				component.AnimationData->m_StateIndex = animationComponent["StateIndex"].as<size_t>(component.AnimationData->m_StateIndex);
				component.EnableRootMotion = animationComponent["EnableRootMotion"].as<bool>(component.EnableRootMotion);
				component.RootMotionTarget = animationComponent["RootMotionTarget"].as<uint64_t>(component.RootMotionTarget);
			}*/

			auto cameraComponent = entity["CameraComponent"];
			if (cameraComponent)
			{
				auto& component = deserializedEntity.AddComponent<CameraComponent>();
				auto& cameraNode = cameraComponent["Camera"];

				component.Camera = SceneCamera();
				auto& camera = component.Camera;

				if (cameraNode.IsMap())
				{
					if (cameraNode["ProjectionType"])
						camera.SetProjectionType((SceneCamera::ProjectionType)cameraNode["ProjectionType"].as<int>());
					if (cameraNode["PerspectiveFOV"])
						camera.SetDegPerspectiveVerticalFOV(cameraNode["PerspectiveFOV"].as<float>());
					if (cameraNode["PerspectiveNear"])
						camera.SetPerspectiveNearClip(cameraNode["PerspectiveNear"].as<float>());
					if (cameraNode["PerspectiveFar"])
						camera.SetPerspectiveFarClip(cameraNode["PerspectiveFar"].as<float>());
					if (cameraNode["OrthographicSize"])
						camera.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
					if (cameraNode["OrthographicNear"])
						camera.SetOrthographicNearClip(cameraNode["OrthographicNear"].as<float>());
					if (cameraNode["OrthographicFar"])
						camera.SetOrthographicFarClip(cameraNode["OrthographicFar"].as<float>());
				}

				component.Primary = cameraComponent["Primary"].as<bool>();
			}

			auto directionalLightComponent = entity["DirectionalLightComponent"];
			if (directionalLightComponent)
			{
				auto& component = deserializedEntity.AddComponent<DirectionalLightComponent>();
				component.Intensity = directionalLightComponent["Intensity"].as<float>(1.0f);
				component.Radiance = directionalLightComponent["Radiance"].as<glm::vec3>(glm::vec3(1.0f));
				component.CastShadows = directionalLightComponent["CastShadows"].as<bool>(true);
				component.SoftShadows = directionalLightComponent["SoftShadows"].as<bool>(true);
				component.LightSize = directionalLightComponent["LightSize"].as<float>(0.5f);
				component.ShadowAmount = directionalLightComponent["ShadowAmount"].as<float>(1.0f);
			}

			auto pointLightComponent = entity["PointLightComponent"];
			if (pointLightComponent)
			{
				auto& component = deserializedEntity.AddComponent<PointLightComponent>();
				component.Radiance = pointLightComponent["Radiance"].as<glm::vec3>();
				component.Intensity = pointLightComponent["Intensity"].as<float>();
				component.CastsShadows = pointLightComponent["CastShadows"].as<bool>();
				component.SoftShadows = pointLightComponent["SoftShadows"].as<bool>();
				component.LightSize = pointLightComponent["LightSize"].as<float>();
				component.Radius = pointLightComponent["Radius"].as<float>();
				component.MinRadius = pointLightComponent["MinRadius"].as<float>();
				component.Falloff = pointLightComponent["Falloff"].as<float>();
			}

			auto spotLightComponent = entity["SpotLightComponent"];
			if (spotLightComponent)
			{
				auto& component = deserializedEntity.AddComponent<SpotLightComponent>();
				component.Radiance = spotLightComponent["Radiance"].as<glm::vec3>();
				component.Intensity = spotLightComponent["Intensity"].as<float>();
				component.CastsShadows = spotLightComponent["CastsShadows"].as<bool>();
				component.SoftShadows = spotLightComponent["SoftShadows"].as<bool>();
				component.Angle = spotLightComponent["Angle"].as<float>();
				component.Range = spotLightComponent["Range"].as<float>();
				component.Falloff = spotLightComponent["Falloff"].as<float>();
				component.AngleAttenuation = spotLightComponent["AngleAttenuation"].as<float>();
			}

			auto skyLightComponent = entity["SkyLightComponent"];
			if (skyLightComponent)
			{
				auto& component = deserializedEntity.AddComponent<SkyLightComponent>();

				if (skyLightComponent["EnvironmentMap"])
				{
					AssetHandle assetHandle = skyLightComponent["EnvironmentMap"].as<uint64_t>();
					if (AssetManager::IsAssetHandleValid(assetHandle))
					{
						component.SceneEnvironment = assetHandle;
					}
				}

				component.Intensity = skyLightComponent["Intensity"].as<float>();
				component.Lod = skyLightComponent["Lod"].as<float>(1.0f); // default matches original SkyLightComponent.Lod default

				if (skyLightComponent["DynamicSky"])
				{
					component.DynamicSky = skyLightComponent["DynamicSky"].as<bool>();
					if (component.DynamicSky)
					{
						component.TurbidityAzimuthInclination = skyLightComponent["TurbidityAzimuthInclination"].as<glm::vec3>();
						//skyLightComponent.SceneEnvironment = AssetManager::CreateMemoryOnlyAsset<Environment>(preethamEnv, preethamEnv);
					}
				}

				//if (!AssetManager::IsAssetHandleValid(component.SceneEnvironment))
				//{
				//	X2_CORE_ERROR("Tried to load invalid environment map in Entity {0}", deserializedEntity.GetUUID());
				//}
			}

			auto fogVolumeComponent = entity["FogVolumeComponent"];
			if (fogVolumeComponent)
			{
				auto& component = deserializedEntity.AddComponent<FogVolumeComponent>();
			}

			auto spriteRendererComponent = entity["SpriteRendererComponent"];
			if (spriteRendererComponent)
			{
				auto& component = deserializedEntity.AddComponent<SpriteRendererComponent>();
				component.Color = spriteRendererComponent["Color"].as<glm::vec4>();
				if (spriteRendererComponent["Texture"])
					component.Texture = spriteRendererComponent["Texture"].as<AssetHandle>();
				component.TilingFactor = spriteRendererComponent["TilingFactor"].as<float>();
				if (spriteRendererComponent["UVStart"])
					component.UVStart = spriteRendererComponent["UVStart"].as<glm::vec2>();
				if (spriteRendererComponent["UVEnd"])
					component.UVEnd = spriteRendererComponent["UVEnd"].as<glm::vec2>();
			}

			/*auto textComponent = entity["TextComponent"];
			if (textComponent)
			{
				auto& component = deserializedEntity.AddComponent<TextComponent>();
				component.TextString = textComponent["TextString"].as<std::string>();
				component.TextHash = std::hash<std::string>()(component.TextString);
				AssetHandle fontHandle = textComponent["FontHandle"].as<uint64_t>();
				if (AssetManager::IsAssetHandleValid(fontHandle))
					component.FontHandle = fontHandle;
				else
					component.FontHandle = Font::GetDefaultFont()->Handle;
				component.Color = textComponent["Color"].as<glm::vec4>();
				component.LineSpacing = textComponent["LineSpacing"].as<float>();
				component.Kerning = textComponent["Kerning"].as<float>();
				component.MaxWidth = textComponent["MaxWidth"].as<float>();
			}

			auto rigidBody2DComponent = entity["RigidBody2DComponent"];
			if (rigidBody2DComponent)
			{
				auto& component = deserializedEntity.AddComponent<RigidBody2DComponent>();
				component.BodyType = (RigidBody2DComponent::Type)rigidBody2DComponent["BodyType"].as<int>();
				component.FixedRotation = rigidBody2DComponent["FixedRotation"] ? rigidBody2DComponent["FixedRotation"].as<bool>() : false;
			}

			auto boxCollider2DComponent = entity["BoxCollider2DComponent"];
			if (boxCollider2DComponent)
			{
				auto& component = deserializedEntity.AddComponent<BoxCollider2DComponent>();
				component.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
				component.Size = boxCollider2DComponent["Size"].as<glm::vec2>();
				component.Density = boxCollider2DComponent["Density"] ? boxCollider2DComponent["Density"].as<float>() : 1.0f;
				component.Friction = boxCollider2DComponent["Friction"] ? boxCollider2DComponent["Friction"].as<float>() : 1.0f;
			}

			auto circleCollider2DComponent = entity["CircleCollider2DComponent"];
			if (circleCollider2DComponent)
			{
				auto& component = deserializedEntity.AddComponent<CircleCollider2DComponent>();
				component.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>();
				component.Radius = circleCollider2DComponent["Radius"].as<float>();
				component.Density = circleCollider2DComponent["Density"] ? circleCollider2DComponent["Density"].as<float>() : 1.0f;
				component.Friction = circleCollider2DComponent["Friction"] ? circleCollider2DComponent["Friction"].as<float>() : 1.0f;
			}

			auto rigidBodyComponent = entity["RigidBodyComponent"];
			if (rigidBodyComponent)
			{
				auto& component = deserializedEntity.AddComponent<RigidBodyComponent>();
				component.BodyType = (RigidBodyComponent::Type)rigidBodyComponent["BodyType"].as<int>(0);
				component.Mass = rigidBodyComponent["Mass"].as<float>(1.0f);
				component.LinearDrag = rigidBodyComponent["LinearDrag"].as<float>(0.0f);
				component.AngularDrag = rigidBodyComponent["AngularDrag"].as<float>(0.05f);
				component.DisableGravity = rigidBodyComponent["DisableGravity"].as<bool>(false);
				component.IsKinematic = rigidBodyComponent["IsKinematic"].as<bool>(false);
				component.LayerID = rigidBodyComponent["LayerID"].as<uint32_t>(0);

				component.CollisionDetection = (CollisionDetectionType)rigidBodyComponent["CollisionDetection"].as<uint32_t>(0);

				auto lockFlagsNode = rigidBodyComponent["LockFlags"];
				if (lockFlagsNode)
				{
					component.LockFlags = lockFlagsNode.as<uint8_t>(0);
				}
				else
				{
					component.LockFlags |= rigidBodyComponent["Constraints"]["LockPositionX"].as<bool>(false) ? (uint8_t)ActorLockFlag::TranslationX : 0;
					component.LockFlags |= rigidBodyComponent["Constraints"]["LockPositionY"].as<bool>(false) ? (uint8_t)ActorLockFlag::TranslationY : 0;
					component.LockFlags |= rigidBodyComponent["Constraints"]["LockPositionZ"].as<bool>(false) ? (uint8_t)ActorLockFlag::TranslationZ : 0;
					component.LockFlags |= rigidBodyComponent["Constraints"]["LockRotationX"].as<bool>(false) ? (uint8_t)ActorLockFlag::RotationX : 0;
					component.LockFlags |= rigidBodyComponent["Constraints"]["LockRotationY"].as<bool>(false) ? (uint8_t)ActorLockFlag::RotationY : 0;
					component.LockFlags |= rigidBodyComponent["Constraints"]["LockRotationZ"].as<bool>(false) ? (uint8_t)ActorLockFlag::RotationZ : 0;
				}
			}*/

			auto characterControllerComponent = entity["CharacterControllerComponent"];
			if (characterControllerComponent)
			{
				auto& component = deserializedEntity.AddComponent<CharacterControllerComponent>();
				component.LayerID = characterControllerComponent["Layer"].as<uint32_t>(0);
				component.DisableGravity = characterControllerComponent["DisableGravity"].as<bool>(false);
				component.SlopeLimitDeg = characterControllerComponent["SlopeLimit"].as<float>(0.0f);
				component.StepOffset = characterControllerComponent["StepOffset"].as<float>(0.0f);
			}

//			auto fixedJointComponent = entity["FixedJointComponent"];
//			if (fixedJointComponent)
//			{
//				auto& component = deserializedEntity.AddComponent<FixedJointComponent>();
//				component.ConnectedEntity = fixedJointComponent["ConnectedEntity"].as<UUID>(0);
//				component.IsBreakable = fixedJointComponent["IsBreakable"].as<bool>(true);
//				component.BreakForce = fixedJointComponent["BreakForce"].as<float>(100.0f);
//				component.BreakTorque = fixedJointComponent["BreakTorque"].as<float>(10.0f);
//				component.EnableCollision = fixedJointComponent["EnableCollision"].as<bool>(false);
//				component.EnablePreProcessing = fixedJointComponent["EnablePreProcessing"].as<bool>(true);
//			}
//
//			auto boxColliderComponent = entity["BoxColliderComponent"];
//			if (boxColliderComponent)
//			{
//				auto& component = deserializedEntity.AddComponent<BoxColliderComponent>();
//				component.Offset = boxColliderComponent["Offset"].as<glm::vec3>();
//				if (boxColliderComponent["Size"])
//					component.HalfSize = boxColliderComponent["Size"].as<glm::vec3>() / 2.0f;
//				else
//					component.HalfSize = boxColliderComponent["HalfSize"].as<glm::vec3>(glm::vec3(0.5f));
//				component.IsTrigger = boxColliderComponent["IsTrigger"] ? boxColliderComponent["IsTrigger"].as<bool>() : false;
//
//				auto material = boxColliderComponent["Material"];
//				if (material && AssetManager::IsAssetHandleValid(material.as<uint64_t>()))
//					component.Material = material.as<uint64_t>();
//			}
//
//			auto sphereColliderComponent = entity["SphereColliderComponent"];
//			if (sphereColliderComponent)
//			{
//				auto& component = deserializedEntity.AddComponent<SphereColliderComponent>();
//				component.Radius = sphereColliderComponent["Radius"].as<float>();
//				component.IsTrigger = sphereColliderComponent["IsTrigger"] ? sphereColliderComponent["IsTrigger"].as<bool>() : false;
//				component.Offset = sphereColliderComponent["Offset"].as<glm::vec3>(glm::vec3(0.0f));
//
//				auto material = sphereColliderComponent["Material"];
//				if (material && AssetManager::IsAssetHandleValid(material.as<uint64_t>()))
//					component.Material = material.as<uint64_t>();
//			}
//
//			auto capsuleColliderComponent = entity["CapsuleColliderComponent"];
//			if (capsuleColliderComponent)
//			{
//				auto& component = deserializedEntity.AddComponent<CapsuleColliderComponent>();
//				component.Radius = capsuleColliderComponent["Radius"].as<float>();
//				component.Height = capsuleColliderComponent["Height"].as<float>();
//				component.Offset = capsuleColliderComponent["Offset"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
//
//				component.IsTrigger = capsuleColliderComponent["IsTrigger"] ? capsuleColliderComponent["IsTrigger"].as<bool>() : false;
//
//				auto material = capsuleColliderComponent["Material"];
//				if (material && AssetManager::IsAssetHandleValid(material.as<uint64_t>()))
//					component.Material = material.as<uint64_t>();
//			}
//
//			auto meshColliderComponent = entity["MeshColliderComponent"];
//			if (meshColliderComponent)
//			{
//				auto& component = deserializedEntity.AddComponent<MeshColliderComponent>();
//
//				if (meshColliderComponent["ColliderHandle"])
//				{
//					component.ColliderAsset = meshColliderComponent["ColliderHandle"].as<AssetHandle>(0);
//					component.UseSharedShape = meshColliderComponent["UseSharedShape"].as<bool>(false);
//					component.OverrideMaterial = meshColliderComponent["OverrideMaterial"].as<AssetHandle>(0);
//					component.CollisionComplexity = (ECollisionComplexity)meshColliderComponent["CollisionComplexity"].as<uint8_t>(0);
//
//					if (deserializedEntity.HasComponent<MeshComponent>())
//					{
//						const auto& mc = deserializedEntity.GetComponent<MeshComponent>();
//						component.SubmeshIndex = mc.SubmeshIndex;
//					}
//
//					Ref<MeshColliderAsset> colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);
//
//					// Most likely a memory only asset from a previous session, re-create the asset
//					if (!colliderAsset)
//					{
//						if (deserializedEntity.HasComponent<MeshComponent>())
//						{
//							const auto& mc = deserializedEntity.GetComponent<MeshComponent>();
//							component.ColliderAsset = AssetManager::CreateMemoryOnlyAsset<MeshColliderAsset>(mc.Mesh);
//						}
//						else if (deserializedEntity.HasComponent<StaticMeshComponent>())
//						{
//							component.ColliderAsset = AssetManager::CreateMemoryOnlyAsset<MeshColliderAsset>(deserializedEntity.GetComponent<StaticMeshComponent>().StaticMesh);
//						}
//
//						colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);
//						colliderAsset->CollisionComplexity = component.CollisionComplexity;
//					}
//
//					if (colliderAsset && !PhysicsSystem::GetMeshCache().Exists(colliderAsset))
//						CookingFactory::CookMesh(colliderAsset);
//				}
//				else
//				{
//					AssetHandle colliderMesh = 0;
//
//					if (deserializedEntity.HasComponent<MeshComponent>())
//						colliderMesh = deserializedEntity.GetComponent<MeshComponent>().Mesh;
//					else if (deserializedEntity.HasComponent<StaticMeshComponent>())
//						colliderMesh = deserializedEntity.GetComponent<StaticMeshComponent>().StaticMesh;
//
//					bool overrideMesh = meshColliderComponent["OverrideMesh"].as<bool>(false);
//					if (overrideMesh)
//					{
//						AssetHandle tempHandle = meshColliderComponent["AssetID"].as<uint64_t>(0);
//						overrideMesh = AssetManager::IsAssetHandleValid(tempHandle);
//						colliderMesh = overrideMesh ? tempHandle : colliderMesh;
//					}
//
//					component.ColliderAsset = AssetManager::CreateMemoryOnlyAsset<MeshColliderAsset>(colliderMesh);
//
//					if (AssetManager::IsAssetHandleValid(component.ColliderAsset))
//						CookingFactory::CookMesh(component.ColliderAsset);
//					else
//						X2_CORE_WARN("MeshColliderComponent in use without valid mesh!");
//
//					auto material = meshColliderComponent["Material"];
//					if (material && AssetManager::IsAssetHandleValid(material.as<AssetHandle>(0)))
//						component.OverrideMaterial = material.as<AssetHandle>();
//				}
//
//				component.IsTrigger = meshColliderComponent["IsTrigger"].as<bool>(false);
//			}
//
//			// NOTE(Peter): This can probably be removed eventually
//			auto physicsLayerComponent = entity["PhysicsLayerComponent"];
//			if (physicsLayerComponent)
//			{
//				if (!deserializedEntity.HasComponent<RigidBodyComponent>() && deserializedEntity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>())
//					deserializedEntity.AddComponent<RigidBodyComponent>();
//
//				if (deserializedEntity.HasComponent<RigidBodyComponent>())
//					deserializedEntity.GetComponent<RigidBodyComponent>().LayerID = physicsLayerComponent["LayerID"].as<uint32_t>(0);
//			}
//
//			auto audioComponent = entity["AudioComponent"];
//			if (audioComponent)
//			{
//#define X2_DESERIALIZE_PROPERTY(propName, destination, inputData, defaultValue) destination = inputData[#propName] ? inputData[#propName].as<decltype(defaultValue)>() : defaultValue
//
//				auto& component = deserializedEntity.AddComponent<AudioComponent>();
//
//				X2_DESERIALIZE_PROPERTY(StartEvent, component.StartEvent, audioComponent, std::string(""));
//				component.StartCommandID = audioComponent["StartCommandID"] ? Audio::CommandID::FromUnsignedInt(audioComponent["StartCommandID"].as<uint32_t>()) : Audio::CommandID::InvalidID();
//
//				X2_DESERIALIZE_PROPERTY(PlayOnAwake, component.bPlayOnAwake, audioComponent, false);
//				X2_DESERIALIZE_PROPERTY(StopIfEntityDestroyed, component.bStopWhenEntityDestroyed, audioComponent, true);
//				X2_DESERIALIZE_PROPERTY(VolumeMultiplier, component.VolumeMultiplier, audioComponent, 1.0f);
//				X2_DESERIALIZE_PROPERTY(PitchMultiplier, component.PitchMultiplier, audioComponent, 1.0f);
//				X2_DESERIALIZE_PROPERTY(AutoDestroy, component.bAutoDestroy, audioComponent, false);
//			}
//
//			auto audioListener = entity["AudioListenerComponent"];
//			if (audioListener)
//			{
//				auto& component = deserializedEntity.AddComponent<AudioListenerComponent>();
//				component.Active = audioListener["Active"] ? audioListener["Active"].as<bool>() : false;
//				component.ConeInnerAngleInRadians = audioListener["ConeInnerAngle"] ? audioListener["ConeInnerAngle"].as<float>() : 6.283185f;
//				component.ConeOuterAngleInRadians = audioListener["ConeOuterAngle"] ? audioListener["ConeOuterAngle"].as<float>() : 6.283185f;
//				component.ConeOuterGain = audioListener["ConeOuterGain"] ? audioListener["ConeOuterGain"].as<float>() : 1.0f;
//#undef X2_DESERIALIZE_PROPERTY
//			}
		}

		scene->SortEntities();
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath);
		X2_CORE_ASSERT(stream);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		DeserializeFromYAML(strStream.str());

		// Asset handle
		const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(filepath);
		m_Scene->Handle = metadata.Handle;

		// NOTE(Peter): Fix for "UntitledScene" name, hardcoded which isn't good
		if (m_Scene->GetName() == "UntitledScene")
			m_Scene->SetName(Utils::RemoveExtension(filepath.filename().string()));

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::filesystem::path& filepath)
	{
		// Not implemented
		X2_CORE_ASSERT(false);
		return false;
	}

	bool SceneSerializer::SerializeToAssetPack(FileStreamWriter& stream, AssetSerializationInfo& outInfo)
	{
		YAML::Emitter out;
		SerializeToYAML(out);

		outInfo.Offset = stream.GetStreamPosition();
		std::string yamlString = out.c_str();
		stream.WriteString(yamlString);
		outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
		return true;
	}

	bool SceneSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::SceneInfo& sceneInfo)
	{
		stream.SetStreamPosition(sceneInfo.PackedOffset);
		std::string sceneYAML;
		stream.ReadString(sceneYAML);

		return DeserializeFromYAML(sceneYAML);
	}

	bool SceneSerializer::DeserializeReferencedPrefabs(const std::filesystem::path& filepath, std::unordered_set<AssetHandle>& outPrefabs)
	{
		std::ifstream stream(filepath);
		X2_CORE_ASSERT(stream);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Scene"])
			return false;

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				//auto scriptComponent = entity["ScriptComponent"];
				//auto storedFields = scriptComponent["StoredFields"];
				//if (storedFields)
				//{
				//	for (auto field : storedFields)
				//	{
				//		uint32_t id = field["ID"].as<uint32_t>(0);
				//		std::string name = field["Name"].as<std::string>();
				//		uint32_t type = field["Type"].as<uint32_t>();
				//		if (type == 18) // Prefab
				//		{
				//			uint64_t data = field["Data"].as<uint64_t>();
				//			if (AssetManager::IsAssetHandleValid((AssetHandle)data))
				//			{
				//				X2_CORE_INFO("Adding {}", name);
				//				outPrefabs.insert((AssetHandle)data);
				//			}
				//		}
				//	}
				//}
			}
		}
		return outPrefabs.size() > 0;
	}

}
