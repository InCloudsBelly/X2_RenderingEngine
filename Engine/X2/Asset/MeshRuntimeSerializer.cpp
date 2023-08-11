#include "Precompiled.h"
#include "MeshRuntimeSerializer.h"

#include "MeshSourceFile.h"

#include "X2/Asset/AssetManager.h"
#include "X2/Renderer/Renderer.h"
#include "X2/Asset/AssimpMeshImporter.h"

namespace X2 {

	struct MeshMaterial
	{
		std::string MaterialName;
		std::string ShaderName;

		glm::vec3 AlbedoColor;
		float Emission;
		float Metalness;
		float Roughness;
		bool UseNormalMap;

		uint64_t AlbedoTexture;
		uint64_t NormalTexture;
		uint64_t MetallicRoughnessTexture;
		uint64_t EmissionTexture;

		//uint64_t MetalnessTexture;
		//uint64_t RoughnessTexture;

		static void Serialize(StreamWriter* serializer, const MeshMaterial& instance)
		{
			serializer->WriteString(instance.MaterialName);
			serializer->WriteString(instance.ShaderName);

			serializer->WriteRaw(instance.AlbedoColor);
			serializer->WriteRaw(instance.Emission);
			serializer->WriteRaw(instance.Metalness);
			serializer->WriteRaw(instance.Roughness);
			serializer->WriteRaw(instance.UseNormalMap);

			serializer->WriteRaw(instance.AlbedoTexture);
			serializer->WriteRaw(instance.NormalTexture);
			serializer->WriteRaw(instance.MetallicRoughnessTexture);

			//serializer->WriteRaw(instance.MetalnessTexture);
			//serializer->WriteRaw(instance.RoughnessTexture);
		}

		static void Deserialize(StreamReader* deserializer, MeshMaterial& instance)
		{
			deserializer->ReadString(instance.MaterialName);
			deserializer->ReadString(instance.ShaderName);

			deserializer->ReadRaw(instance.AlbedoColor);
			deserializer->ReadRaw(instance.Emission);
			deserializer->ReadRaw(instance.Metalness);
			deserializer->ReadRaw(instance.Roughness);
			deserializer->ReadRaw(instance.UseNormalMap);

			deserializer->ReadRaw(instance.AlbedoTexture);
			deserializer->ReadRaw(instance.NormalTexture);
			deserializer->ReadRaw(instance.MetallicRoughnessTexture);

			//deserializer->ReadRaw(instance.MetalnessTexture);
			//deserializer->ReadRaw(instance.RoughnessTexture);
		}
	};

