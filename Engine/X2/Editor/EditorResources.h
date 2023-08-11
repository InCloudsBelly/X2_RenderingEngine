#pragma once

#include "X2/Vulkan/VulkanTexture.h"
#include "X2/Utilities/FileSystem.h"

namespace X2 {

	class EditorResources
	{
	public:
		// Generic
		inline static Ref<VulkanTexture2D> GearIcon = nullptr;
		inline static Ref<VulkanTexture2D> PlusIcon = nullptr;
		inline static Ref<VulkanTexture2D> PencilIcon = nullptr;
		inline static Ref<VulkanTexture2D> ForwardIcon = nullptr;
		inline static Ref<VulkanTexture2D> BackIcon = nullptr;
		inline static Ref<VulkanTexture2D> PointerIcon = nullptr;
		inline static Ref<VulkanTexture2D> SearchIcon = nullptr;
		inline static Ref<VulkanTexture2D> ClearIcon = nullptr;
		inline static Ref<VulkanTexture2D> SaveIcon = nullptr;


		// Icons
		inline static Ref<VulkanTexture2D> AnimationIcon = nullptr;
		inline static Ref<VulkanTexture2D> AssetIcon = nullptr;
		inline static Ref<VulkanTexture2D> AudioIcon = nullptr;
		inline static Ref<VulkanTexture2D> AudioListenerIcon = nullptr;
		inline static Ref<VulkanTexture2D> BoxColliderIcon = nullptr;
		inline static Ref<VulkanTexture2D> BoxCollider2DIcon = nullptr;
		inline static Ref<VulkanTexture2D> CameraIcon = nullptr;
		inline static Ref<VulkanTexture2D> CapsuleColliderIcon = nullptr;
		inline static Ref<VulkanTexture2D> CharacterControllerIcon = nullptr;
		inline static Ref<VulkanTexture2D> CircleCollider2DIcon = nullptr;
		inline static Ref<VulkanTexture2D> DirectionalLightIcon = nullptr;
		inline static Ref<VulkanTexture2D> FixedJointIcon = nullptr;
		inline static Ref<VulkanTexture2D> MeshIcon = nullptr;
		inline static Ref<VulkanTexture2D> MeshColliderIcon = nullptr;
		inline static Ref<VulkanTexture2D> PointLightIcon = nullptr;
		inline static Ref<VulkanTexture2D> RigidBodyIcon = nullptr;
		inline static Ref<VulkanTexture2D> RigidBody2DIcon = nullptr;
		inline static Ref<VulkanTexture2D> ScriptIcon = nullptr;
		inline static Ref<VulkanTexture2D> SpriteIcon = nullptr;
		inline static Ref<VulkanTexture2D> SkyLightIcon = nullptr;
		inline static Ref<VulkanTexture2D> SphereColliderIcon = nullptr;
		inline static Ref<VulkanTexture2D> SpotLightIcon = nullptr;
		inline static Ref<VulkanTexture2D> StaticMeshIcon = nullptr;
		inline static Ref<VulkanTexture2D> TextIcon = nullptr;
		inline static Ref<VulkanTexture2D> TransformIcon = nullptr;

		// Viewport
		inline static Ref<VulkanTexture2D> PlayIcon = nullptr;
		inline static Ref<VulkanTexture2D> PauseIcon = nullptr;
		inline static Ref<VulkanTexture2D> StopIcon = nullptr;
		inline static Ref<VulkanTexture2D> SimulateIcon = nullptr;
		inline static Ref<VulkanTexture2D> MoveIcon = nullptr;
		inline static Ref<VulkanTexture2D> RotateIcon = nullptr;
		inline static Ref<VulkanTexture2D> ScaleIcon = nullptr;

		// Window
		inline static Ref<VulkanTexture2D> MinimizeIcon = nullptr;
		inline static Ref<VulkanTexture2D> MaximizeIcon = nullptr;
		inline static Ref<VulkanTexture2D> RestoreIcon = nullptr;
		inline static Ref<VulkanTexture2D> CloseIcon = nullptr;

		// Content Browser
		inline static Ref<VulkanTexture2D> FolderIcon = nullptr;
		inline static Ref<VulkanTexture2D> FileIcon = nullptr;
		inline static Ref<VulkanTexture2D> FBXFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> OBJFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> GLTFFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> GLBFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> WAVFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> MP3FileIcon = nullptr;
		inline static Ref<VulkanTexture2D> OGGFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> CSFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> PNGFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> JPGFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> MaterialFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> SceneFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> PrefabFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> FontFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> AnimationFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> AnimationControllerFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> MeshFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> StaticMeshFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> MeshColliderFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> PhysicsMaterialFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> SkeletonFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> SoundConfigFileIcon = nullptr;
		inline static Ref<VulkanTexture2D> SoundGraphFileIcon = nullptr;

