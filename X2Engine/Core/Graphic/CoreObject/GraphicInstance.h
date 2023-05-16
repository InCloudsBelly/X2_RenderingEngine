
#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <mutex>
#include <map>
#include <string>
#include <vma/vk_mem_alloc.h>
//#include <qvulkanwindow.h>

class Component;

class FrameBuffer;
class Shader;
class Material;

class RenderPassManager;
class DescriptorSetManager;
class LightManager;
class InputManager;
class RenderPipelineManager;
class AssetManager;

class Image;
class ImageSampler;

class CommandPool;
class CommandBuffer;

class Window;
class VKinstance;
class Device;
class Swapchain;


class Instance;
struct Queue
{
	friend class Engine;
	friend class Swapchain;

	friend class Instance;
	uint32_t queueFamilyIndex;
	VkQueue queue;
	//std::mutex mutex;
	std::string name;
private:
	Queue(std::string name, uint32_t queueFamilyIndex, VkQueue queue);
};

class Instance final
{
	friend class Engine;
private:
	static std::map<std::string, Queue*> g_queues;

	static Window*			g_window;
	static VKinstance*		g_vkInstance;

	static VkPhysicalDevice g_physicalDevice;
	static VkDevice			g_device;
	static Swapchain*	g_swapchain;


	VkDebugUtilsMessengerEXT            m_debugMessenger;

	//static MemoryManager* g_memoryManager;
	static VmaAllocator* g_vmaAllocator;
	static RenderPassManager* g_renderPassManager;
	static DescriptorSetManager* g_descriptorSetManager;
	static LightManager* g_lightManager;
	static RenderPipelineManager* g_renderPipelineManager;
	static AssetManager* g_assetManager;

	static CommandPool* g_graphicCommandPool;
	static CommandPool* g_computeCommandPool;
	static CommandPool* g_transferCommandPool;
	
	static CommandBuffer* g_transferCommandBuffer;

	//static Utils::Condition* _startPresentCondition;
	//static Utils::Condition* _endPresentCondition;
	//static Utils::Condition* _startRenderCondition;
	//static Utils::Condition* _endRenderCondition;

	static std::vector<Component*> g_lights;
	static std::vector<Component*> g_cameras;
	static std::vector<Component*> g_renderers;

	Instance();
public:
	static Image* g_backgroundImage;
	static ImageSampler* g_defaultSampler;

public:
	static void Init();

	static void destroy();

	static Queue* getQueue(std::string name);
	//static QVulkanInstance* QVulkanInstance_();
	static VkInstance& getVkInstance();
	static VkPhysicalDevice& getPhysicalDevice();
	//static QVulkanDeviceFunctions* QVulkanDeviceFunctions_();
	static VkDevice& getDevice();
	static Window* getWindow();
	static Swapchain* getSwapchain();

	//static MemoryManager& getMemoryManager();
	static RenderPassManager& getRenderPassManager();
	static DescriptorSetManager& getDescriptorSetManager();
	static LightManager& getLightManager();
	static VmaAllocator& getVmaAllocator();
	static RenderPipelineManager* getRenderPipelineManager();
	static AssetManager* getAssetManager();

	static CommandPool* getGraphicCommandPool();
	static CommandPool* getComputeCommandPool();

	static CommandBuffer* getTransferCommandBuffer();

	/*static Utils::Condition& StartPresentCondition();
	static Utils::Condition& EndPresentCondition();
	static Utils::Condition& StartRenderCondition();
	static Utils::Condition& EndRenderCondition();*/

	static void addLight(std::vector<Component*>& lights);
	static void addCamera(std::vector<Component*>& cameras);
	static void addRenderer(std::vector<Component*>& renderers);
	static void clearLight();
	static void clearCamera();
	static void clearRenderer();
};