	bool MeshRuntimeSerializer::SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo)
	{
		outInfo.Offset = stream.GetStreamPosition();

		uint64_t streamOffset = stream.GetStreamPosition();

		Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(handle);

		MeshSourceFile file;

		bool hasMaterials = meshSource->GetMaterials().size() > 0;

	/*	bool hasAnimation = (meshSource->GetAnimationCount() > 0);
		bool hasSkeleton = meshSource->HasSkeleton();

		if (hasAnimation && !meshSource->m_Skeleton)
		{
			AssimpMeshImporter importer(Project::GetEditorAssetManager()->GetFileSystemPath(handle));
			importer.ImportSkeleton(meshSource->m_Skeleton);
		}*/

		file.Data.Flags = 0;
		if (hasMaterials)
			file.Data.Flags |= (uint32_t)MeshSourceFile::MeshFlags::HasMaterials;

		/*if (hasAnimation)
			file.Data.Flags |= (uint32_t)MeshSourceFile::MeshFlags::HasAnimation;
		if (hasSkeleton)
			file.Data.Flags |= (uint32_t)MeshSourceFile::MeshFlags::HasSkeleton;*/


		// Write header
		stream.WriteRaw<MeshSourceFile::FileHeader>(file.Header);
		// Leave space for Metadata
		uint64_t metadataAbsolutePosition = stream.GetStreamPosition();
		stream.WriteZero(sizeof(MeshSourceFile::Metadata));

		// Write nodes
		file.Data.NodeArrayOffset = stream.GetStreamPosition() - streamOffset;
		stream.WriteArray(meshSource->m_Nodes);
		file.Data.NodeArraySize = (stream.GetStreamPosition() - streamOffset) - file.Data.NodeArrayOffset;

		// Write submeshes
		file.Data.SubmeshArrayOffset = stream.GetStreamPosition() - streamOffset;
		stream.WriteArray(meshSource->m_Submeshes);
		file.Data.SubmeshArraySize = (stream.GetStreamPosition() - streamOffset) - file.Data.SubmeshArrayOffset;

		// Write VulkanMaterial Buffer
		if (hasMaterials)
		{
			// Prepare materials
			std::vector<MeshMaterial> meshMaterials(meshSource->GetMaterials().size());
			const auto& meshSourceMaterials = meshSource->GetMaterials();
			for (size_t i = 0; i < meshMaterials.size(); i++)
			{
				MeshMaterial& material = meshMaterials[i];
				Ref<VulkanMaterial> meshSourceMaterial = meshSourceMaterials[i];

				material.MaterialName = meshSourceMaterial->GetName();
				material.ShaderName = meshSourceMaterial->GetShader()->GetName();

				material.AlbedoColor = meshSourceMaterial->GetVector3("u_MaterialUniforms.AlbedoColor");
				material.Emission = meshSourceMaterial->GetFloat("u_MaterialUniforms.Emission");
				material.Metalness = meshSourceMaterial->GetFloat("u_MaterialUniforms.Metalness");
				material.Roughness = meshSourceMaterial->GetFloat("u_MaterialUniforms.Roughness");
				material.UseNormalMap = meshSourceMaterial->GetBool("u_MaterialUniforms.UseNormalMap");

				material.AlbedoTexture = meshSourceMaterial->GetTexture2D("u_AlbedoTexture")->Handle;
				material.NormalTexture = meshSourceMaterial->GetTexture2D("u_NormalTexture")->Handle;
				material.MetallicRoughnessTexture = meshSourceMaterial->GetTexture2D("u_MetallicRoughnessTexture")->Handle;
				material.EmissionTexture = meshSourceMaterial->GetTexture2D("u_EmissionTexture")->Handle;
				//material.EmissionTexture = Renderer::GetBlackTexture()->Handle;

			}

			// Write materials
			file.Data.MaterialArrayOffset = stream.GetStreamPosition() - streamOffset;
			stream.WriteArray(meshMaterials);
			file.Data.MaterialArraySize = (stream.GetStreamPosition() - streamOffset) - file.Data.MaterialArrayOffset;
		}
		else
		{
			// No materials
			file.Data.MaterialArrayOffset = 0;
			file.Data.MaterialArraySize = 0;
		}

		// Write Vertex Buffer
		file.Data.VertexBufferOffset = stream.GetStreamPosition() - streamOffset;
		stream.WriteArray(meshSource->m_Vertices);
		file.Data.VertexBufferSize = (stream.GetStreamPosition() - streamOffset) - file.Data.VertexBufferOffset;

		// Write Index Buffer
		file.Data.IndexBufferOffset = stream.GetStreamPosition() - streamOffset;
		stream.WriteArray(meshSource->m_Indices);
		file.Data.IndexBufferSize = (stream.GetStreamPosition() - streamOffset) - file.Data.IndexBufferOffset;

		// Write Animation Data
		//if (hasAnimation || hasSkeleton)
		//{
		//	// Cannot write animation data to assetpack unless the skeleton is present
		//	//X2_CORE_ASSERT(meshSource->HasSkeleton());

		//	file.Data.AnimationDataOffset = stream.GetStreamPosition() - streamOffset;

		//	if (hasSkeleton)
		//	{
		//		stream.WriteArray(meshSource->m_BoneInfluences);
		//		stream.WriteArray(meshSource->m_BoneInfo);
		//		stream.WriteObject(*meshSource->m_Skeleton);
		//	}

		//	stream.WriteRaw((uint32_t)meshSource->m_Animations.size());
		//	for (Scope<Animation>& animation : meshSource->m_Animations)
		//		Animation::Serialize(&stream, *animation);

		//	file.Data.AnimationDataSize = (stream.GetStreamPosition() - streamOffset) - file.Data.AnimationDataOffset;
		//}
		//else
		//{
		file.Data.AnimationDataOffset = 0;
		file.Data.AnimationDataSize = 0;
		//}

		// Write Metadata
		uint64_t endOfStream = stream.GetStreamPosition();
		stream.SetStreamPosition(metadataAbsolutePosition);
		stream.WriteRaw<MeshSourceFile::Metadata>(file.Data);
		stream.SetStreamPosition(endOfStream);

		outInfo.Size = stream.GetStreamPosition() - outInfo.Offset;
		return true;
	}

	Ref<Asset> MeshRuntimeSerializer::DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo)
	{
		stream.SetStreamPosition(assetInfo.PackedOffset);
		uint64_t streamOffset = stream.GetStreamPosition();

		MeshSourceFile file;
		stream.ReadRaw<MeshSourceFile::FileHeader>(file.Header);
		bool validHeader = memcmp(file.Header.HEADER, "X2MS", 4) == 0;
		X2_CORE_ASSERT(validHeader);
		if (!validHeader)
			return nullptr;

		Ref<MeshSource> meshSource = Ref<MeshSource>::Create();
		meshSource->m_Runtime = true;

		stream.ReadRaw<MeshSourceFile::Metadata>(file.Data);

		const auto& metadata = file.Data;
		bool hasMaterials = metadata.Flags & (uint32_t)MeshSourceFile::MeshFlags::HasMaterials;
		bool hasAnimation = metadata.Flags & (uint32_t)MeshSourceFile::MeshFlags::HasAnimation;
		bool hasSkeleton = metadata.Flags & (uint32_t)MeshSourceFile::MeshFlags::HasSkeleton;

		stream.SetStreamPosition(metadata.NodeArrayOffset + streamOffset);
		stream.ReadArray(meshSource->m_Nodes);
		stream.SetStreamPosition(metadata.SubmeshArrayOffset + streamOffset);
		stream.ReadArray(meshSource->m_Submeshes);

		if (hasMaterials)
		{
			std::vector<MeshMaterial> meshMaterials;
			stream.SetStreamPosition(metadata.MaterialArrayOffset + streamOffset);
			stream.ReadArray(meshMaterials);

			meshSource->m_Materials.resize(meshMaterials.size());
			for (size_t i = 0; i < meshMaterials.size(); i++)
			{
				const auto& meshMaterial = meshMaterials[i];
				Ref<VulkanMaterial> material = Ref<VulkanMaterial>::Create(Renderer::GetShaderLibrary()->Get(meshMaterial.ShaderName), meshMaterial.MaterialName);

				material->Set("u_MaterialUniforms.AlbedoColor", meshMaterial.AlbedoColor);
				material->Set("u_MaterialUniforms.Emission", meshMaterial.Emission);
				material->Set("u_MaterialUniforms.Metalness", meshMaterial.Metalness);
				material->Set("u_MaterialUniforms.Roughness", meshMaterial.Roughness);
				material->Set("u_MaterialUniforms.UseNormalMap", meshMaterial.UseNormalMap);

				// Get textures from AssetManager (note: this will potentially trigger additional loads)
				// TODO(Yan): set maybe to runtime error texture if no asset is present
				Ref<VulkanTexture2D> albedoTexture = AssetManager::GetAsset<VulkanTexture2D>(meshMaterial.AlbedoTexture);
				if (!albedoTexture)
					albedoTexture = Renderer::GetWhiteTexture();
				material->Set("u_AlbedoTexture", albedoTexture);

				Ref<VulkanTexture2D> normalTexture = AssetManager::GetAsset<VulkanTexture2D>(meshMaterial.NormalTexture);
				if (!normalTexture)
					normalTexture = Renderer::GetWhiteTexture();
				material->Set("u_NormalTexture", normalTexture);

				Ref<VulkanTexture2D> metallicRoughnessTexture = AssetManager::GetAsset<VulkanTexture2D>(meshMaterial.MetallicRoughnessTexture);
				if (!metallicRoughnessTexture)
					metallicRoughnessTexture = Renderer::GetWhiteTexture();
				material->Set("u_MetallicRoughnessTexture", metallicRoughnessTexture);

				Ref<VulkanTexture2D> EmissiveTexture = AssetManager::GetAsset<VulkanTexture2D>(meshMaterial.EmissionTexture);
				if (!EmissiveTexture)
					EmissiveTexture = Renderer::GetBlackTexture();
				material->Set("u_EmissionTexture", EmissiveTexture);

				meshSource->m_Materials[i] = material;
			}
		}

		stream.SetStreamPosition(metadata.VertexBufferOffset + streamOffset);
		stream.ReadArray(meshSource->m_Vertices);

		stream.SetStreamPosition(metadata.IndexBufferOffset + streamOffset);
		stream.ReadArray(meshSource->m_Indices);

		/*if (hasAnimation || hasSkeleton)
		{
			stream.SetStreamPosition(metadata.AnimationDataOffset + streamOffset);
			if (hasSkeleton)
			{
				stream.ReadArray(meshSource->m_BoneInfluences);
				stream.ReadArray(meshSource->m_BoneInfo);

				meshSource->m_Skeleton = CreateScope<Skeleton>();
				stream.ReadObject(*meshSource->m_Skeleton);
			}

			uint32_t animationCount;
			stream.ReadRaw(animationCount);
			meshSource->m_Animations.resize(animationCount);
			for (uint32_t i = 0; i < animationCount; i++)
			{
				meshSource->m_Animations[i] = CreateScope<Animation>();
				Animation::Deserialize(&stream, *meshSource->m_Animations[i]);
			}
		}*/

		if (!meshSource->m_Vertices.empty())
			meshSource->m_VertexBuffer = Ref<VulkanVertexBuffer>::Create(meshSource->m_Vertices.data(), (uint32_t)(meshSource->m_Vertices.size() * sizeof(Vertex)));

		/*if (!meshSource->m_BoneInfluences.empty())
			meshSource->m_BoneInfluenceBuffer = VertexBuffer::Create(meshSource->m_BoneInfluences.data(), (uint32_t)(meshSource->m_BoneInfluences.size() * sizeof(BoneInfluence)));*/

		if (!meshSource->m_Indices.empty())
			meshSource->m_IndexBuffer = Ref<VulkanIndexBuffer>::Create(meshSource->m_Indices.data(), (uint32_t)(meshSource->m_Indices.size() * sizeof(Index)));

		return meshSource;
	}

}
