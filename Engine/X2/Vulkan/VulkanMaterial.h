#pragma once

#include "X2/Core/Ref.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"

namespace X2 {

	enum class MaterialFlag
	{
		None = BIT(0),
		DepthTest = BIT(1),
		Blend = BIT(2),
		TwoSided = BIT(3),
		DisableShadowCasting = BIT(4)
	};

	class VulkanMaterial : public RefCounted
	{
	public:
		VulkanMaterial(const Ref<VulkanShader>& shader, const std::string& name = "");
		VulkanMaterial(Ref<VulkanMaterial> material, const std::string& name = "");
		virtual ~VulkanMaterial() ;

		virtual void Invalidate() ;
		virtual void OnShaderReloaded() ;

		virtual void Set(const std::string& name, float value) ;
		virtual void Set(const std::string& name, int value) ;
		virtual void Set(const std::string& name, uint32_t value) ;
		virtual void Set(const std::string& name, bool value) ;
		virtual void Set(const std::string& name, const glm::ivec2& value) ;
		virtual void Set(const std::string& name, const glm::ivec3& value) ;
		virtual void Set(const std::string& name, const glm::ivec4& value) ;
		virtual void Set(const std::string& name, const glm::vec2& value) ;
		virtual void Set(const std::string& name, const glm::vec3& value) ;
		virtual void Set(const std::string& name, const glm::vec4& value) ;
		virtual void Set(const std::string& name, const glm::mat3& value) ;
		virtual void Set(const std::string& name, const glm::mat4& value) ;

		virtual void Set(const std::string& name, const Ref<VulkanTexture2D>& texture) ;
		virtual void Set(const std::string& name, const Ref<VulkanTexture2D>& texture, uint32_t arrayIndex) ;
		virtual void Set(const std::string& name, const Ref<VulkanTextureCube>& texture) ;
		virtual void Set(const std::string& name, const Ref<VulkanImage2D>& image) ;

		virtual float& GetFloat(const std::string& name) ;
		virtual int32_t& GetInt(const std::string& name) ;
		virtual uint32_t& GetUInt(const std::string& name) ;
		virtual bool& GetBool(const std::string& name) ;
		virtual glm::vec2& GetVector2(const std::string& name) ;
		virtual glm::vec3& GetVector3(const std::string& name) ;
		virtual glm::vec4& GetVector4(const std::string& name) ;
		virtual glm::mat3& GetMatrix3(const std::string& name) ;
		virtual glm::mat4& GetMatrix4(const std::string& name) ;

		virtual Ref<VulkanTexture2D> GetTexture2D(const std::string& name) ;
		virtual Ref<VulkanTextureCube> GetTextureCube(const std::string& name) ;

		virtual Ref<VulkanTexture2D> TryGetTexture2D(const std::string& name) ;
		virtual Ref<VulkanTextureCube> TryGetTextureCube(const std::string& name) ;

