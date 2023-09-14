#include "Precompiled.h"
#include "VulkanShaderCompiler.h"

#include "X2/Utilities/StringUtils.h"

#include "ShaderPreprocessing/GlslIncluder.h"

#include "VulkanShaderCache.h"

#include <spirv_cross/spirv_glsl.hpp>
#include <spirv-tools/libspirv.h>

//#include <dxc/dxcapi.h>
#include <shaderc/shaderc.hpp>
#include <libshaderc_util/file_finder.h>

#include "X2/Core/Hash.h"

#include "X2/Vulkan/VulkanShader.h"
#include "X2/Vulkan/VulkanContext.h"

#include "X2/Serialization/FileStream.h"

#include "X2/Renderer/Renderer.h"

namespace X2 {

	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::UniformBuffer>> s_UniformBuffers; // set -> binding point -> buffer
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::StorageBuffer>> s_StorageBuffers; // set -> binding point -> buffer

	namespace Utils {

		static const char* GetCacheDirectory()
		{
			// TODO: make sure the assets directory is valid
			return "Resources/Cache/Shader/Vulkan";
		}

		static void CreateCacheDirectoryIfNeeded()
		{
			std::string cacheDirectory = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}

		static ShaderUniformType SPIRTypeToShaderUniformType(spirv_cross::SPIRType type)
		{
			switch (type.basetype)
			{
				case spirv_cross::SPIRType::Boolean:  return ShaderUniformType::Bool;
				case spirv_cross::SPIRType::Int:
					if (type.vecsize == 1)            return ShaderUniformType::Int;
					if (type.vecsize == 2)            return ShaderUniformType::IVec2;
					if (type.vecsize == 3)            return ShaderUniformType::IVec3;
					if (type.vecsize == 4)            return ShaderUniformType::IVec4;

				case spirv_cross::SPIRType::UInt:     return ShaderUniformType::UInt;
				case spirv_cross::SPIRType::Float:
					if (type.columns == 3)            return ShaderUniformType::Mat3;
					if (type.columns == 4)            return ShaderUniformType::Mat4;

					if (type.vecsize == 1)            return ShaderUniformType::Float;
					if (type.vecsize == 2)            return ShaderUniformType::Vec2;
					if (type.vecsize == 3)            return ShaderUniformType::Vec3;
					if (type.vecsize == 4)            return ShaderUniformType::Vec4;
					break;
			}
			X2_CORE_ASSERT(false, "Unknown type!");
			return ShaderUniformType::None;
		}
	}

	VulkanShaderCompiler::VulkanShaderCompiler(const std::filesystem::path& shaderSourcePath, bool disableOptimization)
		: m_ShaderSourcePath(shaderSourcePath), m_DisableOptimization(disableOptimization)
	{
		m_Language = ShaderUtils::ShaderLangFromExtension(shaderSourcePath.extension().string());
	}

	bool VulkanShaderCompiler::Reload(bool forceCompile)
	{
		m_ShaderSource.clear();
		m_StagesMetadata.clear();
		m_SPIRVDebugData.clear();
		m_SPIRVData.clear();

		Utils::CreateCacheDirectoryIfNeeded();
		const std::string source = Utils::ReadFileAndSkipBOM(std::string(PROJECT_ROOT) + m_ShaderSourcePath.string());
		X2_CORE_VERIFY(source.size(), "Failed to load shader!");

		X2_CORE_TRACE_TAG("Renderer", "Compiling shader: {}", m_ShaderSourcePath.string());
		m_ShaderSource = PreProcess(source);
		const VkShaderStageFlagBits changedStages = VulkanShaderCache::HasChanged(this);

		bool compileSucceeded = CompileOrGetVulkanBinaries(m_SPIRVDebugData, m_SPIRVData, changedStages, forceCompile);
		if (!compileSucceeded)
		{
			X2_CORE_ASSERT(false);
			return false;
		}


		// Reflection
		if (forceCompile || changedStages || !TryReadCachedReflectionData())
		{
			ReflectAllShaderStages(m_SPIRVDebugData);
			SerializeReflectionData();
		}

		return true;
	}

