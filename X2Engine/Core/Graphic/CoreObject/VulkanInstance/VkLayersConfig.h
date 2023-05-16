#pragma once

#include <vector>

namespace VkLayersConfig
{
#ifdef NDEBUG
    inline const bool VALIDATION_LAYERS_ENABLED = false;
#else
    inline const bool VALIDATION_LAYERS_ENABLED = true;
#endif

    const std::vector<const char*> VALIDATION_LAYERS = {
        // Contains all the standard validations.
        "VK_LAYER_KHRONOS_validation"
    };
};