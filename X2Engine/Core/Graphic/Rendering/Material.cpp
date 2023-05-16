#include "Material.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Instance/DescriptorSet.h"
#include "Core/Graphic/Instance/Image.h"

#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Rendering/Shader.h"
#include "Core/Graphic/Manager/DescriptorSetManager.h"
#include "Core/IO/Manager/AssetManager.h"

//#include "Utils/Log.h"

Buffer* Material::getUniformBuffer(std::string name)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::UNIFORM_BUFFER)
	{
		return static_cast<Buffer*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get uniform buffer."));
		return nullptr;
	}
}

void Material::setUniformBuffer(std::string name, Buffer* buffer)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::UNIFORM_BUFFER)
	{
		m_slots[name].asset = buffer;
		m_slots[name].descriptorSet->UpdateBindingData(
			{
				{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer->getBuffer(), buffer->Offset(), buffer->Size()}
			}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set uniform buffer."));
	}
}

Buffer* Material::getStorageBuffer(std::string name)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::STORAGE_BUFFER)
	{
		return static_cast<Buffer*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get storage buffer."));
		return nullptr;
	}
}

void Material::setStorageBuffer(std::string name, Buffer* buffer)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::STORAGE_BUFFER)
	{
		m_slots[name].asset = buffer;
		m_slots[name].descriptorSet->UpdateBindingData(
			{
				{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer->getBuffer(), buffer->Offset(), buffer->Size()}
			}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set storage buffer."));
	}
}

Buffer* Material::getUniformTexelBuffer(std::string name)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::UNIFORM_TEXEL_BUFFER)
	{
		return static_cast<Buffer*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get uniform texel buffer."));
		return nullptr;
	}
}

void Material::setUniformTexelBuffer(std::string name, Buffer* buffer)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::UNIFORM_TEXEL_BUFFER)
	{
		m_slots[name].asset = buffer;
		m_slots[name].descriptorSet->UpdateBindingData(
			{
				{0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, buffer->getBuffer(), buffer->getBufferView(), buffer->Offset(), buffer->Size()}
			}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set uniform texel buffer."));
	}
}

Buffer* Material::getStorgeTexelBuffer(std::string name)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::STORAGE_TEXEL_BUFFER)
	{
		return static_cast<Buffer*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get storage texel buffer."));
		return nullptr;
	}
}

void Material::setStorgeTexelBuffer(std::string name, Buffer* buffer)
{
	if (m_slots.count(name) && m_slots[name].slotType == ShaderSlotType::STORAGE_TEXEL_BUFFER)
	{
		m_slots[name].asset = buffer;
		m_slots[name].descriptorSet->UpdateBindingData(
			{
				{0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, buffer->getBuffer(), buffer->getBufferView(), buffer->Offset(), buffer->Size()}
			}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set storage texel buffer."));
	}
}

VkPipelineLayout& Material::getPipelineLayout()
{
	return this->m_shader->getPipelineLayout();
}

std::vector<VkDescriptorSet> Material::getDescriptorSets()
{
	std::vector<VkDescriptorSet> sets = std::vector<VkDescriptorSet>(m_slots.size());

	for (const auto& slotPair : m_slots)
	{
		sets[slotPair.second.setIndex] = slotPair.second.descriptorSet->get();
	}
	return sets;
}

Shader* Material::getShader()
{
	return m_shader;
}

void Material::setSlotData(std::string name, std::vector<DescriptorSet::DescriptorSetWriteData> data)
{
	m_slots[name].asset = nullptr;
	m_slots[name].descriptorSet->UpdateBindingData( data);
}

Image*Material::getSampledImageCube(std::string name)
{
	if (m_slots.count(name) && (m_slots[name].slotType == ShaderSlotType::TEXTURE_CUBE))
	{
		return static_cast<Image*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get textureCube."));
		return nullptr;
	}
}
void Material::setSampledImageCube(std::string slotName, Image* imageCube, ImageSampler* sampler, std::string imageViewName)
{
	if (m_slots.count(slotName) && m_slots[slotName].slotType == ShaderSlotType::TEXTURE_CUBE)
	{
		m_slots[slotName].asset = imageCube;
		m_slots[slotName].descriptorSet->UpdateBindingData(
			{
				{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler->getSampler(), imageCube->getImageView(imageViewName), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
			}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set textureCube."));
	}
}
Image* Material::getSampledImage2D(std::string name)
{
	if (m_slots.count(name) && (m_slots[name].slotType == ShaderSlotType::TEXTURE2D))
	{
		return static_cast<Image*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get texture2D."));
		return nullptr;
	}
}
void Material::setSampledImage2D(std::string slotName, Image* image2D, ImageSampler* sampler, std::string imageViewName)
{
	if (m_slots.count(slotName) && m_slots[slotName].slotType == ShaderSlotType::TEXTURE2D)
	{
		m_slots[slotName].asset = image2D;
		m_slots[slotName].descriptorSet->UpdateBindingData(
			{
				{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler->getSampler(), image2D->getImageView(imageViewName), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
			}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set texture2D."));
	}
}
Image* Material::getStorageImage2D(std::string name)
{
	if (m_slots.count(name) && (m_slots[name].slotType == ShaderSlotType::STORAGE_TEXTURE2D))
	{
		return static_cast<Image*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get storage texture2D."));
		return nullptr;
	}
}
void Material::setStorageImage2D(std::string slotName, Image* image2D, std::string imageViewName)
{
	if (m_slots.count(slotName) && m_slots[slotName].slotType == ShaderSlotType::STORAGE_TEXTURE2D)
	{
		m_slots[slotName].asset = image2D;
		m_slots[slotName].descriptorSet->UpdateBindingData(
				{
					{0, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image2D->getImageView(imageViewName), VkImageLayout::VK_IMAGE_LAYOUT_GENERAL}
				}
			);
	}
	else
	{
		throw(std::runtime_error("Failed to set storage texture2D."));
	}
}


Image* Material::getInputAttachment(std::string name)
{
	if (m_slots.count(name) && (m_slots[name].slotType == ShaderSlotType::INPUT_ATTACHMENT))
	{
		return static_cast<Image*>(m_slots[name].asset);
	}
	else
	{
		throw(std::runtime_error("Failed to get storage texture2D."));
		return nullptr;
	}
}

void Material::setInputAttachment(std::string slotName, Image* image2D, std::string imageViewName)
{
	if (m_slots.count(slotName) && m_slots[slotName].slotType == ShaderSlotType::INPUT_ATTACHMENT)
	{
		m_slots[slotName].asset = image2D;
		m_slots[slotName].descriptorSet->UpdateBindingData(
			{
				{0, VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, image2D->getImageView(imageViewName), VkImageLayout::VK_IMAGE_LAYOUT_GENERAL}
			}
		);
	}
	else
	{
		throw(std::runtime_error("Failed to set storage texture2D."));
	}
}

void Material::destroy()
{
}

Material::Material(Shader* shader)
	: m_shader(shader)
	, m_slots()
{
	for (const auto& pair : *shader->getSlotDescriptors())
	{
		Slot newSlot = Slot();
		newSlot.asset = nullptr;
		newSlot.name = pair.second.name;
		newSlot.slotType = pair.second.slotType;
		newSlot.descriptorSet = Instance::getDescriptorSetManager().acquireDescripterSet(pair.second.slotType, pair.second.vkDescriptorSetLayout);
		newSlot.setIndex = pair.second.setIndex;
		m_slots.emplace(newSlot.name, newSlot);
	}
}
Material::~Material()
{
	Instance::getAssetManager()->unload(m_shader);
}
