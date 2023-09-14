#pragma once
#include "X2/Core/Ref.h"

#include "VulkanAllocator.h"

namespace X2 {

	class VulkanUniformBuffer 
	{
	public:
		VulkanUniformBuffer(uint32_t size, uint32_t binding);
		virtual ~VulkanUniformBuffer();

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) ;
		virtual void RT_SetData(const void* data, uint32_t size, uint32_t offset = 0) ;
		virtual uint32_t GetBinding() const  { return m_Binding; }

		const VkDescriptorBufferInfo& GetDescriptorBufferInfo() const { return m_DescriptorInfo; }
	private:
		void Release();
		void RT_Invalidate();
	private:
		VmaAllocation m_MemoryAlloc = nullptr;
		VkBuffer m_Buffer;
		VkDescriptorBufferInfo m_DescriptorInfo{};
		uint32_t m_Size = 0;
		uint32_t m_Binding = 0;
		std::string m_Name;
		VkShaderStageFlagBits m_ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		uint8_t* m_LocalStorage = nullptr;
	};
}
