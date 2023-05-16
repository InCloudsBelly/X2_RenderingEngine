#include "DescriptorSetManager.h"
#include "Core/Graphic/Instance/DescriptorSet.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
//#include "Utils/Log.h"

void DescriptorSetManager::addDescriptorSetPool(ShaderSlotType slotType, std::vector<VkDescriptorType> descriptorTypes, uint32_t chunkSize)
{
	//std::unique_lock<std::shared_mutex> lock(_managerMutex);

	m_pools[slotType] = new DescriptorSetChunkPool(slotType, descriptorTypes, chunkSize);
}

void DescriptorSetManager::deleteDescriptorSetPool(ShaderSlotType slotType)
{
	//std::unique_lock<std::shared_mutex> lock(_managerMutex);

	delete m_pools[slotType];
	m_pools.erase(slotType);
}

DescriptorSet* DescriptorSetManager::acquireDescripterSet(ShaderSlotType slotType, VkDescriptorSetLayout descriptorSetLayout)
{
	//std::shared_lock<std::shared_mutex> lock(_managerMutex);

	return m_pools[slotType]->acquireDescripterSet(descriptorSetLayout);
}

void DescriptorSetManager::releaseDescripterSet(DescriptorSet*& descriptorSet)
{
	//std::shared_lock<std::shared_mutex> lock(_managerMutex);

	m_pools[descriptorSet->m_slotType]->releaseDescripterSet(descriptorSet);
}

void DescriptorSetManager::collect()
{
	//std::unique_lock<std::shared_mutex> lock(_managerMutex);

	for (auto& poolPair : m_pools)
	{
		poolPair.second->collect();
	}
}

DescriptorSetManager::DescriptorSetManager()
	//: _managerMutex()
	: m_pools()
{
}

DescriptorSetManager::~DescriptorSetManager()
{
	//std::unique_lock<std::shared_mutex> lock(_managerMutex);
	for (auto& poolPair : m_pools)
	{
		delete poolPair.second;
	}
}

DescriptorSetManager::DescriptorSetChunkPool::DescriptorSetChunkPool(ShaderSlotType slotType, std::vector<VkDescriptorType>& types, uint32_t chunkSize)
	: slotType(slotType)
	//, mutex()
	, chunkSize(chunkSize)
	, vkDescriptorPoolSizes(populateVkDescriptorPoolSizes(types, chunkSize))
	, chunkCreateInfo(populateChunkCreateInfo(chunkSize, vkDescriptorPoolSizes))
	, chunks()
	, handles()
{
}

DescriptorSetManager::DescriptorSetChunkPool::~DescriptorSetChunkPool()
{
	//std::unique_lock<std::mutex> lock(mutex);

	for (const auto& handle : handles)
	{
		vkFreeDescriptorSets(Instance::getDevice(), handle->m_sourceVkDescriptorChunk, 1, &handle->m_descriptorSet);

		delete handle;
	}

	for (const auto& chunkPair : chunks)
	{
		vkResetDescriptorPool(Instance::getDevice(), chunkPair.first, 0);
		vkDestroyDescriptorPool(Instance::getDevice(), chunkPair.first, nullptr);
	}
}

DescriptorSet* DescriptorSetManager::DescriptorSetChunkPool::acquireDescripterSet(VkDescriptorSetLayout descriptorSetLayout)
{
	//std::unique_lock<std::mutex> lock(mutex);

	VkDescriptorPool useableChunk = VK_NULL_HANDLE;
	for (const auto& chunkPair : chunks)
	{
		if (chunkPair.second > 0)
		{
			useableChunk = chunkPair.first;
			break;
		}
	}
	if (useableChunk == VK_NULL_HANDLE)
	{
		if (vkCreateDescriptorPool(Instance::getDevice(), &chunkCreateInfo, nullptr, &useableChunk) != VK_SUCCESS)
			throw(std::runtime_error("Failed to create descriptor chunk."));
		chunks.emplace(useableChunk, chunkSize);
	}
	--chunks[useableChunk];

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = useableChunk;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet newVkSet = VK_NULL_HANDLE;
	if (vkAllocateDescriptorSets(Instance::getDevice(), &allocInfo, &newVkSet) != VK_SUCCESS)
		throw(std::runtime_error("Failed to allocate descriptor sets."));

	DescriptorSet* newSet = new DescriptorSet(slotType, newVkSet, useableChunk);

	handles.emplace(newSet);

	return newSet;
}

void DescriptorSetManager::DescriptorSetChunkPool::releaseDescripterSet(DescriptorSet*& descriptorSet)
{
	//std::unique_lock<std::mutex> lock(mutex);

	++chunks[descriptorSet->m_sourceVkDescriptorChunk];
	vkFreeDescriptorSets(Instance::getDevice(), descriptorSet->m_sourceVkDescriptorChunk, 1, &descriptorSet->m_descriptorSet);

	handles.erase(descriptorSet);

	delete descriptorSet;
	descriptorSet = nullptr;
}

void DescriptorSetManager::DescriptorSetChunkPool::collect()
{
	//std::unique_lock<std::mutex> lock(mutex);

	for (auto it = chunks.begin(); it != chunks.end(); )
	{
		if (it->second == chunkSize)
		{
			vkResetDescriptorPool(Instance::getDevice(), it->first, 0);
			vkDestroyDescriptorPool(Instance::getDevice(), it->first, nullptr);
			it = chunks.erase(it);
		}
		else
		{
			++it;
		}
	}
}

std::vector<VkDescriptorPoolSize> DescriptorSetManager::DescriptorSetChunkPool::populateVkDescriptorPoolSizes(std::vector<VkDescriptorType>& types, int chunkSize)
{
	std::map<VkDescriptorType, uint32_t> typeCounts = std::map<VkDescriptorType, uint32_t>();
	for (const auto& descriptorType : types)
	{
		if (!typeCounts.count(descriptorType))
		{
			typeCounts[descriptorType] = 0;
		}
		++typeCounts[descriptorType];
	}

	std::vector<VkDescriptorPoolSize> poolSizes = std::vector<VkDescriptorPoolSize>(typeCounts.size());
	size_t poolSizeIndex = 0;
	for (const auto& pair : typeCounts)
	{
		poolSizes[poolSizeIndex].type = pair.first;
		poolSizes[poolSizeIndex].descriptorCount = pair.second * chunkSize;

		poolSizeIndex++;
	}

	return poolSizes;
}

VkDescriptorPoolCreateInfo DescriptorSetManager::DescriptorSetChunkPool::populateChunkCreateInfo(uint32_t chunkSize, std::vector<VkDescriptorPoolSize> const& chunkSizes)
{
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(chunkSizes.size());
	poolInfo.pPoolSizes = chunkSizes.data();
	poolInfo.maxSets = chunkSize;
	return poolInfo;
}
