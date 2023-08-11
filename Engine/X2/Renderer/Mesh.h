#pragma once

//#include "Hazel/Animation/Animation.h"
//#include "Hazel/Animation/Skeleton.h"

#include "X2/Asset/Asset.h"

#include "X2/Math/AABB.h"

#include "MaterialAsset.h"

#include "X2/Vulkan/VulkanMaterial.h"
#include "X2/Vulkan/VulkanIndexBuffer.h"
#include "X2/Vulkan/VulkanUniformBuffer.h"
#include "X2/Vulkan/VulkanVertexBuffer.h"

#include "X2/Serialization/FileStream.h"

#include <vector>
#include <glm/glm.hpp>

namespace X2 {

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec3 Binormal;
		glm::vec2 Texcoord;
	};

	struct BoneInfo
	{
		glm::mat4 SubMeshInverseTransform;
		glm::mat4 InverseBindPose;
		uint32_t SubMeshIndex;
		uint32_t BoneIndex;

		BoneInfo() = default;
		BoneInfo(glm::mat4 subMeshInverseTransform, glm::mat4 inverseBindPose, uint32_t subMeshIndex, uint32_t boneIndex)
			: SubMeshInverseTransform(subMeshInverseTransform)
			, InverseBindPose(inverseBindPose)
			, SubMeshIndex(subMeshIndex)
			, BoneIndex(boneIndex)
		{}

		static void Serialize(StreamWriter* serializer, const BoneInfo& instance)
		{
			serializer->WriteRaw(instance);
		}

		static void Deserialize(StreamReader* deserializer, BoneInfo& instance)
		{
			deserializer->ReadRaw(instance);
		}
	};

	struct BoneInfluence
	{
		uint32_t BoneInfoIndices[4] = { 0, 0, 0, 0 };
		float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		void AddBoneData(uint32_t boneInfoIndex, float weight)
		{
			if (weight < 0.0f || weight > 1.0f)
			{
				X2_CORE_WARN("Vertex bone weight is out of range. We will clamp it to [0, 1] (BoneID={0}, Weight={1})", boneInfoIndex, weight);
				weight = std::clamp(weight, 0.0f, 1.0f);
			}
			if (weight > 0.0f)
			{
				for (size_t i = 0; i < 4; i++)
				{
					if (Weights[i] == 0.0f)
					{
						BoneInfoIndices[i] = boneInfoIndex;
						Weights[i] = weight;
						return;
					}
				}

				// Note: when importing from assimp we are passing aiProcess_LimitBoneWeights which automatically keeps only the top N (where N defaults to 4)
				//       bone weights (and normalizes the sum to 1), which is exactly what we want.
				//       So, we should never get here.
				X2_CORE_WARN("Vertex has more than four bones affecting it, extra bone influences will be discarded (BoneID={0}, Weight={1})", boneInfoIndex, weight);
			}
		}

		void NormalizeWeights()
		{
			float sumWeights = 0.0f;
			for (size_t i = 0; i < 4; i++)
			{
				sumWeights += Weights[i];
			}
			if (sumWeights > 0.0f)
			{
				for (size_t i = 0; i < 4; i++)
				{
					Weights[i] /= sumWeights;
				}
			}
		}

		static void Serialize(StreamWriter* serializer, const BoneInfluence& instance)
		{
			serializer->WriteRaw(instance);
		}

		static void Deserialize(StreamReader* deserializer, BoneInfluence& instance)
		{
			deserializer->ReadRaw(instance);
		}
	};

	static const int NumAttributes = 5;

	struct Index
	{
		uint32_t V1, V2, V3;
	};

	static_assert(sizeof(Index) == 3 * sizeof(uint32_t));

	struct Triangle
	{
		Vertex V0, V1, V2;

		Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
			: V0(v0), V1(v1), V2(v2) {}
	};

	class Submesh
	{
	public:
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t MaterialIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;

		glm::mat4 Transform{ 1.0f }; // World transform
		glm::mat4 LocalTransform{ 1.0f };
		AABB BoundingBox;

		std::string NodeName, MeshName;
		bool IsRigged = false;

		static void Serialize(StreamWriter* serializer, const Submesh& instance)
		{
			serializer->WriteRaw(instance.BaseVertex);
			serializer->WriteRaw(instance.BaseIndex);
			serializer->WriteRaw(instance.MaterialIndex);
			serializer->WriteRaw(instance.IndexCount);
			serializer->WriteRaw(instance.VertexCount);
			serializer->WriteRaw(instance.Transform);
			serializer->WriteRaw(instance.LocalTransform);
			serializer->WriteRaw(instance.BoundingBox);
			serializer->WriteString(instance.NodeName);
			serializer->WriteString(instance.MeshName);
			serializer->WriteRaw(instance.IsRigged);
		}

		static void Deserialize(StreamReader* deserializer, Submesh& instance)
		{
			deserializer->ReadRaw(instance.BaseVertex);
			deserializer->ReadRaw(instance.BaseIndex);
			deserializer->ReadRaw(instance.MaterialIndex);
			deserializer->ReadRaw(instance.IndexCount);
			deserializer->ReadRaw(instance.VertexCount);
			deserializer->ReadRaw(instance.Transform);
			deserializer->ReadRaw(instance.LocalTransform);
			deserializer->ReadRaw(instance.BoundingBox);
			deserializer->ReadString(instance.NodeName);
			deserializer->ReadString(instance.MeshName);
			deserializer->ReadRaw(instance.IsRigged);
		}
	};

	struct MeshNode
	{
		uint32_t Parent = 0xffffffff;
		std::vector<uint32_t> Children;
		std::vector<uint32_t> Submeshes;

		std::string Name;
		glm::mat4 LocalTransform;

		inline bool IsRoot() const { return Parent == 0xffffffff; }

		static void Serialize(StreamWriter* serializer, const MeshNode& instance)
		{
			serializer->WriteRaw(instance.Parent);
			serializer->WriteArray(instance.Children);
			serializer->WriteArray(instance.Submeshes);
			serializer->WriteString(instance.Name);
			serializer->WriteRaw(instance.LocalTransform);
		}

		static void Deserialize(StreamReader* deserializer, MeshNode& instance)
		{
			deserializer->ReadRaw(instance.Parent);
			deserializer->ReadArray(instance.Children);
			deserializer->ReadArray(instance.Submeshes);
			deserializer->ReadString(instance.Name);
			deserializer->ReadRaw(instance.LocalTransform);
		}
	};

	//
	// MeshSource is a representation of an actual asset file on disk
	// Meshes are created from MeshSource
	//
	class MeshSource : public Asset
	{
	public:
		MeshSource() = default;
		MeshSource(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const glm::mat4& transform);
		MeshSource(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const std::vector<Submesh>& submeshes);
		virtual ~MeshSource();

		void DumpVertexBuffer();

		std::vector<Submesh>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }

		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<Index>& GetIndices() const { return m_Indices; }

		bool IsSubmeshRigged(uint32_t submeshIndex) const { return m_Submeshes[submeshIndex].IsRigged; }


		std::vector<Ref<VulkanMaterial>>& GetMaterials() { return m_Materials; }
		const std::vector<Ref<VulkanMaterial>>& GetMaterials() const { return m_Materials; }
		const std::string& GetFilePath() const { return m_FilePath; }

		const std::vector<Triangle> GetTriangleCache(uint32_t index) const { return m_TriangleCache.at(index); }

		Ref<VulkanVertexBuffer> GetVertexBuffer() { return m_VertexBuffer; }
		Ref<VulkanIndexBuffer> GetIndexBuffer() { return m_IndexBuffer; }

		static AssetType GetStaticType() { return AssetType::MeshSource; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

		const AABB& GetBoundingBox() const { return m_BoundingBox; }

		const MeshNode& GetRootNode() const { return m_Nodes[0]; }
		const std::vector<MeshNode>& GetNodes() const { return m_Nodes; }
	private:
		std::vector<Submesh> m_Submeshes;

		Ref<VulkanVertexBuffer> m_VertexBuffer;
		Ref<VulkanIndexBuffer> m_IndexBuffer;

		std::vector<Vertex> m_Vertices;
		std::vector<Index> m_Indices;


		std::vector<Ref<VulkanMaterial>> m_Materials;

		std::unordered_map<uint32_t, std::vector<Triangle>> m_TriangleCache;

		AABB m_BoundingBox;

		std::string m_FilePath;

		std::vector<MeshNode> m_Nodes;

		// TEMP
		bool m_Runtime = false;

		friend class Scene;
		friend class SceneRenderer;
		friend class Renderer;
		friend class VulkanRenderer;
		friend class SceneHierarchyPanel;
		friend class MeshViewerPanel;
		friend class Mesh;
		friend class AssimpMeshImporter;
		friend class MeshRuntimeSerializer;
	};

	// Dynamic Mesh - supports skeletal animation and retains hierarchy
	class Mesh : public Asset
	{
	public:
		explicit Mesh(Ref<MeshSource> meshSource);
		Mesh(Ref<MeshSource> meshSource, const std::vector<uint32_t>& submeshes);
		Mesh(const Ref<Mesh>& other);
		virtual ~Mesh();

		//bool HasSkeleton() { return m_MeshSource && m_MeshSource->HasSkeleton(); }

		std::vector<uint32_t>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<uint32_t>& GetSubmeshes() const { return m_Submeshes; }

		// Pass in an empty vector to set ALL submeshes for MeshSource
		void SetSubmeshes(const std::vector<uint32_t>& submeshes);

		Ref<MeshSource> GetMeshSource() { return m_MeshSource; }
		Ref<MeshSource> GetMeshSource() const { return m_MeshSource; }
		void SetMeshAsset(Ref<MeshSource> meshSource) { m_MeshSource = meshSource; }

		Ref<MaterialTable> GetMaterials() const { return m_Materials; }

		static AssetType GetStaticType() { return AssetType::Mesh; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		Ref<MeshSource> m_MeshSource;
		std::vector<uint32_t> m_Submeshes; // TODO(Yan): physics/render masks

		// Materials
		Ref<MaterialTable> m_Materials;

		friend class Scene;
		friend class Renderer;
		friend class VulkanRenderer;
		friend class OpenGLRenderer;
		friend class SceneHierarchyPanel;
		friend class MeshViewerPanel;
	};

	// Static Mesh - no skeletal animation, flattened hierarchy
	class StaticMesh : public Asset
	{
	public:
		explicit StaticMesh(Ref<MeshSource> meshSource);
		StaticMesh(Ref<MeshSource> meshSource, const std::vector<uint32_t>& submeshes);
		StaticMesh(const Ref<StaticMesh>& other);
		virtual ~StaticMesh();

		std::vector<uint32_t>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<uint32_t>& GetSubmeshes() const { return m_Submeshes; }

		// Pass in an empty vector to set ALL submeshes for MeshSource
		void SetSubmeshes(const std::vector<uint32_t>& submeshes);

		Ref<MeshSource> GetMeshSource() { return m_MeshSource; }
		Ref<MeshSource> GetMeshSource() const { return m_MeshSource; }
		void SetMeshAsset(Ref<MeshSource> meshAsset) { m_MeshSource = meshAsset; }

		Ref<MaterialTable> GetMaterials() const { return m_Materials; }

		static AssetType GetStaticType() { return AssetType::StaticMesh; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }
	private:
		Ref<MeshSource> m_MeshSource;
		std::vector<uint32_t> m_Submeshes; // TODO(Yan): physics/render masks

		// Materials
		Ref<MaterialTable> m_Materials;

		friend class Scene;
		friend class Renderer;
		friend class VulkanRenderer;
		friend class SceneHierarchyPanel;
		friend class MeshViewerPanel;
	};

}
