#include <vulkan/vulkan_core.h>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <set>

enum class ShaderSlotType;

class DescriptorSet;

class Instance;

class DescriptorSetManager
{
	friend class Instance;
	//friend class CoreObject::Thread;
private:
	struct DescriptorSetChunkPool
	{
		ShaderSlotType const slotType;
		uint32_t const chunkSize;
		std::vector<VkDescriptorPoolSize> const vkDescriptorPoolSizes;
		VkDescriptorPoolCreateInfo const chunkCreateInfo;

		//std::mutex mutex;

		std::map<VkDescriptorPool, uint32_t> chunks;

		std::set<DescriptorSet*> handles;

		DescriptorSetChunkPool(ShaderSlotType slotType, std::vector< VkDescriptorType>& types, uint32_t chunkSize);
		~DescriptorSetChunkPool();
		DescriptorSet* acquireDescripterSet(VkDescriptorSetLayout descriptorSetLayout);
		void releaseDescripterSet(DescriptorSet*& descriptorSet);
		void collect();

		static std::vector<VkDescriptorPoolSize> populateVkDescriptorPoolSizes(std::vector< VkDescriptorType>& types, int chunkSize);
		static VkDescriptorPoolCreateInfo populateChunkCreateInfo(uint32_t chunkSize, std::vector<VkDescriptorPoolSize> const& chunkSizes);
	};
	//std::shared_mutex _managerMutex;
	std::map<ShaderSlotType, DescriptorSetChunkPool*> m_pools;

	DescriptorSetManager();
	~DescriptorSetManager();
	DescriptorSetManager(const DescriptorSetManager&) = delete;
	DescriptorSetManager& operator=(const DescriptorSetManager&) = delete;
	DescriptorSetManager(DescriptorSetManager&&) = delete;
	DescriptorSetManager& operator=(DescriptorSetManager&&) = delete;
public:
	void addDescriptorSetPool(ShaderSlotType slotType, std::vector< VkDescriptorType> descriptorTypes, uint32_t chunkSize);
	void deleteDescriptorSetPool(ShaderSlotType slotType);
	DescriptorSet* acquireDescripterSet(ShaderSlotType slotType, VkDescriptorSetLayout descriptorSetLayout);
	void releaseDescripterSet(DescriptorSet*& descriptorSet);
	void collect();

};