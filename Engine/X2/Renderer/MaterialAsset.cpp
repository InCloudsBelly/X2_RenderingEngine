#include "Precompiled.h"
#include "MaterialAsset.h"

#include "X2/Renderer/Renderer.h"

namespace X2 {

	static const std::string s_AlbedoColorUniform = "u_MaterialUniforms.AlbedoColor";
	static const std::string s_UseNormalMapUniform = "u_MaterialUniforms.UseNormalMap";
	static const std::string s_MetalnessUniform = "u_MaterialUniforms.Metalness";
	static const std::string s_RoughnessUniform = "u_MaterialUniforms.Roughness";
	static const std::string s_EmissionUniform = "u_MaterialUniforms.Emission";
	static const std::string s_TransparencyUniform = "u_MaterialUniforms.Transparency";

	static const std::string s_AlbedoMapUniform = "u_AlbedoTexture";
	static const std::string s_NormalMapUniform = "u_NormalTexture";
	static const std::string s_MetallicRoughnessMapUniform = "u_MetallicRoughnessTexture";
	static const std::string s_EmissionMapUniform = "u_EmissionTexture";
	//static const std::string s_MetalnessMapUniform = "u_MetalnessTexture";
	//static const std::string s_RoughnessMapUniform = "u_RoughnessTexture";

	MaterialAsset::MaterialAsset(bool transparent)
		: m_Transparent(transparent)
	{
		Handle = {};

		if (transparent)
			m_Material = CreateRef<VulkanMaterial>(Renderer::GetShaderLibrary()->Get("PBR_Transparent"));
		else
			m_Material = CreateRef<VulkanMaterial>(Renderer::GetShaderLibrary()->Get("PBR_Static"));

		SetDefaults();
	}

	MaterialAsset::MaterialAsset(Ref<VulkanMaterial> material)
	{
		Handle = {};
		m_Material = CreateRef<VulkanMaterial>(material);
	}

	MaterialAsset::~MaterialAsset()
	{
	}

	glm::vec3& MaterialAsset::GetAlbedoColor()
	{
		return m_Material->GetVector3(s_AlbedoColorUniform);
	}

	void MaterialAsset::SetAlbedoColor(const glm::vec3& color)
	{
		m_Material->Set(s_AlbedoColorUniform, color);
	}

	float& MaterialAsset::GetMetalness()
	{
		return m_Material->GetFloat(s_MetalnessUniform);
	}

	void MaterialAsset::SetMetalness(float value)
	{
		m_Material->Set(s_MetalnessUniform, value);
	}

	float& MaterialAsset::GetRoughness()
	{
		return m_Material->GetFloat(s_RoughnessUniform);
	}

	void MaterialAsset::SetRoughness(float value)
	{
		m_Material->Set(s_RoughnessUniform, value);
	}

	float& MaterialAsset::GetEmission()
	{
		return m_Material->GetFloat(s_EmissionUniform);
	}

	void MaterialAsset::SetEmission(float value)
	{
		m_Material->Set(s_EmissionUniform, value);
	}

	Ref<VulkanTexture2D> MaterialAsset::GetAlbedoMap()
	{
		return m_Material->TryGetTexture2D(s_AlbedoMapUniform);
	}

	void MaterialAsset::SetAlbedoMap(Ref<VulkanTexture2D> texture)
	{
		m_Material->Set(s_AlbedoMapUniform, texture);
	}

	void MaterialAsset::ClearAlbedoMap()
	{
		m_Material->Set(s_AlbedoMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<VulkanTexture2D> MaterialAsset::GetNormalMap()
	{
		return m_Material->TryGetTexture2D(s_NormalMapUniform);
	}

	void MaterialAsset::SetNormalMap(Ref<VulkanTexture2D> texture)
	{
		m_Material->Set(s_NormalMapUniform, texture);
	}

	bool MaterialAsset::IsUsingNormalMap()
	{
		return m_Material->GetBool(s_UseNormalMapUniform);
	}

	void MaterialAsset::SetUseNormalMap(bool value)
	{
		m_Material->Set(s_UseNormalMapUniform, value);
	}

	void MaterialAsset::ClearNormalMap()
	{
		m_Material->Set(s_NormalMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<VulkanTexture2D> MaterialAsset::GetMetallicRoughnessMap()
	{
		return m_Material->TryGetTexture2D(s_MetallicRoughnessMapUniform);
	}

	void MaterialAsset::SetMetallicRoughnessMap(Ref<VulkanTexture2D> texture)
	{
		m_Material->Set(s_MetallicRoughnessMapUniform, texture);
	}

	void MaterialAsset::ClearMetallicRoughnessMap()
	{
		m_Material->Set(s_MetallicRoughnessMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<VulkanTexture2D> MaterialAsset::GetEmissionMap()
	{
		return m_Material->TryGetTexture2D(s_EmissionMapUniform);
	}

	void MaterialAsset::SetEmissionMap(Ref<VulkanTexture2D> texture)
	{
		m_Material->Set(s_EmissionMapUniform, texture);
	}

	void MaterialAsset::ClearEmissionsMap()
	{
		m_Material->Set(s_EmissionMapUniform, Renderer::GetWhiteTexture());
	}


	float& MaterialAsset::GetTransparency()
	{
		return m_Material->GetFloat(s_TransparencyUniform);
	}

	void MaterialAsset::SetTransparency(float transparency)
	{
		m_Material->Set(s_TransparencyUniform, transparency);
	}

	void MaterialAsset::SetDefaults()
	{
		if (m_Transparent)
		{
			// Set defaults
			SetAlbedoColor(glm::vec3(0.8f));

			// Maps
			SetAlbedoMap(Renderer::GetWhiteTexture());
		}
		else
		{
			// Set defaults
			SetAlbedoColor(glm::vec3(0.8f));
			SetEmission(0.0f);
			SetUseNormalMap(false);
			SetMetalness(0.0f);
			SetRoughness(0.4f);

			// Maps
			SetAlbedoMap(Renderer::GetWhiteTexture());
			SetNormalMap(Renderer::GetWhiteTexture());
			SetMetallicRoughnessMap(Renderer::GetWhiteTexture());
			SetEmissionMap(Renderer::GetBlackTexture());

			/*SetMetalnessMap(Renderer::GetWhiteTexture());
			SetRoughnessMap(Renderer::GetWhiteTexture());*/
		}
	}

	MaterialTable::MaterialTable(uint32_t materialCount)
		: m_MaterialCount(materialCount)
	{
	}

	MaterialTable::MaterialTable(Ref<MaterialTable> other)
		: m_MaterialCount(other->m_MaterialCount)
	{
		const auto& meshMaterials = other->GetMaterials();
		for (auto [index, materialAsset] : meshMaterials)
			//SetMaterial(index, CreateRef<MaterialAsset>(materialAsset->GetMaterial()));
			SetMaterial(index, materialAsset);
	}

	void MaterialTable::SetMaterial(uint32_t index, AssetHandle material)
	{
		m_Materials[index] = material;
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

	void MaterialTable::ClearMaterial(uint32_t index)
	{
		X2_CORE_ASSERT(HasMaterial(index));
		m_Materials.erase(index);
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

	void MaterialTable::Clear()
	{
		m_Materials.clear();
	}

}