	void VulkanShaderCompiler::ClearUniformBuffers()
	{
		s_UniformBuffers.clear();
		s_StorageBuffers.clear();
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::PreProcess(const std::string& source)
	{
		switch (m_Language)
		{
			case ShaderUtils::SourceLang::GLSL: return PreProcessGLSL(source);
			//case ShaderUtils::SourceLang::HLSL: return PreProcessHLSL(source);
		}

		X2_CORE_VERIFY(false);
		return {};
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::PreProcessGLSL(const std::string& source)
	{
		std::map<VkShaderStageFlagBits, std::string> shaderSources = ShaderPreprocessor::PreprocessShader<ShaderUtils::SourceLang::GLSL>(source, m_AcknowledgedMacros);

		static shaderc::Compiler compiler;

		shaderc_util::FileFinder fileFinder;
		fileFinder.search_path().emplace_back(std::string(PROJECT_ROOT)+"Resources/Shaders/Include/GLSL/"); //Main include directory
		fileFinder.search_path().emplace_back(std::string(PROJECT_ROOT) + "Resources/Shaders/Include/Common/"); //Shared include directory
		for (auto& [stage, shaderSource] : shaderSources)
		{
			shaderc::CompileOptions options;
			options.AddMacroDefinition("__GLSL__");
			options.AddMacroDefinition(std::string(ShaderUtils::VKStageToShaderMacro(stage)));

			const auto& globalMacros = Renderer::GetGlobalShaderMacros();
			for (const auto& [name, value] : globalMacros)
				options.AddMacroDefinition(name, value);

			// Deleted by shaderc and created per stage
			GlslIncluder* includer = new GlslIncluder(&fileFinder);

			options.SetIncluder(std::unique_ptr<GlslIncluder>(includer));
			const auto preProcessingResult = compiler.PreprocessGlsl(shaderSource, ShaderUtils::ShaderStageToShaderC(stage), m_ShaderSourcePath.string().c_str(), options);
			if (preProcessingResult.GetCompilationStatus() != shaderc_compilation_status_success)
				X2_CORE_ERROR_TAG("Renderer", fmt::format("Failed to pre-process \"{}\"'s {} shader.\nError: {}", m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage), preProcessingResult.GetErrorMessage()));

			m_StagesMetadata[stage].HashValue = Hash::GenerateFNVHash(shaderSource);
			m_StagesMetadata[stage].Headers = std::move(includer->GetIncludeData());

			m_AcknowledgedMacros.merge(includer->GetParsedSpecialMacros());

			shaderSource = std::string(preProcessingResult.begin(), preProcessingResult.end());
		}
		return shaderSources;
	}

	
	std::string VulkanShaderCompiler::Compile(std::vector<uint32_t>& outputBinary, const VkShaderStageFlagBits stage, CompilationOptions options) const
	{
		const std::string& stageSource = m_ShaderSource.at(stage);

		if (m_Language == ShaderUtils::SourceLang::GLSL)
		{
			static shaderc::Compiler compiler;
			shaderc::CompileOptions shaderCOptions;
			shaderCOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			shaderCOptions.SetWarningsAsErrors();
			if (options.GenerateDebugInfo)
				shaderCOptions.SetGenerateDebugInfo();

			if (options.Optimize)
				shaderCOptions.SetOptimizationLevel(shaderc_optimization_level_performance);

			// Compile shader
			const shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(stageSource, ShaderUtils::ShaderStageToShaderC(stage), m_ShaderSourcePath.string().c_str(), shaderCOptions);

			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				return fmt::format("{}While compiling shader file: {} \nAt stage: {}", module.GetErrorMessage(), m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage));

			outputBinary = std::vector<uint32_t>(module.begin(), module.end());
			return {}; // Success
		}
		return "Unknown language!";
	}

