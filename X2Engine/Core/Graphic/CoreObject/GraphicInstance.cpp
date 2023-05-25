#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Logic/CoreObject/LogicInstance.h"
//#include "Core/Manager/MemoryManager.h"
//#include "Core/CoreObject/Window.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Manager/DescriptorSetManager.h"
//#include "Utils/Condition.h"
#include "Core/Graphic/Manager/LightManager.h"
#include "Core/Graphic/Manager/RenderPipelineManager.h"
#include "Core/Logic/Manager/InputManager.h"
#include "Core/IO/Manager/AssetManager.h"

#include "Core/Graphic/Command/CommandPool.h"

#include "Core/Graphic/CoreObject/Window.h"

#include "Core/Graphic/CoreObject/VulkanInstance/VKinstance.h"
#include "Core/Graphic/CoreObject/VulkanInstance/VkLayersConfig.h"

#include "Core/Graphic/CoreObject/Swapchain.h"

#include "Core/Graphic/Instance/Image.h"


std::map<std::string, Queue*> Instance::g_queues = std::map<std::string, Queue*>();
//QVulkanInstance* _qVulkanInstance = nullptr;
VKinstance* Instance::g_vkInstance = nullptr;


VkPhysicalDevice Instance::g_physicalDevice = VK_NULL_HANDLE;
VkDevice Instance::g_device = VK_NULL_HANDLE;
Swapchain* Instance::g_swapchain = nullptr;

VmaAllocator* Instance::g_vmaAllocator = nullptr;

RenderPassManager* Instance::g_renderPassManager = nullptr;
DescriptorSetManager* Instance::g_descriptorSetManager = nullptr;
LightManager* Instance::g_lightManager = nullptr;
RenderPipelineManager* Instance::g_renderPipelineManager = nullptr;

AssetManager* Instance::g_assetManager = nullptr;

CommandPool* Instance::g_graphicCommandPool = nullptr;
CommandPool* Instance::g_computeCommandPool = nullptr;

CommandPool* Instance::g_transferCommandPool = nullptr;
CommandBuffer* Instance::g_transferCommandBuffer = nullptr;

Window* Instance::g_window = nullptr;

Image* Instance::g_backgroundImage = nullptr;
ImageSampler* Instance::g_defaultSampler = nullptr;

//
//Condition* _startPresentCondition = nullptr;
//Condition* _endPresentCondition = nullptr;
//Condition* _startRenderCondition = nullptr;
//Condition* _endRenderCondition = nullptr;

std::vector<Component*> Instance::g_lights = std::vector<Component*>();
std::vector<Component*> Instance::g_cameras = std::vector<Component*>();
std::vector<Component*> Instance::g_renderers = std::vector<Component*>();


Queue* Instance::getQueue(std::string name)
{
    return g_queues[name];
}

//QVulkanInstance* QVulkanInstance_()
//{
//    return _qVulkanInstance;
//}

VkInstance& Instance::getVkInstance()
{
    return g_vkInstance->get();
}

VkPhysicalDevice& Instance::getPhysicalDevice()
{
    return g_physicalDevice;
}


VkDevice& Instance::getDevice()
{
    return g_device;
}

Window* Instance::getWindow()
{
    return g_window;
}

Swapchain* Instance::getSwapchain()
{
    return g_swapchain;
}


RenderPassManager& Instance::getRenderPassManager()
{
	return *g_renderPassManager;
}

DescriptorSetManager& Instance::getDescriptorSetManager()
{
	return *g_descriptorSetManager;
}

LightManager& Instance::getLightManager()
{
	return *g_lightManager;
}

AssetManager* Instance::getAssetManager()
{
    return g_assetManager;
}

VmaAllocator& Instance::getVmaAllocator()
{
	return *g_vmaAllocator;
}

RenderPipelineManager* Instance::getRenderPipelineManager()
{
	return g_renderPipelineManager;
}

CommandPool* Instance::getGraphicCommandPool()
{
    return g_graphicCommandPool;
}

CommandPool* Instance::getComputeCommandPool()
{
    return g_computeCommandPool;
}

CommandBuffer* Instance::getTransferCommandBuffer()
{
    return g_transferCommandBuffer;
}

//Condition& StartPresentCondition()
//{
//	return *_startPresentCondition;
//}
//
//Condition& EndPresentCondition()
//{
//	return *_endPresentCondition;
//}
//
//Condition& StartRenderCondition()
//{
//	return *_startRenderCondition;
//}
//
//Condition& EndRenderCondition()
//{
//	return *_endRenderCondition;
//}

void Instance::addLight(std::vector<Component*>& lights)
{
	g_lights.insert(g_lights.end(), lights.begin(), lights.end());
}

void Instance::addCamera(std::vector<Component*>& cameras)
{
	g_cameras.insert(g_cameras.end(), cameras.begin(), cameras.end());
}

void Instance::addRenderer(std::vector<Component*>& renderers)
{
	g_renderers.insert(g_renderers.end(), renderers.begin(), renderers.end());
}

void Instance::clearLight()
{
	g_lights.clear();
}

void Instance::clearCamera()
{
	g_cameras.clear();
}

void Instance::clearRenderer()
{
	g_renderers.clear();
}

Instance::Instance()
{

}

void createLogicalDevice(std::vector<uint32_t> requiredQueueFamilyIndices, void* pNextChain, VkDevice* device);

