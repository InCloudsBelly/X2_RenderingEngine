#include "Model.h"
#include "Mesh.h"

#include "Core/Graphic/Command/CommandBuffer.h"
#include <glm/glm.hpp>

#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Command/Semaphore.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/IO/Manager/AssetManager.h"

#include "Core/Graphic/Instance/Image.h"

//#include <Utils/Log.h>
#include "Utils/OrientedBoundingBox.h"
#include <rttr/registration>

RTTR_REGISTRATION
{
    rttr::registration::class_<Model>("Model");
}

static std::unordered_map<aiTextureType, std::string> defaultTexturePathMap{
    {aiTextureType_DIFFUSE,std::string(MODEL_DIR) + "defaultTexture/DefaultTexture.png" },
    {aiTextureType_UNKNOWN, std::string(MODEL_DIR) + "defaultTexture/DefaultMetallicRoughness.png"},
    {aiTextureType_EMISSIVE, std::string(MODEL_DIR) + "defaultTexture/DefaultAmbientOcclusion.png"},
    {aiTextureType_LIGHTMAP, std::string(MODEL_DIR) + "defaultTexture/DefaultEmissiveColor.png"},
    {aiTextureType_NORMALS,std::string(MODEL_DIR) + "defaultTexture/DefaultNormal.png" }
};



Image* Model::getTextureByType(aiMaterial* material, aiTextureType type)
{
    std::string texPath;
    
    std::size_t found = getPath().find_last_of("/\\");
    std::string dirPath = getPath().substr(0, found);

    if (material->GetTextureCount(type) > 0)
    {
        aiString str;
        material->GetTexture(type, 0, &str);
        texPath = dirPath + "/" + str.C_Str();
    }
    else
        texPath = defaultTexturePathMap[type];

    return Instance::getAssetManager()->load<Image>(texPath);
}

void Model::processNode(CommandBuffer * transferCommandBuffer, aiNode * node, const aiScene * scene)
{
    // Processes all the node's meshes(if any).
    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];

        std::vector<VertexData> vertices;
        std::vector<uint32_t> indices;

        for (uint32_t i = 0; i < aiMesh->mNumVertices; i++)
        {
            VertexData vertex{};

            //Position
            vertex.position = { aiMesh->mVertices[i].x,aiMesh->mVertices[i].y,aiMesh->mVertices[i].z };
            if (aiMesh->mNormals != NULL)
                vertex.normal = glm::normalize(glm::fvec3(aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z));
            else
                throw std::runtime_error("Mesh doesn't have normals!");


            //TexCoords
            if (aiMesh->mTextureCoords[0] != NULL)
                vertex.texCoords = { aiMesh->mTextureCoords[0][i].x,aiMesh->mTextureCoords[0][i].y, };
            else
                vertex.texCoords = glm::fvec3(1.0f);

            //Tangents
            if (aiMesh->mTangents != NULL)
                vertex.tangent = glm::normalize(glm::fvec3(aiMesh->mTangents[i].x, aiMesh->mTangents[i].y, aiMesh->mTangents[i].z));
            else
                vertex.tangent = glm::fvec3(1.0f);


            if (aiMesh->mBitangents != NULL)
                vertex.bitangent = glm::normalize(glm::fvec3(aiMesh->mBitangents[i].x, aiMesh->mBitangents[i].y, aiMesh->mBitangents[i].z));
            else
                vertex.bitangent = glm::fvec3(1.0f);

            vertices.emplace_back(vertex);
        }

        for (uint32_t i = 0; i < aiMesh->mNumFaces; i++)
        {
            auto face = aiMesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; j++)
                indices.emplace_back(face.mIndices[j]);
        }

        Mesh* newMesh = new Mesh(transferCommandBuffer, vertices, indices);

        aiMaterial* aiMaterial = scene->mMaterials[aiMesh->mMaterialIndex];
       

        PBR_Textures* pbrTextures = new PBR_Textures;
        {
            pbrTextures->albedo              = getTextureByType(aiMaterial, aiTextureType_DIFFUSE);
            pbrTextures->metallicRoughness   = getTextureByType(aiMaterial, aiTextureType_UNKNOWN);
            pbrTextures->emissive            = getTextureByType(aiMaterial, aiTextureType_EMISSIVE);
            pbrTextures->ao                  = getTextureByType(aiMaterial, aiTextureType_LIGHTMAP);
            pbrTextures->normal              = getTextureByType(aiMaterial, aiTextureType_NORMALS);
        }

        m_meshTextureMap.insert(std::pair< Mesh*, PBR_Textures*>(newMesh, pbrTextures));
    }
    // Processes all the node's childrens(if any).
    for (uint32_t i = 0; i < node->mNumChildren; i++)
        processNode(transferCommandBuffer, node->mChildren[i], scene);
}


void Model::onLoad(CommandBuffer* transferCommandBuffer)
{
    unsigned int flags = (aiProcess_Triangulate | aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace | aiProcess_PreTransformVertices);

    Assimp::Importer importer;
    auto* scene = importer.ReadFile(getPath(), flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        throw std::runtime_error("ERROR::ASSIMP::" + std::string(importer.GetErrorString()));
    }
    processNode(transferCommandBuffer, scene->mRootNode, scene);
}

Model::~Model()
{
    for (auto pair : m_meshTextureMap)
    {
        delete pair.first;

        Instance::getAssetManager()->unload(pair.second->albedo);
        Instance::getAssetManager()->unload(pair.second->metallicRoughness);
        Instance::getAssetManager()->unload(pair.second->emissive);
        Instance::getAssetManager()->unload(pair.second->ao);
        Instance::getAssetManager()->unload(pair.second->normal);
    }
}