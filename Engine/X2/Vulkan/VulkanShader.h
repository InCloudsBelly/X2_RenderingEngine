#pragma once

#include <filesystem>
#include <unordered_set>

#include "X2/Core/Ref.h"
#include "VulkanShaderResource.h"

#include "vk_mem_alloc.h"

namespace X2 {

	namespace ShaderUtils {
		enum class SourceLang
		{
			NONE, GLSL, HLSL,
		};
	}

	enum class ShaderUniformType
	{
		None = 0, Bool, Int, UInt, Float, Vec2, Vec3, Vec4, Mat3, Mat4,
		IVec2, IVec3, IVec4
	};

	class ShaderUniform
	{
	public:
		ShaderUniform() = default;
		ShaderUniform(std::string name, ShaderUniformType type, uint32_t size, uint32_t offset);

		const std::string& GetName() const { return m_Name; }
		ShaderUniformType GetType() const { return m_Type; }
		uint32_t GetSize() const { return m_Size; }
		uint32_t GetOffset() const { return m_Offset; }

		static constexpr std::string_view UniformTypeToString(ShaderUniformType type);

		static void Serialize(StreamWriter* serializer, const ShaderUniform& instance)
		{
			serializer->WriteString(instance.m_Name);
			serializer->WriteRaw(instance.m_Type);
			serializer->WriteRaw(instance.m_Size);
			serializer->WriteRaw(instance.m_Offset);
		}

		static void Deserialize(StreamReader* deserializer, ShaderUniform& instance)
		{
			deserializer->ReadString(instance.m_Name);
			deserializer->ReadRaw(instance.m_Type);
			deserializer->ReadRaw(instance.m_Size);
			deserializer->ReadRaw(instance.m_Offset);
		}
	private:
		std::string m_Name;
		ShaderUniformType m_Type = ShaderUniformType::None;
		uint32_t m_Size = 0;
		uint32_t m_Offset = 0;
	};

	struct ShaderUniformBuffer
	{
		std::string Name;
		uint32_t Index;
		uint32_t BindingPoint;
		uint32_t Size;
		uint32_t RendererID;
		std::vector<ShaderUniform> Uniforms;
	};

	struct ShaderStorageBuffer
	{
		std::string Name;
		uint32_t Index;
		uint32_t BindingPoint;
		uint32_t Size;
		uint32_t RendererID;
		//std::vector<ShaderUniform> Uniforms;
	};

	struct ShaderBuffer
	{
		std::string Name;
		uint32_t Size = 0;
		std::unordered_map<std::string, ShaderUniform> Uniforms;

		static void Serialize(StreamWriter* serializer, const ShaderBuffer& instance)
		{
			serializer->WriteString(instance.Name);
			serializer->WriteRaw(instance.Size);
			serializer->WriteMap(instance.Uniforms);
		}

		static void Deserialize(StreamReader* deserializer, ShaderBuffer& instance)
		{
			deserializer->ReadString(instance.Name);
			deserializer->ReadRaw(instance.Size);
			deserializer->ReadMap(instance.Uniforms);
		}
	};


	class VulkanShader 
	{
	public:
		struct ReflectionData
		{
			std::vector<ShaderResource::ShaderDescriptorSet> ShaderDescriptorSets;
			std::unordered_map<std::string, ShaderResourceDeclaration> Resources;
			std::unordered_map<std::string, ShaderBuffer> ConstantBuffers;
			std::vector<ShaderResource::PushConstantRange> PushConstantRanges;
		};
		
		using ShaderReloadedCallback = std::function<void()>;

	public:
		VulkanShader() = default;
		VulkanShader(const std::string& path, bool forceCompile = false, bool disableOptimization = false);
		virtual ~VulkanShader();
		void Release();

		void Reload(bool forceCompile = false) ;
		void RT_Reload(bool forceCompile) ;

		virtual size_t GetHash() const ;
		void SetMacro(const std::string& name, const std::string& value)  {}