	Ref<VulkanShader> VulkanShaderCompiler::Compile(const std::filesystem::path& shaderSourcePath, bool forceCompile, bool disableOptimization)
	{
		// Set name
		std::string path = shaderSourcePath.string();
		size_t found = path.find_last_of("/\\");
		std::string name = found != std::string::npos ? path.substr(found + 1) : path;
		found = name.find_last_of('.');
		name = found != std::string::npos ? name.substr(0, found) : name;

		Ref<VulkanShader> shader = CreateRef<VulkanShader>();
		shader->m_AssetPath = shaderSourcePath;
		shader->m_Name = name;
		shader->m_DisableOptimization = disableOptimization;

		Ref<VulkanShaderCompiler> compiler = CreateRef<VulkanShaderCompiler>(shaderSourcePath, disableOptimization);
		compiler->Reload(forceCompile);

		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->CreateDescriptors();

		Renderer::AcknowledgeParsedGlobalMacros(compiler->GetAcknowledgedMacros(), shader.get());
		Renderer::OnShaderReloaded(shader->GetHash());
		return shader;
	}

	bool VulkanShaderCompiler::TryRecompile(VulkanShader* shader)
	{
		Ref<VulkanShaderCompiler> compiler = CreateRef<VulkanShaderCompiler>(shader->m_AssetPath, shader->m_DisableOptimization);
		bool compileSucceeded = compiler->Reload(true);
		if (!compileSucceeded)
			return false;

		shader->Release();

		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->CreateDescriptors();

		Renderer::AcknowledgeParsedGlobalMacros(compiler->GetAcknowledgedMacros(), shader);
		Renderer::OnShaderReloaded(shader->GetHash());

		return true;
	}

	bool VulkanShaderCompiler::CompileOrGetVulkanBinaries(std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputDebugBinary, std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBinary, const VkShaderStageFlagBits changedStages, const bool forceCompile)
	{
		for (auto [stage, source] : m_ShaderSource)
		{
			if (!CompileOrGetVulkanBinary(stage, outputDebugBinary[stage], true, changedStages, forceCompile))
				return false;
			if (!CompileOrGetVulkanBinary(stage, outputBinary[stage], false, changedStages, forceCompile))
				return false;
		}
		return true;
	}


	bool VulkanShaderCompiler::CompileOrGetVulkanBinary(VkShaderStageFlagBits stage, std::vector<uint32_t>& outputBinary, bool debug, VkShaderStageFlagBits changedStages, bool forceCompile)
	{
		const std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

		// Compile shader with debug info so we can reflect
		const auto extension = ShaderUtils::ShaderStageCachedFileExtension(stage, debug);
		if (!forceCompile && stage & ~changedStages) // Per-stage cache is found and is unchanged 
		{
			TryGetVulkanCachedBinary(cacheDirectory, extension, outputBinary);
		}

		if (outputBinary.empty())
		{
			CompilationOptions options;
			if (debug)
			{
				options.GenerateDebugInfo = true;
				options.Optimize = false;
			}
			else
			{
				options.GenerateDebugInfo = false;
				// Disable optimization for compute shaders because of shaderc internal error
				options.Optimize = !m_DisableOptimization && stage != VK_SHADER_STAGE_COMPUTE_BIT;
			}

			if (std::string error = Compile(outputBinary, stage, options); error.size())
			{
				X2_CORE_ERROR_TAG("Renderer", "{}", error);
				TryGetVulkanCachedBinary(cacheDirectory, extension, outputBinary);
				if (outputBinary.empty())
				{
					X2_CONSOLE_LOG_ERROR("Failed to compile shader and couldn't find a cached version.");
				}
				else
				{
					X2_CONSOLE_LOG_ERROR("Failed to compile {}:{} so a cached version was loaded instead.", m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage));
#if 0
					if (GImGui) // Guaranteed to be null before first ImGui frame
					{
						ImGuiWindow* logWindow = ImGui::FindWindowByName("Log");
						ImGui::FocusWindow(logWindow);
					}
#endif
				}
				return false;
			}
			else // Compile success
			{
				auto path = cacheDirectory / (m_ShaderSourcePath.filename().string() + extension);
				std::string cachedFilePath = path.string();

				FILE* f;
				errno_t err = fopen_s(&f, cachedFilePath.c_str(), "wb");
				if (err)
					X2_CORE_ERROR("Failed to cache shader binary!");
				fwrite(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
				fclose(f);
			}
		}

		return true;
	}