		// Node Graph Editor
		inline static Ref<VulkanTexture2D> CompileIcon = nullptr;
		inline static Ref<VulkanTexture2D> PinValueConnectIcon = nullptr;
		inline static Ref<VulkanTexture2D> PinValueDisconnectIcon = nullptr;
		inline static Ref<VulkanTexture2D> PinFlowConnectIcon = nullptr;
		inline static Ref<VulkanTexture2D> PinFlowDisconnectIcon = nullptr;
		inline static Ref<VulkanTexture2D> PinAudioConnectIcon = nullptr;
		inline static Ref<VulkanTexture2D> PinAudioDisconnectIcon = nullptr;

		// Textures
		inline static Ref<VulkanTexture2D> X2LogoTexture = nullptr;
		inline static Ref<VulkanTexture2D> CheckerboardTexture = nullptr;
		inline static Ref<VulkanTexture2D> ShadowTexture = nullptr;
		inline static Ref<VulkanTexture2D> TranslucencyTexture = nullptr;

		static void Init()
		{
			TextureSpecification spec;
			spec.SamplerWrap = TextureWrap::Clamp;

			// Generic
			GearIcon = LoadTexture("Generic/Gear.png", "GearIcon", spec);
			PlusIcon = LoadTexture("Generic/Plus.png", "PlusIcon", spec);
			PencilIcon = LoadTexture("Generic/Pencil.png", "PencilIcon", spec);
			ForwardIcon = LoadTexture("Generic/Forward.png", "ForwardIcon", spec);
			BackIcon = LoadTexture("Generic/Back.png", "BackIcon", spec);
			PointerIcon = LoadTexture("Generic/Pointer.png", "PointerIcon", spec);
			SearchIcon = LoadTexture("Generic/Search.png", "SearchIcon", spec);
			ClearIcon = LoadTexture("Generic/Clear.png", "ClearIcon", spec);
			SaveIcon = LoadTexture("Generic/Save.png", "ClearIcon", spec);

			// Icons
			AnimationIcon = LoadTexture("Icons/Animation.png", "AnimationIcon", spec);
			AssetIcon = LoadTexture("Icons/Generic.png", "GenericIcon", spec);
			AudioIcon = LoadTexture("Icons/Audio.png", "AudioIcon", spec);
			AudioListenerIcon = LoadTexture("Icons/AudioListener.png", "AudioListenerIcon", spec);
			BoxColliderIcon = LoadTexture("Icons/BoxCollider.png", "BoxColliderIcon", spec);
			BoxCollider2DIcon = LoadTexture("Icons/BoxCollider2D.png", "BoxCollider2DIcon", spec);
			CameraIcon = LoadTexture("Icons/Camera.png", "CameraIcon", spec);
			CapsuleColliderIcon = LoadTexture("Icons/CapsuleCollider.png", "CapsuleColliderIcon", spec);
			CharacterControllerIcon = LoadTexture("Icons/CharacterController.png", "CharacterControllerIcon", spec);
			CircleCollider2DIcon = LoadTexture("Icons/CircleCollider2D.png", "CircleCollider2DIcon", spec);
			DirectionalLightIcon = LoadTexture("Icons/DirectionalLight.png", "DirectionalLightIcon", spec);
			FixedJointIcon = LoadTexture("Icons/FixedJoint.png", "FixedJointIcon", spec);
			MeshIcon = LoadTexture("Icons/Mesh.png", "MeshIcon", spec);
			MeshColliderIcon = LoadTexture("Icons/MeshCollider.png", "MeshColliderIcon", spec);
			PointLightIcon = LoadTexture("Icons/PointLight.png", "PointLightIcon", spec);
			RigidBodyIcon = LoadTexture("Icons/RigidBody.png", "RigidBodyIcon", spec);
			RigidBody2DIcon = LoadTexture("Icons/RigidBody2D.png", "RigidBody2DIcon", spec);
			ScriptIcon = LoadTexture("Icons/Script.png", "ScriptIcon", spec);
			SpriteIcon = LoadTexture("Icons/SpriteRenderer.png", "SpriteIcon", spec);
			SkyLightIcon = LoadTexture("Icons/SkyLight.png", "SkyLightIcon", spec);
			SphereColliderIcon = LoadTexture("Icons/SphereCollider.png", "SphereColliderIcon", spec);
			SpotLightIcon = LoadTexture("Icons/SpotLight.png", "SpotLightIcon", spec);
			StaticMeshIcon = LoadTexture("Icons/StaticMesh.png", "StaticMeshIcon", spec);
			TextIcon = LoadTexture("Icons/Text.png", "TextIcon", spec);
			TransformIcon = LoadTexture("Icons/Transform.png", "TransformIcon", spec);

			// Viewport
			PlayIcon = LoadTexture("Viewport/Play.png", "PlayIcon", spec);
			PauseIcon = LoadTexture("Viewport/Pause.png", "PauseIcon", spec);
			StopIcon = LoadTexture("Viewport/Stop.png", "StopIcon", spec);
			SimulateIcon = LoadTexture("Viewport/Simulate.png", "SimulateIcon", spec);
			MoveIcon = LoadTexture("Viewport/MoveTool.png", "MoveIcon", spec);
			RotateIcon = LoadTexture("Viewport/RotateTool.png", "RotateIcon", spec);
			ScaleIcon = LoadTexture("Viewport/ScaleTool.png", "ScaleIcon", spec);

			// Window
			MinimizeIcon = LoadTexture("Window/Minimize.png", "MinimizeIcon", spec);
			MaximizeIcon = LoadTexture("Window/Maximize.png", "MaximizeIcon", spec);
			RestoreIcon = LoadTexture("Window/Restore.png", "RestoreIcon", spec);
			CloseIcon = LoadTexture("Window/Close.png", "CloseIcon", spec);

			// Content Browser
			FolderIcon = LoadTexture("ContentBrowser/Folder.png", "FolderIcon", spec);
			FileIcon = LoadTexture("ContentBrowser/File.png", "FileIcon", spec);
			FBXFileIcon = LoadTexture("ContentBrowser/FBX.png", "FBXFileIcon", spec);
			OBJFileIcon = LoadTexture("ContentBrowser/OBJ.png", "OBJFileIcon", spec);
			GLTFFileIcon = LoadTexture("ContentBrowser/GLTF.png", "GLTFFileIcon", spec);
			GLBFileIcon = LoadTexture("ContentBrowser/GLB.png", "GLBFileIcon", spec);
			WAVFileIcon = LoadTexture("ContentBrowser/WAV.png", "WAVFileIcon", spec);
			MP3FileIcon = LoadTexture("ContentBrowser/MP3.png", "MP3FileIcon", spec);
			OGGFileIcon = LoadTexture("ContentBrowser/OGG.png", "OGGFileIcon", spec);
			CSFileIcon = LoadTexture("ContentBrowser/CS.png", "CSFileIcon", spec);
			PNGFileIcon = LoadTexture("ContentBrowser/PNG.png", "PNGFileIcon", spec);
			JPGFileIcon = LoadTexture("ContentBrowser/JPG.png", "JPGFileIcon", spec);
			MaterialFileIcon = LoadTexture("ContentBrowser/Material.png", "MaterialFileIcon", spec);
			SceneFileIcon = LoadTexture("X_logo_white_100.png", "SceneFileIcon", spec);
			PrefabFileIcon = LoadTexture("ContentBrowser/Prefab.png", "PrefabFileIcon", spec);
			FontFileIcon = LoadTexture("ContentBrowser/Font.png", "FontFileIcon", spec);
			AnimationFileIcon = LoadTexture("ContentBrowser/Animation.png", "AnimationFileIcon", spec);
			AnimationControllerFileIcon = LoadTexture("ContentBrowser/AnimationController.png", "AnimationControllerFileIcon", spec);
			MeshFileIcon = LoadTexture("ContentBrowser/Mesh.png", "MeshFileIcon", spec);
			StaticMeshFileIcon = LoadTexture("ContentBrowser/StaticMesh.png", "StaticMeshFileIcon", spec);
			MeshColliderFileIcon = LoadTexture("ContentBrowser/MeshCollider.png", "MeshColliderFileIcon", spec);
			PhysicsMaterialFileIcon = LoadTexture("ContentBrowser/PhysicsMaterial.png", "PhysicsMaterialFileIcon", spec);
			SkeletonFileIcon = LoadTexture("ContentBrowser/Skeleton.png", "SkeletonFileIcon", spec);
			SoundConfigFileIcon = LoadTexture("ContentBrowser/SoundConfig.png", "SoundConfigFileIcon", spec);
			SoundGraphFileIcon = LoadTexture("ContentBrowser/SoundGraph.png", "SoundGraphFileIcon", spec);

			// Node Graph
			CompileIcon = LoadTexture("NodeGraph/Compile.png");
			PinValueConnectIcon = LoadTexture("NodeGraph/Pins/ValueConnect.png");
			PinValueDisconnectIcon = LoadTexture("NodeGraph/Pins/ValueDisconnect.png");
			PinFlowConnectIcon = LoadTexture("NodeGraph/Pins/FlowConnect.png");
			PinFlowDisconnectIcon = LoadTexture("NodeGraph/Pins/FlowDisconnect.png");
			PinAudioConnectIcon = LoadTexture("NodeGraph/Pins/AudioConnect.png");
			PinAudioDisconnectIcon = LoadTexture("NodeGraph/Pins/AudioDisconnect.png");

			// Textures
			X2LogoTexture = LoadTexture("X_logo_white_50.png");
			CheckerboardTexture = LoadTexture("Checkerboard.tga");
			ShadowTexture = LoadTexture("Panels/Shadow.png", "ShadowTexture", spec);
			TranslucencyTexture = LoadTexture("Panels/Translucency.png", "TranslucencyTexture", spec);
		}