		virtual const std::string& GetName() const  { return m_Name; }
		virtual const std::unordered_map<std::string, ShaderBuffer>& GetShaderBuffers() const  { return m_ReflectionData.ConstantBuffers; }
		virtual const std::unordered_map<std::string, ShaderResourceDeclaration>& GetResources() const ;
		virtual void AddShaderReloadedCallback(const ShaderReloadedCallback& callback) ;

		bool TryReadReflectionData(StreamReader* serializer);

		void SerializeReflectionData(StreamWriter* serializer);

		void SetReflectionData(const ReflectionData& reflectionData);

		// Vulkan-specific
		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }

		VkDescriptorSet GetDescriptorSet() { return m_DescriptorSet; }
		VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) { return m_DescriptorSetLayouts.at(set); }
		std::vector<VkDescriptorSetLayout> GetAllDescriptorSetLayouts();

		ShaderResource::UniformBuffer& GetUniformBuffer(const uint32_t binding = 0, const uint32_t set = 0) { X2_CORE_ASSERT(m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.size() > binding); return m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.at(binding); }
		uint32_t GetUniformBufferCount(const uint32_t set = 0)
		{
			if (m_ReflectionData.ShaderDescriptorSets.size() < set)
				return 0;

			return (uint32_t)m_ReflectionData.ShaderDescriptorSets[set].UniformBuffers.size();
		}

		const std::vector<ShaderResource::ShaderDescriptorSet>& GetShaderDescriptorSets() const { return m_ReflectionData.ShaderDescriptorSets; }
		bool HasDescriptorSet(uint32_t set) const { return m_TypeCounts.find(set) != m_TypeCounts.end(); }

		const std::vector<ShaderResource::PushConstantRange>& GetPushConstantRanges() const { return m_ReflectionData.PushConstantRanges; }

		struct ShaderMaterialDescriptorSet
		{
			VkDescriptorPool Pool = nullptr;
			std::vector<VkDescriptorSet> DescriptorSets;
		};

		ShaderMaterialDescriptorSet AllocateDescriptorSet(uint32_t set = 0);
		ShaderMaterialDescriptorSet CreateOrGetDescriptorSets(uint32_t set = 0);
		ShaderMaterialDescriptorSet CreateOrGetDescriptorSets(uint32_t set, uint32_t numberOfSets);
		const VkWriteDescriptorSet* GetDescriptorSet(const std::string& name, uint32_t set = 0) const;

		
		static constexpr const char* GetShaderDirectoryPath()
		{
			return "Resources/Shaders/";
		}

	private:
		void LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void CreateDescriptors();
	private:
		std::vector<VkPipelineShaderStageCreateInfo> m_PipelineShaderStageCreateInfos;

		std::filesystem::path m_AssetPath;
		std::string m_Name;
		bool m_DisableOptimization = false;

		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> m_ShaderData;
		ReflectionData m_ReflectionData;

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		VkDescriptorSet m_DescriptorSet;
		//VkDescriptorPool m_DescriptorPool = nullptr;

		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> m_TypeCounts;
		std::unordered_map<uint32_t, ShaderMaterialDescriptorSet> m_ShaderMtDescSets;

	private:
		friend class ShaderCache;
		friend class ShaderPack;
		friend class VulkanShaderCompiler;
	};


	class ShaderPack;

	// This should be eventually handled by the Asset Manager
	class ShaderLibrary 
	{
	public:
		ShaderLibrary();
		~ShaderLibrary();

		void Add(const Ref<VulkanShader>& shader);
		void Load(std::string_view path, bool forceCompile = false, bool disableOptimization = false);
		void Load(std::string_view name, const std::string& path);
		void LoadShaderPack(const std::filesystem::path& path);

		const Ref<VulkanShader>& Get(const std::string& name) const;
		size_t GetSize() const { return m_Shaders.size(); }

		std::unordered_map<std::string, Ref<VulkanShader>>& GetShaders() { return m_Shaders; }
		const std::unordered_map<std::string, Ref<VulkanShader>>& GetShaders() const { return m_Shaders; }
	private:
		std::unordered_map<std::string, Ref<VulkanShader>> m_Shaders;
		Ref<ShaderPack> m_ShaderPack;
	};

}
