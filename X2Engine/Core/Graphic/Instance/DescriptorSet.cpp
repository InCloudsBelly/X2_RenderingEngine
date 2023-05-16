#include "DescriptorSet.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"

VkDescriptorSet& DescriptorSet::get()
{
	return m_descriptorSet;
}

DescriptorSet::DescriptorSet(ShaderSlotType slotType, VkDescriptorSet descriptorSet, VkDescriptorPool sourceDescriptorChunk)
	: m_slotType(slotType)
	, m_descriptorSet(descriptorSet)
	, m_sourceVkDescriptorChunk(sourceDescriptorChunk)
{
}


DescriptorSet::DescriptorSetWriteData::DescriptorSetWriteData(uint32_t binding, VkDescriptorType type, VkBuffer& buffer, VkDeviceSize offset, VkDeviceSize range)
	: binding(binding)
	, type(type)
	, buffer(buffer)
	, offset(offset)
	, range(range)
{
}

DescriptorSet::DescriptorSetWriteData::DescriptorSetWriteData(uint32_t binding, VkDescriptorType type, VkBuffer& buffer, VkBufferView& bufferView, VkDeviceSize offset, VkDeviceSize range)
	: binding(binding)
	, type(type)
	, buffer(buffer)
	, offset(offset)
	, range(range)
	, bufferView(bufferView)
{
}

DescriptorSet::DescriptorSetWriteData::DescriptorSetWriteData(uint32_t binding, VkDescriptorType type, VkSampler& sampler, VkImageView& imageView, VkImageLayout imageLayout)
	: binding(binding)
	, type(type)
	, sampler(sampler)
	, imageView(imageView)
	, imageLayout(imageLayout)
{
}

DescriptorSet::DescriptorSetWriteData::DescriptorSetWriteData(uint32_t binding, VkDescriptorType type, VkImageView& imageView, VkImageLayout imageLayout)
	: binding(binding)
	, type(type)
	, sampler(VK_NULL_HANDLE)
	, imageView(imageView)
	, imageLayout(imageLayout)
{
}

void DescriptorSet::UpdateBindingData(std::vector<DescriptorSet::DescriptorSetWriteData> data)
{
	std::vector< VkDescriptorBufferInfo> bufferInfos = std::vector< VkDescriptorBufferInfo>(data.size());
	std::vector< VkDescriptorImageInfo> imageInfos = std::vector< VkDescriptorImageInfo>(data.size());
	std::vector< VkWriteDescriptorSet> writeInfos = std::vector< VkWriteDescriptorSet>(data.size());

	for (size_t i = 0; i < data.size(); i++)
	{
		bufferInfos[i] = {};
		writeInfos[i] = {};
		imageInfos[i] = {};
		if (data[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || data[i].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			bufferInfos[i].buffer = data[i].buffer;
			bufferInfos[i].offset = data[i].offset;
			bufferInfos[i].range = data[i].range;

			writeInfos[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfos[i].dstSet = m_descriptorSet;
			writeInfos[i].dstBinding = data[i].binding;
			writeInfos[i].dstArrayElement = 0;
			writeInfos[i].descriptorType = data[i].type;
			writeInfos[i].descriptorCount = 1;
			writeInfos[i].pBufferInfo = &bufferInfos[i];
		}
		else if (data[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || data[i].type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
		{
			bufferInfos[i].buffer = data[i].buffer;
			bufferInfos[i].offset = data[i].offset;
			bufferInfos[i].range = data[i].range;

			writeInfos[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfos[i].dstSet = m_descriptorSet;
			writeInfos[i].dstBinding = data[i].binding;
			writeInfos[i].dstArrayElement = 0;
			writeInfos[i].descriptorType = data[i].type;
			writeInfos[i].descriptorCount = 1;
			writeInfos[i].pBufferInfo = &bufferInfos[i];
			writeInfos[i].pTexelBufferView = &data[i].bufferView;
		}
		else if (data[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || data[i].type == VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || data[i].type == VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
		{
			imageInfos[i].imageLayout = data[i].imageLayout;
			imageInfos[i].imageView = data[i].imageView;
			imageInfos[i].sampler = data[i].sampler;

			writeInfos[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfos[i].dstSet = m_descriptorSet;
			writeInfos[i].dstBinding = data[i].binding;
			writeInfos[i].dstArrayElement = 0;
			writeInfos[i].descriptorType = data[i].type;
			writeInfos[i].descriptorCount = 1;
			writeInfos[i].pImageInfo = &imageInfos[i];
		}
	}
	vkUpdateDescriptorSets(Instance::getDevice(), static_cast<uint32_t>(writeInfos.size()), writeInfos.data(), 0, nullptr);
}
