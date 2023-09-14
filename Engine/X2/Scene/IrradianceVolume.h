#ifndef X2_IRRADIANCEVOLUME_H
#define X2_IRRADIANCEVOLUME_H


#include "X2/Math/AABB.h"

#include "X2/Core/Buffer.h"
#include "X2/Vulkan/VulkanImage.h"

#include <glm/glm.hpp>

namespace X2 {

    namespace Lighting {

        class InternalIrradianceVolume {
        public:
            InternalIrradianceVolume() = default;

            InternalIrradianceVolume(glm::ivec2 irrRes, glm::ivec2 momRes, glm::ivec3 probeCount);

            void SetRayCount(uint32_t rayCount, uint32_t rayCountInactive);

            void SwapTextures();

            std::tuple<const VulkanImage2D&, const VulkanImage2D&>
                GetCurrentProbes() const;

            std::tuple<const VulkanImage2D&, const VulkanImage2D&>
                GetLastProbes() const;

            void ClearProbes(glm::ivec2 irrRes, glm::ivec2 momRes, glm::ivec3 probeCount);

            void ResetProbeOffsets();

            BufferSafe rayDirBuffer;
            BufferSafe rayDirInactiveBuffer;
            BufferSafe probeOffsetBuffer;
            BufferSafe probeStateBuffer;

        private:
            void FillRayBuffers();

            Ref<VulkanImage2D> irradianceArray0;
            Ref<VulkanImage2D> momentsArray0;

            Ref<VulkanImage2D> irradianceArray1;
            Ref<VulkanImage2D> momentsArray1;

            int32_t swapIdx = 0;

            uint32_t m_rayCount = 0;
            uint32_t m_rayCountInactive = 0;
            glm::ivec3 m_probeCount{ 0 };

        };

        class IrradianceVolume {

        public:
            IrradianceVolume() = default;

            IrradianceVolume(Volume::AABB aabb, glm::ivec3 probeCount, bool lowerResMoments = true);

            glm::ivec3 GetIrradianceArrayOffset(glm::ivec3 probeIndex);

            glm::ivec3 GetMomentsArrayOffset(glm::ivec3 probeIndex);

            glm::vec3 GetProbeLocation(glm::ivec3 probeIndex);

            void SetAABB(Volume::AABB aabb);

            void SetRayCount(uint32_t rayCount, uint32_t rayCountInactive);

            void SetProbeCount(glm::ivec3 probeCount);

            void ClearProbes();

            void ResetProbeOffsets();

            Volume::AABB aabb;
            glm::ivec3 probeCount;

            glm::vec3 size;
            glm::vec3 cellSize;

            const int32_t irrRes = 6;
            const int32_t momRes = 14;

            uint32_t rayCount = 100;
            uint32_t rayCountInactive = 32;

            float hysteresis = 0.98f;
            float bias = 0.3f;
            float sharpness = 50.0f;

            float gamma = 5.0f;

            float strength = 1.0f;

            bool enable = true;
            bool update = true;
            bool sampleEmissives = false;
            bool debug = false;
            bool optimizeProbes = true;
            bool useShadowMap = false;
            bool lowerResMoments = false;
            bool opacityCheck = false;

            InternalIrradianceVolume internal;

        };

    }

}

#endif