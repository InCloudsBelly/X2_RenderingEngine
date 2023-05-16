#include "Mesh.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Command/Semaphore.h"

//#include <Utils/Log.h>
#include "Utils/OrientedBoundingBox.h"
#include <rttr/registration>

RTTR_REGISTRATION
{
    rttr::registration::class_<Mesh>("Mesh");
}

void Mesh::onLoad(CommandBuffer* transferCommandBuffer)
{
    std::vector<glm::vec3> vertexPositions = std::vector<glm::vec3>();

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(getPath(), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw(std::runtime_error(importer.GetErrorString()));

    // process ASSIMP's root node recursively
    aiMesh* mesh = scene->mMeshes[scene->mRootNode->mMeshes[0]];
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        VertexData vertexData;
        glm::vec3 vector;
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertexData.position = vector;
        // normals
        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertexData.normal = vector;
        }
        // texture coordinates
        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertexData.texCoords = vec;
            // tangent
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertexData.tangent = vector;
            // bitangent
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertexData.bitangent = vector;
        }
        else
        {
            throw(std::runtime_error("Mesh do not contains uv."));
        }

        m_vertices.push_back(vertexData);
        vertexPositions.emplace_back(vertexData.position);
    }
    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            m_indices.push_back(face.mIndices[j]);
    }
    importer.FreeScene();

    VkDeviceSize vertexBufferSize = sizeof(VertexData) * m_vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();

    Buffer stageVertexBuffer = Buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stageVertexBuffer.WriteData(m_vertices.data(), vertexBufferSize);
    m_vertexBuffer = new Buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer stageIndexBuffer = Buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stageIndexBuffer.WriteData(m_indices.data(), indexBufferSize);
    m_indexBuffer = new Buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


    transferCommandBuffer->reset();
    transferCommandBuffer->beginRecord(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    transferCommandBuffer->copyBuffer(&stageVertexBuffer, m_vertexBuffer);
    transferCommandBuffer->copyBuffer(&stageIndexBuffer, m_indexBuffer);
    transferCommandBuffer->endRecord();
    transferCommandBuffer->submit();

    m_orientedBoundingBox = new OrientedBoundingBox(vertexPositions);

    transferCommandBuffer->waitForFinish();
    transferCommandBuffer->reset();

}

Mesh::Mesh()
    : AssetBase(true)
    , m_orientedBoundingBox(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_vertices()
    , m_indices()
{
}

Mesh::Mesh(CommandBuffer* transferCommandBuffer, std::vector<VertexData> vertices, std::vector<uint32_t> indices)
    : AssetBase(false)
    , m_orientedBoundingBox(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_vertices(vertices)
    , m_indices(indices)
{
    VkDeviceSize vertexBufferSize = sizeof(VertexData) * m_vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();

    Buffer stageVertexBuffer = Buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stageVertexBuffer.WriteData(m_vertices.data(), vertexBufferSize);
    m_vertexBuffer = new Buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer stageIndexBuffer = Buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stageIndexBuffer.WriteData(m_indices.data(), indexBufferSize);
    m_indexBuffer = new Buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


    transferCommandBuffer->reset();
    transferCommandBuffer->beginRecord(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    transferCommandBuffer->copyBuffer(&stageVertexBuffer, m_vertexBuffer);
    transferCommandBuffer->copyBuffer(&stageIndexBuffer, m_indexBuffer);
    transferCommandBuffer->endRecord();
    transferCommandBuffer->submit({}, {}, {});

    std::vector<glm::vec3> vertexPositions = std::vector<glm::vec3>(vertices.size());
    for (unsigned int i = 0; i < vertices.size(); i++)
    {
        vertexPositions[i] = vertices[i].position;
    }

    m_orientedBoundingBox = new OrientedBoundingBox(vertexPositions);

    transferCommandBuffer->waitForFinish();
    transferCommandBuffer->reset();

}

Mesh::~Mesh()
{
    delete m_orientedBoundingBox;
    delete m_vertexBuffer;
    delete m_indexBuffer;
}

Buffer& Mesh::getVertexBuffer()
{
    return *m_vertexBuffer;
}

Buffer& Mesh::getIndexBuffer()
{
    return *m_indexBuffer;
}

std::vector<VertexData>& Mesh::getVertices()
{
    return m_vertices;
}

std::vector<uint32_t>& Mesh::getIndices()
{
    return m_indices;
}

OrientedBoundingBox& Mesh::getOrientedBoundingBox()
{
    return *m_orientedBoundingBox;
}
