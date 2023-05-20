#pragma once
#include "Core/IO/Asset/AssetBase.h"
#include <glm/glm.hpp>
#include <vector>
#include <rttr/type>

class OrientedBoundingBox;

class Buffer;

class CommandBuffer;

struct VertexData
{
	glm::vec3 position;
	glm::vec2 texCoords;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;
};

class Mesh final : public AssetBase
{
	friend class Model;
private:
	std::vector<VertexData> m_vertices;
	std::vector<uint32_t> m_indices;
	Buffer* m_vertexBuffer;
	Buffer* m_indexBuffer;
	OrientedBoundingBox* m_orientedBoundingBox;
	void onLoad(CommandBuffer* transferCommandBuffer)override;
public:
	Mesh();
	Mesh(CommandBuffer* transferCommandBuffer, std::vector<VertexData> vertices, std::vector<uint32_t> indices);
	~Mesh();
	Buffer& getVertexBuffer();
	Buffer& getIndexBuffer();
	std::vector<VertexData>& getVertices();
	std::vector<uint32_t>& getIndices();
	OrientedBoundingBox& getOrientedBoundingBox();

	RTTR_ENABLE(AssetBase)
};