#pragma once
#include "Core/IO/Asset/AssetBase.h"
#include "Core/Logic/Object/GameObject.h"
#include <glm/glm.hpp>
#include <vector>
#include <rttr/type>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <unordered_map>

class OrientedBoundingBox;

class Buffer;

class CommandBuffer;

class Mesh;

class Image;

struct PBR_Textures
{
	Image* albedo;
	Image* metallicRoughness;
	Image* emissive;
	Image* ao;
	Image* normal;

};

class Model final : public AssetBase 
{
private:
	Image* getTextureByType(aiMaterial* material, aiTextureType type);
	void processNode(CommandBuffer* transferCommandBuffer, aiNode* node, const aiScene* scene);
	void onLoad(CommandBuffer* transferCommandBuffer)override;

public:
	std::unordered_map<Mesh*, PBR_Textures*> m_meshTextureMap;

	Model() {};
	~Model();

	RTTR_ENABLE(AssetBase)
};