		static void Shutdown()
		{
			// Generic
			GearIcon.Reset();
			PlusIcon.Reset();
			PencilIcon.Reset();
			ForwardIcon.Reset();
			BackIcon.Reset();
			PointerIcon.Reset();
			SearchIcon.Reset();
			ClearIcon.Reset();
			SaveIcon.Reset();

			// Icons
			AnimationIcon.Reset();
			AssetIcon.Reset();
			AudioIcon.Reset();
			AudioListenerIcon.Reset();
			BoxColliderIcon.Reset();
			BoxCollider2DIcon.Reset();
			CameraIcon.Reset();
			CapsuleColliderIcon.Reset();
			CharacterControllerIcon.Reset();
			CircleCollider2DIcon.Reset();
			DirectionalLightIcon.Reset();
			FixedJointIcon.Reset();
			MeshIcon.Reset();
			MeshColliderIcon.Reset();
			PointLightIcon.Reset();
			RigidBodyIcon.Reset();
			RigidBody2DIcon.Reset();
			ScriptIcon.Reset();
			SpriteIcon.Reset();
			SkyLightIcon.Reset();
			SphereColliderIcon.Reset();
			SpotLightIcon.Reset();
			StaticMeshIcon.Reset();
			TextIcon.Reset();
			TransformIcon.Reset();

			// Viewport
			PlayIcon.Reset();
			PauseIcon.Reset();
			StopIcon.Reset();
			SimulateIcon.Reset();
			MoveIcon.Reset();
			RotateIcon.Reset();
			ScaleIcon.Reset();

			// Window
			MinimizeIcon.Reset();
			MaximizeIcon.Reset();
			RestoreIcon.Reset();
			CloseIcon.Reset();

			// Content Browser
			FolderIcon.Reset();
			FileIcon.Reset();
			FBXFileIcon.Reset();
			OBJFileIcon.Reset();
			GLTFFileIcon.Reset();
			GLBFileIcon.Reset();
			WAVFileIcon.Reset();
			MP3FileIcon.Reset();
			OGGFileIcon.Reset();
			CSFileIcon.Reset();
			PNGFileIcon.Reset();
			JPGFileIcon.Reset();
			MaterialFileIcon.Reset();
			SceneFileIcon.Reset();
			PrefabFileIcon.Reset();
			FontFileIcon.Reset();
			AnimationFileIcon.Reset();
			AnimationControllerFileIcon.Reset();
			MeshFileIcon.Reset();
			StaticMeshFileIcon.Reset();
			MeshColliderFileIcon.Reset();
			PhysicsMaterialFileIcon.Reset();
			SkeletonFileIcon.Reset();
			SoundConfigFileIcon.Reset();
			SoundGraphFileIcon.Reset();

			// Node Graph
			CompileIcon.Reset();
			PinValueConnectIcon.Reset();
			PinValueDisconnectIcon.Reset();
			PinFlowConnectIcon.Reset();
			PinFlowDisconnectIcon.Reset();
			PinAudioConnectIcon.Reset();
			PinAudioDisconnectIcon.Reset();

			// Textures
			X2LogoTexture.Reset();
			CheckerboardTexture.Reset();
			ShadowTexture.Reset();
			TranslucencyTexture.Reset();
		}

	private:
		static Ref<VulkanTexture2D> LoadTexture(const std::filesystem::path& relativePath, TextureSpecification specification = TextureSpecification())
		{
			return LoadTexture(relativePath, "", specification);
		}

		static Ref<VulkanTexture2D> LoadTexture(const std::filesystem::path& relativePath, const std::string& name, TextureSpecification specification)
		{
			specification.DebugName = name;

			std::filesystem::path path = PROJECT_ROOT/std::filesystem::path("Resources/Editor")/relativePath;

			if (!FileSystem::Exists(path))
			{
				X2_CORE_FATAL("Failed to load icon {0}! The file doesn't exist.", path.string());
				X2_CORE_VERIFY(false);
				return nullptr;
			}

			return Ref<VulkanTexture2D>::Create(specification, path.string());
		}
	};

}
