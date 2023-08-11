#pragma once

#include <unordered_map>

#include "AssetTypes.h"

namespace X2 {

	inline static std::unordered_map<std::string, AssetType> s_AssetExtensionMap =
	{
		// types
		{ ".xscene", AssetType::Scene },
		{ ".xmesh", AssetType::Mesh },
		{ ".xsmesh", AssetType::StaticMesh },
		{ ".xmaterial", AssetType::Material },
		{ ".xprefab", AssetType::Prefab },

		// mesh/animation source
		{ ".fbx", AssetType::MeshSource },
		{ ".gltf", AssetType::MeshSource },
		{ ".glb", AssetType::MeshSource },
		{ ".obj", AssetType::MeshSource },

		// Textures
		{ ".png", AssetType::Texture },
		{ ".jpg", AssetType::Texture },
		{ ".jpeg", AssetType::Texture },
		{ ".hdr", AssetType::EnvMap },

		// Fonts
		{ ".ttf", AssetType::Font },
		{ ".ttc", AssetType::Font },
		{ ".otf", AssetType::Font },

	};

}