	void VulkanShaderCompiler::ClearReflectionData()
	{
		m_ReflectionData.ShaderDescriptorSets.clear();
		m_ReflectionData.Resources.clear();
		m_ReflectionData.ConstantBuffers.clear();
		m_ReflectionData.PushConstantRanges.clear();
	}

	void VulkanShaderCompiler::TryGetVulkanCachedBinary(const std::filesystem::path & cacheDirectory, const std::string & extension, std::vector<uint32_t>&outputBinary) const
	{
		const auto path = cacheDirectory / (m_ShaderSourcePath.filename().string() + extension);
		const std::string cachedFilePath = path.string();

		FILE* f;
		errno_t err = fopen_s(&f, cachedFilePath.data(), "rb");
		if (err)
			return;

		fseek(f, 0, SEEK_END);
		uint64_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		outputBinary = std::vector<uint32_t>(size / sizeof(uint32_t));
		fread(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
		fclose(f);
	}

	bool VulkanShaderCompiler::TryReadCachedReflectionData()
	{
		struct ReflectionFileHeader
		{
			char Header[4] = { 'X','2','S','R' };
		} header;

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();
		const auto path = cacheDirectory / (m_ShaderSourcePath.filename().string() + ".cached_vulkan.refl");
		FileStreamReader serializer(path);
		if (!serializer)
			return false;

		serializer.ReadRaw(header);

		bool validHeader = memcmp(&header, "X2SR", 4) == 0;
		X2_CORE_VERIFY(validHeader);
		if (!validHeader)
			return false;

		ClearReflectionData();

		uint32_t shaderDescriptorSetCount;
		serializer.ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			auto& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			serializer.ReadMap(descriptorSet.UniformBuffers);
			serializer.ReadMap(descriptorSet.StorageBuffers);
			serializer.ReadMap(descriptorSet.ImageSamplers);
			serializer.ReadMap(descriptorSet.StorageImages);
			serializer.ReadMap(descriptorSet.SeparateTextures);
			serializer.ReadMap(descriptorSet.SeparateSamplers);
			serializer.ReadMap(descriptorSet.WriteDescriptorSets);
		}

		serializer.ReadMap(m_ReflectionData.Resources);
		serializer.ReadMap(m_ReflectionData.ConstantBuffers);
		serializer.ReadArray(m_ReflectionData.PushConstantRanges);

		return true;
	}

	void VulkanShaderCompiler::SerializeReflectionData()
	{
		struct ReflectionFileHeader
		{
			char Header[4] = { 'X','2','S','R' };
		} header;

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();
		const auto path = cacheDirectory / (m_ShaderSourcePath.filename().string() + ".cached_vulkan.refl");
		FileStreamWriter serializer(path);
		serializer.WriteRaw(header);
		SerializeReflectionData(&serializer);
	}

