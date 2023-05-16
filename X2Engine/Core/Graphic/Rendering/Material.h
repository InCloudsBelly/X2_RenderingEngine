#pragma once
#include <map>
#include <string>
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string>

#include "Core/Graphic/Instance/DescriptorSet.h"
#include "Core/Logic/Object/Object.h"


class DescriptorSetManager;

class Buffer;
class Image;
class ImageSampler;
class DescriptorSet;

enum class ShaderSlotType;
class Shader;

class Material final : Object
{
private:
	struct Slot 
	{
		std::string name;
		uint32_t setIndex;
		Object* asset;
		ShaderSlotType slotType;
		DescriptorSet* descriptorSet;
	};

private:
	Shader* m_shader;
	std::map<std::string, Slot> m_slots;
	void destroy();
public:
	Material(Shader* shader);
	~Material();
	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;
	Material(Material&&) = delete;
	Material& operator=(Material&&) = delete;

	void setSlotData(std::string name, std::vector<DescriptorSet::DescriptorSetWriteData> data);
	Image* getSampledImageCube(std::string name);
	void setSampledImageCube(std::string slotName, Image* imageCube, ImageSampler* sampler, std::string imageViewName = "DefaultImageView");
	Image* getSampledImage2D(std::string name);
	void setSampledImage2D(std::string slotName, Image* image2D, ImageSampler* sampler, std::string imageViewName = "DefaultImageView");
	Image* getStorageImage2D(std::string name);
	void setStorageImage2D(std::string slotName, Image* image2D, std::string imageViewName = "DefaultImageView");

	Image* getInputAttachment(std::string name);
	void setInputAttachment(std::string slotName, Image* image2D, std::string imageViewName = "DefaultImageView");
	
	Buffer* getUniformBuffer(std::string name);
	void setUniformBuffer(std::string name, Buffer* buffer);
	Buffer* getStorageBuffer(std::string name);
	void setStorageBuffer(std::string name, Buffer* buffer);
	Buffer* getUniformTexelBuffer(std::string name);
	void setUniformTexelBuffer(std::string name, Buffer* buffer);
	Buffer* getStorgeTexelBuffer(std::string name);
	void setStorgeTexelBuffer(std::string name, Buffer* buffer);
	
	
	VkPipelineLayout& getPipelineLayout();
	std::vector<VkDescriptorSet> getDescriptorSets();
	Shader* getShader();

};
		