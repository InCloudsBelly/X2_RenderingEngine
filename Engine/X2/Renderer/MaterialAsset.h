#pragma once

#include "X2/Asset/Asset.h"
#include "X2/Vulkan/VulkanMaterial.h"

#include <map>

namespace X2 {

	class MaterialAsset : public Asset
	{
	public:
		MaterialAsset(bool transparent = false);
		MaterialAsset(Ref<VulkanMaterial> material);
		~MaterialAsset();

		glm::vec3& GetAlbedoColor();
		void SetAlbedoColor(const glm::vec3& color);

		float& GetMetalness();
		void SetMetalness(float value);

		float& GetRoughness();
		void SetRoughness(float value);

		float& GetEmission();
		void SetEmission(float value);

		// Textures
		Ref<VulkanTexture2D> GetAlbedoMap();
		void SetAlbedoMap(Ref<VulkanTexture2D> texture);
		void ClearAlbedoMap();

		Ref<VulkanTexture2D> GetNormalMap();
		void SetNormalMap(Ref<VulkanTexture2D> texture);
		bool IsUsingNormalMap();
		void SetUseNormalMap(bool value);
		void ClearNormalMap();

		Ref<VulkanTexture2D> GetMetallicRoughnessMap();
		void SetMetallicRoughnessMap(Ref<VulkanTexture2D> texture);
		void ClearMetallicRoughnessMap();

		Ref<VulkanTexture2D> GetEmissionMap();
		void SetEmissionMap(Ref<VulkanTexture2D> texture);
		void ClearEmissionsMap();

		float& GetTransparency();
		void SetTransparency(float transparency);

		bool IsShadowCasting() const { return !m_Material->GetFlag(MaterialFlag::DisableShadowCasting); }
		void SetShadowCasting(bool castsShadows) { return m_Material->SetFlag(MaterialFlag::DisableShadowCasting, !castsShadows); }

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

		Ref<VulkanMaterial> GetMaterial() const { return m_Material; }
		void SetMaterial(Ref<VulkanMaterial> material) { m_Material = material; }

		bool IsTransparent() const { return m_Transparent; }
	private:
		void SetDefaults();
	private:
		Ref<VulkanMaterial> m_Material;
		bool m_Transparent = false;

		friend class MaterialEditor;
	};

	class MaterialTable 
	{
	public:
		MaterialTable(uint32_t materialCount = 1);
		MaterialTable(Ref<MaterialTable> other);
		~MaterialTable() = default;

		bool HasMaterial(uint32_t materialIndex) const { return m_Materials.find(materialIndex) != m_Materials.end(); }
		void SetMaterial(uint32_t index, AssetHandle material);
		void ClearMaterial(uint32_t index);

		AssetHandle GetMaterial(uint32_t materialIndex) const
		{
			X2_CORE_ASSERT(HasMaterial(materialIndex));
			return m_Materials.at(materialIndex);
		}
		std::map<uint32_t, AssetHandle>& GetMaterials() { return m_Materials; }
		const std::map<uint32_t, AssetHandle>& GetMaterials() const { return m_Materials; }

		uint32_t GetMaterialCount() const { return m_MaterialCount; }
		void SetMaterialCount(uint32_t materialCount) { m_MaterialCount = materialCount; }

		void Clear();
	private:
		std::map<uint32_t, AssetHandle> m_Materials;
		uint32_t m_MaterialCount;
	};

}