	void VulkanShaderCompiler::SerializeReflectionData(StreamWriter* serializer)
	{
		serializer->WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());
		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer->WriteMap(descriptorSet.UniformBuffers);
			serializer->WriteMap(descriptorSet.StorageBuffers);
			serializer->WriteMap(descriptorSet.ImageSamplers);
			serializer->WriteMap(descriptorSet.StorageImages);
			serializer->WriteMap(descriptorSet.SeparateTextures);
			serializer->WriteMap(descriptorSet.SeparateSamplers);
			serializer->WriteMap(descriptorSet.WriteDescriptorSets);
		}

		serializer->WriteMap(m_ReflectionData.Resources);
		serializer->WriteMap(m_ReflectionData.ConstantBuffers);
		serializer->WriteArray(m_ReflectionData.PushConstantRanges);
	}

	void VulkanShaderCompiler::ReflectAllShaderStages(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		ClearReflectionData();

		for (auto [stage, data] : shaderData)
		{
			Reflect(stage, data);
		}
	}

	void VulkanShaderCompiler::Reflect(VkShaderStageFlagBits shaderStage, const std::vector<uint32_t>& shaderData)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		X2_CORE_TRACE_TAG("Renderer", "===========================");
		X2_CORE_TRACE_TAG("Renderer", " Vulkan Shader Reflection");
		X2_CORE_TRACE_TAG("Renderer", "===========================");

		spirv_cross::Compiler compiler(shaderData.data(), shaderData.size());
		auto resources = compiler.get_shader_resources();

		X2_CORE_TRACE_TAG("Renderer", "Uniform Buffers:");
		for (const auto& resource : resources.uniform_buffers)
		{
			auto activeBuffers = compiler.get_active_buffer_ranges(resource.id);
			// Discard unused buffers from headers
			if (activeBuffers.size())
			{
				const auto& name = resource.name;
				auto& bufferType = compiler.get_type(resource.base_type_id);
				int memberCount = (uint32_t)bufferType.member_types.size();
				uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

				if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
					m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

				ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
				if (s_UniformBuffers[descriptorSet].find(binding) == s_UniformBuffers[descriptorSet].end())
				{
					ShaderResource::UniformBuffer uniformBuffer;
					uniformBuffer.BindingPoint = binding;
					uniformBuffer.Size = size;
					uniformBuffer.Name = name;
					uniformBuffer.ShaderStage = VK_SHADER_STAGE_ALL;
					s_UniformBuffers.at(descriptorSet)[binding] = uniformBuffer;
				}
				else
				{
					ShaderResource::UniformBuffer& uniformBuffer = s_UniformBuffers.at(descriptorSet).at(binding);
					if (size > uniformBuffer.Size)
						uniformBuffer.Size = size;

				}
				shaderDescriptorSet.UniformBuffers[binding] = s_UniformBuffers.at(descriptorSet).at(binding);

				X2_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
				X2_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
				X2_CORE_TRACE_TAG("Renderer", "  Size: {0}", size);
				X2_CORE_TRACE_TAG("Renderer", "-------------------");
			}
		}

		X2_CORE_TRACE_TAG("Renderer", "Storage Buffers:");
		for (const auto& resource : resources.storage_buffers)
		{
			auto activeBuffers = compiler.get_active_buffer_ranges(resource.id);
			// Discard unused buffers from headers
			if (activeBuffers.size())
			{
				const auto& name = resource.name;
				auto& bufferType = compiler.get_type(resource.base_type_id);
				uint32_t memberCount = (uint32_t)bufferType.member_types.size();
				uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

				if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
					m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

				ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
				if (s_StorageBuffers[descriptorSet].find(binding) == s_StorageBuffers[descriptorSet].end())
				{
					ShaderResource::StorageBuffer storageBuffer;
					storageBuffer.BindingPoint = binding;
					storageBuffer.Size = size;
					storageBuffer.Name = name;
					storageBuffer.ShaderStage = VK_SHADER_STAGE_ALL;
					s_StorageBuffers.at(descriptorSet)[binding] = storageBuffer;
				}
				else
				{
					ShaderResource::StorageBuffer& storageBuffer = s_StorageBuffers.at(descriptorSet).at(binding);
					if (size > storageBuffer.Size)
						storageBuffer.Size = size;
				}

				shaderDescriptorSet.StorageBuffers[binding] = s_StorageBuffers.at(descriptorSet).at(binding);

				X2_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
				X2_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
				X2_CORE_TRACE_TAG("Renderer", "  Size: {0}", size);
				X2_CORE_TRACE_TAG("Renderer", "-------------------");
			}
		}

		X2_CORE_TRACE_TAG("Renderer", "Push Constant Buffers:");
		for (const auto& resource : resources.push_constant_buffers)
		{
			const auto& bufferName = resource.name;
			auto& bufferType = compiler.get_type(resource.base_type_id);
			auto bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);
			uint32_t memberCount = uint32_t(bufferType.member_types.size());
			uint32_t bufferOffset = 0;
			if (m_ReflectionData.PushConstantRanges.size())
				bufferOffset = m_ReflectionData.PushConstantRanges.back().Offset + m_ReflectionData.PushConstantRanges.back().Size;

			auto& pushConstantRange = m_ReflectionData.PushConstantRanges.emplace_back();
			pushConstantRange.ShaderStage = shaderStage;
			pushConstantRange.Size = bufferSize - bufferOffset;
			pushConstantRange.Offset = bufferOffset;

			// Skip empty push constant buffers - these are for the renderer only
			if (bufferName.empty() || bufferName == "u_Renderer")
				continue;

			ShaderBuffer& buffer = m_ReflectionData.ConstantBuffers[bufferName];
			buffer.Name = bufferName;
			buffer.Size = bufferSize - bufferOffset;

			X2_CORE_TRACE_TAG("Renderer", "  Name: {0}", bufferName);
			X2_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
			X2_CORE_TRACE_TAG("Renderer", "  Size: {0}", bufferSize);

			for (uint32_t i = 0; i < memberCount; i++)
			{
				auto type = compiler.get_type(bufferType.member_types[i]);
				const auto& memberName = compiler.get_member_name(bufferType.self, i);
				auto size = (uint32_t)compiler.get_declared_struct_member_size(bufferType, i);
				auto offset = compiler.type_struct_member_offset(bufferType, i) - bufferOffset;

				std::string uniformName = fmt::format("{}.{}", bufferName, memberName);
				buffer.Uniforms[uniformName] = ShaderUniform(uniformName, Utils::SPIRTypeToShaderUniformType(type), size, offset);
			}
		}

		X2_CORE_TRACE_TAG("Renderer", "Sampled Images:");
		for (const auto& resource : resources.sampled_images)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = baseType.image.dim;
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.ImageSamplers[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = shaderStage;
			imageSampler.ArraySize = arraySize;

			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, binding, 1);

			X2_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
		}

		X2_CORE_TRACE_TAG("Renderer", "Separate Images:");
		for (const auto& resource : resources.separate_images)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = baseType.image.dim;
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.SeparateTextures[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = shaderStage;
			imageSampler.ArraySize = arraySize;

			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, binding, 1);

			X2_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
		}

		X2_CORE_TRACE_TAG("Renderer", "Separate Samplers:");
		for (const auto& resource : resources.separate_samplers)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.SeparateSamplers[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = shaderStage;
			imageSampler.ArraySize = arraySize;

			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, binding, 1);

			X2_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
		}

		X2_CORE_TRACE_TAG("Renderer", "Storage Images:");
		for (const auto& resource : resources.storage_images)
		{
			const auto& name = resource.name;
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = type.image.dim;
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.StorageImages[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ArraySize = arraySize;
			imageSampler.ShaderStage = shaderStage;

			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, binding, 1);

			X2_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
		}

		X2_CORE_TRACE_TAG("Renderer", "Special macros:");
		for (const auto& macro : m_AcknowledgedMacros)
		{
			X2_CORE_TRACE_TAG("Renderer", "  {0}", macro);
		}

		X2_CORE_TRACE_TAG("Renderer", "===========================");


	}


}
