#pragma once
#include "Bitmap.h"

namespace SphericalHarmonicsUtils
{
    std::vector<float> Basis(const glm::vec3& pos);
    std::vector<glm::vec3> computeSkyboxSH(Bitmap& cubemap);
}