void Instance::Init()
{
	//_qVulkanInstance = Window::_window->vulkanInstance();
    g_window = new Window(1920, 1080, "X2");

	g_vkInstance = new VKinstance("X2");
	g_window->createSurface(g_vkInstance->get());
	
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(g_vkInstance->get(), &deviceCount, nullptr);
        if (deviceCount == 0)
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(g_vkInstance->get(), &deviceCount, devices.data());

        g_physicalDevice = devices[0];
    }

    {
        VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
        enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
        createLogicalDevice({ 0 }, &enabledBufferDeviceAddresFeatures, &g_device);
    }

    g_swapchain = new Swapchain();



    
	//_qDeviceFunctions = Window::_qVulkanInstance->deviceFunctions(g_device);

	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(g_device, 0, 0, &queue);
	g_queues["PresentQueue"] = new Queue("PresentQueue", 0, queue);

	vkGetDeviceQueue(g_device, 0, 1, &queue);
	g_queues["GraphicQueue"] = new Queue("GraphicQueue", 0, queue);

	vkGetDeviceQueue(g_device, 0, 2, &queue);
	g_queues["TransferQueue"] = new Queue("TransferQueue", 0, queue);

	vkGetDeviceQueue(g_device, 0, 3, &queue);
	g_queues["ComputeQueue"] = new Queue("ComputeQueue", 0, queue);


    g_graphicCommandPool = new CommandPool("GraphicQueue", VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    g_computeCommandPool = new CommandPool("ComputeQueue", VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);



    g_transferCommandPool = new CommandPool("TransferQueue", VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    g_transferCommandBuffer = g_transferCommandPool->createCommandBuffer(VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
   
    //_memoryManager = new MemoryManager(32 * 1024 * 1024);

    g_vmaAllocator = new VmaAllocator;
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = g_physicalDevice;
	allocatorInfo.device = g_device;
	allocatorInfo.instance = g_vkInstance->get();
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, g_vmaAllocator);


	g_renderPassManager = new RenderPassManager();
	g_descriptorSetManager = new DescriptorSetManager();
	g_lightManager = new LightManager();
	g_renderPipelineManager = new RenderPipelineManager();

    g_assetManager = new AssetManager();

    LogicInstance::getInputManager()->init(g_window->get());

    g_defaultSampler = new ImageSampler(
        VkFilter::VK_FILTER_LINEAR,
        VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK
    );

	//_startPresentCondition = new Utils::Condition();
	//_endPresentCondition = new Utils::Condition();
	//_startRenderCondition = new Utils::Condition();
	//_endRenderCondition = new Utils::Condition();


}

void Instance::destroy()
{
    delete g_defaultSampler;
    if (g_backgroundImage)
        Instance::getAssetManager()->unload(g_backgroundImage);

    delete g_swapchain;

    delete g_renderPipelineManager;
    delete g_lightManager;


    g_assetManager->collect();
    g_descriptorSetManager->collect();
    g_renderPassManager->collect();

    delete g_assetManager;
    delete g_descriptorSetManager;
    delete g_renderPassManager;
    
    Instance::g_transferCommandPool->destoryCommandBuffer(g_transferCommandBuffer);
    delete g_transferCommandPool;
    delete g_computeCommandPool;
    delete g_graphicCommandPool;


    vmaDestroyAllocator(*g_vmaAllocator);

    vkDestroyDevice(g_device, nullptr);

    g_window->destroySurface(g_vkInstance->get());
    g_vkInstance->destroy();
    g_window->destroy();
}

Queue::Queue(std::string name, uint32_t queueFamilyIndex, VkQueue queue)
    : name(name)
    , queueFamilyIndex(queueFamilyIndex)
    , queue(queue)
    //, mutex()
{

}


void createLogicalDevice(std::vector<uint32_t> requiredQueueFamilyIndices, void* pNextChain, VkDevice* device)
{
    const std::vector<const char*> m_requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME ,VK_KHR_DEVICE_GROUP_EXTENSION_NAME };

    // - Specifies which QUEUES we want to create.
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // We use a set because we need to not repeat them to create the new one
    // for the logical device.
    std::set<uint32_t> uniqueQueueFamilies = { 0 };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};

        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 4;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    // - Specifices which device FEATURES we want to use.
    // For now, this will be empty.
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.textureCompressionBC = VK_TRUE;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    deviceFeatures.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
    deviceFeatures.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
    deviceFeatures.shaderStorageImageArrayDynamicIndexing = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT ext = {};
    ext.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

    // Now we can create the logical device.
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &ext;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
    if (pNextChain)
    {
        physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physicalDeviceFeatures2.features = deviceFeatures;
        physicalDeviceFeatures2.pNext = pNextChain;
        createInfo.pEnabledFeatures = nullptr;
        createInfo.pNext = &physicalDeviceFeatures2;
    }
    else
    {
        createInfo.pEnabledFeatures = &deviceFeatures;
    }

    // - Specifies which device EXTENSIONS we want to use.
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_requiredExtensions.size());

    createInfo.ppEnabledExtensionNames = m_requiredExtensions.data();

    // Previous implementations of Vulkan made a distinction between instance 
    // and device specific validation layers, but this is no longer the 
    // case. That means that the enabledLayerCount and ppEnabledLayerNames 
    // fields of VkDeviceCreateInfo are ignored by up-to-date 
    // implementations. However, it is still a good idea to set them anyway to 
    // be compatible with older implementations:

    if (VkLayersConfig::VALIDATION_LAYERS_ENABLED)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VkLayersConfig::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = (VkLayersConfig::VALIDATION_LAYERS.data());
    }
    else
        createInfo.enabledLayerCount = 0;

    auto status = vkCreateDevice(Instance::getPhysicalDevice(), &createInfo, nullptr, device);

    if (status != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device!");
}