		template <typename T>
		void Set(const std::string& name, const T& value)
		{
			auto decl = FindUniformDeclaration(name);
			X2_CORE_ASSERT(decl, "Could not find uniform!");
			if (!decl)
				return;

			auto& buffer = m_UniformStorageBuffer;
			buffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template<typename T>
		T& Get(const std::string& name)
		{
			auto decl = FindUniformDeclaration(name);
			X2_CORE_ASSERT(decl, "Could not find uniform with name 'x'");
			auto& buffer = m_UniformStorageBuffer;
			return buffer.Read<T>(decl->GetOffset());
		}

		template<typename T>
		Ref<T> GetResource(const std::string& name)
		{
			auto decl = FindResourceDeclaration(name);
			X2_CORE_ASSERT(decl, "Could not find uniform with name 'x'");
			uint32_t slot = decl->GetRegister();
			X2_CORE_ASSERT(slot < m_Textures.size(), "Texture slot is invalid!");
			return Ref<T>(m_Textures[slot]);
		}

		template<typename T>
		Ref<T> TryGetResource(const std::string& name)
		{
			auto decl = FindResourceDeclaration(name);
			if (!decl)
				return nullptr;

			uint32_t slot = decl->GetRegister();
			if (slot >= m_Textures.size())
				return nullptr;

			return Ref<T>(m_Textures[slot]);
		}

		virtual uint32_t GetFlags() const  { return m_MaterialFlags; }
		virtual void SetFlags(uint32_t flags)  { m_MaterialFlags = flags; }
		virtual bool GetFlag(MaterialFlag flag) const  { return (uint32_t)flag & m_MaterialFlags; }
		virtual void SetFlag(MaterialFlag flag, bool value = true) 
		{
			if (value)
			{
				m_MaterialFlags |= (uint32_t)flag;
			}
			else
			{
				m_MaterialFlags &= ~(uint32_t)flag;
			}
		}

		virtual Ref<VulkanShader> GetShader()  { return m_Shader; }
		virtual const std::string& GetName() const  { return m_Name; }

		Buffer GetUniformStorageBuffer() { return m_UniformStorageBuffer; }

		void RT_UpdateForRendering(const std::vector<std::vector<VkWriteDescriptorSet>>& uniformBufferWriteDescriptors = std::vector<std::vector<VkWriteDescriptorSet>>());
		void RT_UpdateForRendering(const std::vector<std::vector<VkWriteDescriptorSet>>& uniformBufferWriteDescriptors, const std::vector<std::vector<VkWriteDescriptorSet>>& storageBufferWriteDescriptors);
		void InvalidateDescriptorSets();

		VkDescriptorSet GetDescriptorSet(uint32_t index) const { return !m_DescriptorSets[index].DescriptorSets.empty() ? m_DescriptorSets[index].DescriptorSets[0] : nullptr; }
	private:
		void Init();
		void AllocateStorage();

		void SetVulkanDescriptor(const std::string& name, const Ref<VulkanTexture2D>& texture);
		void SetVulkanDescriptor(const std::string& name, const Ref<VulkanTexture2D>& texture, uint32_t arrayIndex);
		void SetVulkanDescriptor(const std::string& name, const Ref<VulkanTextureCube>& texture);
		void SetVulkanDescriptor(const std::string& name, const Ref<VulkanImage2D>& image);

		const ShaderUniform* FindUniformDeclaration(const std::string& name);
		const ShaderResourceDeclaration* FindResourceDeclaration(const std::string& name);
	private:
		Ref<VulkanShader> m_Shader;
		std::string m_Name;

		enum class PendingDescriptorType
		{
			None = 0, VulkanTexture2D, VulkanTextureCube, VulkanImage2D
		};
		struct PendingDescriptor
		{
			PendingDescriptorType Type = PendingDescriptorType::None;
			VkWriteDescriptorSet WDS;
			VkDescriptorImageInfo ImageInfo;
			Ref<Texture> Texture;
			Ref<Image> Image;
			VkDescriptorImageInfo SubmittedImageInfo{};
		};

		struct PendingDescriptorArray
		{
			PendingDescriptorType Type = PendingDescriptorType::None;
			VkWriteDescriptorSet WDS;
			std::vector<VkDescriptorImageInfo> ImageInfos;
			std::vector<Ref<Texture>> Textures;
			std::vector<Ref<Image>> Images;
			VkDescriptorImageInfo SubmittedImageInfo{};
		};
		std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptor>> m_ResidentDescriptors;
		std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptorArray>> m_ResidentDescriptorArrays;
		std::vector<std::shared_ptr<PendingDescriptor>> m_PendingDescriptors;  // TODO: weak ref

		uint32_t m_MaterialFlags = 0;

		Buffer m_UniformStorageBuffer;

		std::vector<Ref<Texture>> m_Textures; // TODO: Texture should only be stored as images
		std::vector<std::vector<Ref<Texture>>> m_TextureArrays;
		std::vector<Ref<Image>> m_Images;

		VulkanShader::ShaderMaterialDescriptorSet m_DescriptorSets[3];


		std::vector<std::vector<VkWriteDescriptorSet>> m_WriteDescriptors;
		std::vector<bool> m_DirtyDescriptorSets;

	};

}
