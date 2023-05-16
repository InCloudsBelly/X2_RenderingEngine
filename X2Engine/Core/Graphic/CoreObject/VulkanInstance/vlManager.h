#pragma once

#include <vulkan/vulkan.h>

namespace vlManager
{
    void createDebugMessenger(
        VkInstance& instance,
        VkDebugUtilsMessengerEXT& debugMessenger
    );

    bool AllRequestedLayersAvailable();
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo();
    //--------------------------------Proxies-----------------------------------
    VkResult createDebugUtilsMessengerEXT(
        VkInstance& instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    );

    void destroyDebugUtilsMessengerEXT(
        VkInstance& instance,
        VkDebugUtilsMessengerEXT& debugMessenger,
        const VkAllocationCallbacks* pAllocator
    );

};